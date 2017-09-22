/*
 * Author: gaosiyu@youku.com
 */

#include "rtcp_extern.h"
#include <iostream>
using namespace std;
using namespace avformat;
#define LOG(sev) cout

/************************************************************************/
/* parser                                                               */
/************************************************************************/

bool
avformat::RTCPParseCommonHeader(const uint8_t* ptrDataBegin,
const uint8_t* ptrDataEnd,
RTCPCommonHeader& parsedHeader)
{
    if (!ptrDataBegin || !ptrDataEnd)
    {
        return false;
    }

    //  0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |V=2|P|    IC   |      PT       |             length            |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //
    // Common header for all RTCP packets, 4 octets.

    if ((ptrDataEnd - ptrDataBegin) < 4)
    {
        return false;
    }

    parsedHeader.V = ptrDataBegin[0] >> 6;
    parsedHeader.P = ((ptrDataBegin[0] & 0x20) == 0) ? false : true;
    parsedHeader.IC = ptrDataBegin[0] & 0x1f;
    parsedHeader.PT = ptrDataBegin[1];

    parsedHeader.LengthInOctets = (ptrDataBegin[2] << 8) + ptrDataBegin[3] + 1;
    parsedHeader.LengthInOctets *= 4;

    if (parsedHeader.LengthInOctets == 0)
    {
        return false;
    }
    // Check if RTP version field == 2
    if (parsedHeader.V != 2)
    {
        return false;
    }

    return true;
}

// RTCPParserV2 : currently read only
RTCPParserV2::RTCPParserV2(const uint8_t* rtcpData,
    size_t rtcpDataLength,
    bool rtcpReducedSizeEnable)
    : _ptrRTCPDataBegin(rtcpData),
    _RTCPReducedSizeEnable(rtcpReducedSizeEnable),
    _ptrRTCPDataEnd(rtcpData + rtcpDataLength),
    _validPacket(false),
    _ptrRTCPData(rtcpData),
    _ptrRTCPBlockEnd(NULL),
    _state(State_TopLevel),
    _numberOfBlocks(0),
    _packetType(kInvalid) {
    Validate();
}

RTCPParserV2::~RTCPParserV2() {
}

ptrdiff_t
RTCPParserV2::LengthLeft() const
{
    return (_ptrRTCPDataEnd - _ptrRTCPData);
}

RTCPPacketTypes
RTCPParserV2::PacketType() const
{
    return _packetType;
}

const RTCPPacket&
RTCPParserV2::Packet() const
{
    return _packet;
}

RTCPPacketTypes
RTCPParserV2::Begin()
{
    _ptrRTCPData = _ptrRTCPDataBegin;

    return Iterate();
}

RTCPPacketTypes
RTCPParserV2::Iterate()
{
    // Reset packet type
    _packetType = kInvalid;

    if (IsValid())
    {
        switch (_state)
        {
        case State_TopLevel:
            IterateTopLevel();
            break;
        case State_ReportBlockItem:
            IterateReportBlockItem();
            break;
        case State_SDESChunk:
            IterateSDESChunk();
            break;
        case State_BYEItem:
            IterateBYEItem();
            break;
        case State_ExtendedJitterItem:
            IterateExtendedJitterItem();
            break;
        case State_RTPFB_NACKItem:
            IterateNACKItem();
            break;
        case State_RTPFB_TMMBRItem:
            IterateTMMBRItem();
            break;
        case State_RTPFB_TMMBNItem:
            IterateTMMBNItem();
            break;
#ifdef SLICE_FCI
        case State_PSFB_SLIItem:
            IterateSLIItem();
            break;
        case State_PSFB_RPSIItem:
            IterateRPSIItem();
            break;
        case State_PSFB_FIRItem:
            IterateFIRItem();
            break;
        case State_PSFB_AppItem:
            IteratePsfbAppItem();
            break;
        case State_PSFB_REMBItem:
            IteratePsfbREMBItem();
            break;
#endif

        case State_XRItem:
            IterateXrItem();
            break;
        case State_XR_DLLRItem:
            IterateXrDlrrItem();
            break;
        case State_AppItem:
            IterateAppItem();
            break;
        default:
            assert(false); // Invalid state!
            break;
        }
    }
    return _packetType;
}

void
RTCPParserV2::IterateTopLevel()
{
    for (;;)
    {
        RTCPCommonHeader header;

        const bool success = RTCPParseCommonHeader(_ptrRTCPData,
            _ptrRTCPDataEnd,
            header);

        if (!success)
        {
            return;
        }
        _ptrRTCPBlockEnd = _ptrRTCPData + header.LengthInOctets;
        if (_ptrRTCPBlockEnd > _ptrRTCPDataEnd)
        {
            // Bad block!
            return;
        }

        switch (header.PT)
        {
        case PT_SR:
        {
                      // number of Report blocks
                      _numberOfBlocks = header.IC;
                      ParseSR();
                      return;
        }
        case PT_RR:
        {
                      // number of Report blocks
                      _numberOfBlocks = header.IC;
                      ParseRR();
                      return;
        }
        case PT_SDES:
        {
                        // number of SDES blocks
                        _numberOfBlocks = header.IC;
                        const bool ok = ParseSDES();
                        if (!ok)
                        {
                            // Nothing supported found, continue to next block!
                            break;
                        }
                        return;
        }
        case PT_BYE:
        {
                       _numberOfBlocks = header.IC;
                       const bool ok = ParseBYE();
                       if (!ok)
                       {
                           // Nothing supported found, continue to next block!
                           break;
                       }
                       return;
        }
        case PT_IJ:
        {
                      // number of Report blocks
                      _numberOfBlocks = header.IC;
                      ParseIJ();
                      return;
        }
        case PT_RTPFB: // Fall through!
        case PT_PSFB:
        {
                        const bool ok = ParseFBCommon(header);
                        if (!ok)
                        {
                            // Nothing supported found, continue to next block!
                            break;
                        }
                        return;
        }
        case PT_APP:
        {
                       const bool ok = ParseAPP(header);
                       if (!ok)
                       {
                           // Nothing supported found, continue to next block!
                           break;
                       }
                       return;
        }
        case PT_XR:
        {
                      const bool ok = ParseXr();
                      if (!ok)
                      {
                          // Nothing supported found, continue to next block!
                          break;
                      }
                      return;
        }
        default:
            // Not supported! Skip!
            EndCurrentBlock();
            break;
        }
    }
}

void
RTCPParserV2::IterateXrItem()
{
    const bool success = ParseXrItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IterateXrDlrrItem()
{
    const bool success = ParseXrDlrrItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IterateReportBlockItem()
{
    const bool success = ParseReportBlockItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IterateSDESChunk()
{
    const bool success = ParseSDESChunk();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IterateBYEItem()
{
    const bool success = ParseBYEItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IterateExtendedJitterItem()
{
    const bool success = ParseIJItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IterateNACKItem()
{
    const bool success = ParseNACKItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IterateTMMBRItem()
{
    const bool success = ParseTMMBRItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IterateTMMBNItem()
{
    const bool success = ParseTMMBNItem();
    if (!success)
    {
        Iterate();
    }
}



void
RTCPParserV2::IterateAppItem()
{
    const bool success = ParseAPPItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::Validate()
{
    if (_ptrRTCPData == NULL)
    {
        return; // NOT VALID
    }

    RTCPCommonHeader header;
    const bool success = RTCPParseCommonHeader(_ptrRTCPDataBegin,
        _ptrRTCPDataEnd,
        header);

    if (!success)
    {
        return; // NOT VALID!
    }

    // * if (!reducedSize) : first packet must be RR or SR.
    //
    // * The padding bit (P) should be zero for the first packet of a
    //   compound RTCP packet because padding should only be applied,
    //   if it is needed, to the last packet. (NOT CHECKED!)
    //
    // * The length fields of the individual RTCP packets must add up
    //   to the overall length of the compound RTCP packet as
    //   received.  This is a fairly strong check. (NOT CHECKED!)

    if (!_RTCPReducedSizeEnable)
    {
        if ((header.PT != PT_SR) && (header.PT != PT_RR))
        {
            return; // NOT VALID
        }
    }

    _validPacket = true;
}

bool
RTCPParserV2::IsValid() const
{
    return _validPacket;
}

void
RTCPParserV2::EndCurrentBlock()
{
    _ptrRTCPData = _ptrRTCPBlockEnd;
}



bool
RTCPParserV2::ParseRR()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        return false;
    }


    _ptrRTCPData += 4; // Skip header

    _packetType = kRr;

    _packet.RR.SenderSSRC = *_ptrRTCPData++ << 24;
    _packet.RR.SenderSSRC += *_ptrRTCPData++ << 16;
    _packet.RR.SenderSSRC += *_ptrRTCPData++ << 8;
    _packet.RR.SenderSSRC += *_ptrRTCPData++;

    _packet.RR.NumberOfReportBlocks = _numberOfBlocks;

    // State transition
    _state = State_ReportBlockItem;

    return true;
}

bool
RTCPParserV2::ParseSR()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 28)
    {
        EndCurrentBlock();
        return false;
    }

    _ptrRTCPData += 4; // Skip header

    _packetType = kSr;

    _packet.SR.SenderSSRC = *_ptrRTCPData++ << 24;
    _packet.SR.SenderSSRC += *_ptrRTCPData++ << 16;
    _packet.SR.SenderSSRC += *_ptrRTCPData++ << 8;
    _packet.SR.SenderSSRC += *_ptrRTCPData++;

    _packet.SR.NTPMostSignificant = *_ptrRTCPData++ << 24;
    _packet.SR.NTPMostSignificant += *_ptrRTCPData++ << 16;
    _packet.SR.NTPMostSignificant += *_ptrRTCPData++ << 8;
    _packet.SR.NTPMostSignificant += *_ptrRTCPData++;

    _packet.SR.NTPLeastSignificant = *_ptrRTCPData++ << 24;
    _packet.SR.NTPLeastSignificant += *_ptrRTCPData++ << 16;
    _packet.SR.NTPLeastSignificant += *_ptrRTCPData++ << 8;
    _packet.SR.NTPLeastSignificant += *_ptrRTCPData++;

    _packet.SR.RTPTimestamp = *_ptrRTCPData++ << 24;
    _packet.SR.RTPTimestamp += *_ptrRTCPData++ << 16;
    _packet.SR.RTPTimestamp += *_ptrRTCPData++ << 8;
    _packet.SR.RTPTimestamp += *_ptrRTCPData++;

    _packet.SR.SenderPacketCount = *_ptrRTCPData++ << 24;
    _packet.SR.SenderPacketCount += *_ptrRTCPData++ << 16;
    _packet.SR.SenderPacketCount += *_ptrRTCPData++ << 8;
    _packet.SR.SenderPacketCount += *_ptrRTCPData++;

    _packet.SR.SenderOctetCount = *_ptrRTCPData++ << 24;
    _packet.SR.SenderOctetCount += *_ptrRTCPData++ << 16;
    _packet.SR.SenderOctetCount += *_ptrRTCPData++ << 8;
    _packet.SR.SenderOctetCount += *_ptrRTCPData++;

    _packet.SR.NumberOfReportBlocks = _numberOfBlocks;

    // State transition
    if (_numberOfBlocks != 0)
    {
        _state = State_ReportBlockItem;
    }
    else
    {
        // don't go to state report block item if 0 report blocks
        _state = State_TopLevel;
        EndCurrentBlock();
    }
    return true;
}

bool
RTCPParserV2::ParseReportBlockItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 24 || _numberOfBlocks <= 0)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packet.ReportBlockItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.SSRC += *_ptrRTCPData++;

    _packet.ReportBlockItem.FractionLost = *_ptrRTCPData++;

    _packet.ReportBlockItem.CumulativeNumOfPacketsLost = *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.CumulativeNumOfPacketsLost += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.CumulativeNumOfPacketsLost += *_ptrRTCPData++;

    _packet.ReportBlockItem.ExtendedHighestSequenceNumber = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.ExtendedHighestSequenceNumber += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.ExtendedHighestSequenceNumber += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.ExtendedHighestSequenceNumber += *_ptrRTCPData++;

    _packet.ReportBlockItem.Jitter = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.Jitter += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.Jitter += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.Jitter += *_ptrRTCPData++;

    _packet.ReportBlockItem.LastSR = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.LastSR += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.LastSR += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.LastSR += *_ptrRTCPData++;

    _packet.ReportBlockItem.DelayLastSR = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.DelayLastSR += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.DelayLastSR += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.DelayLastSR += *_ptrRTCPData++;

    _numberOfBlocks--;
    _packetType = kReportBlockItem;
    return true;
}

/* From RFC 5450: Transmission Time Offsets in RTP Streams.
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
hdr |V=2|P|    RC   |   PT=IJ=195   |             length            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      inter-arrival jitter                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
.                                                               .
.                                                               .
.                                                               .
|                      inter-arrival jitter                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

bool
RTCPParserV2::ParseIJ()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        return false;
    }

    _ptrRTCPData += 4; // Skip header

    _packetType = kExtendedIj;

    // State transition
    _state = State_ExtendedJitterItem;
    return true;
}

bool
RTCPParserV2::ParseIJItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4 || _numberOfBlocks <= 0)
    {
        _state = State_TopLevel;
        EndCurrentBlock();
        return false;
    }

    _packet.ExtendedJitterReportItem.Jitter = *_ptrRTCPData++ << 24;
    _packet.ExtendedJitterReportItem.Jitter += *_ptrRTCPData++ << 16;
    _packet.ExtendedJitterReportItem.Jitter += *_ptrRTCPData++ << 8;
    _packet.ExtendedJitterReportItem.Jitter += *_ptrRTCPData++;

    _numberOfBlocks--;
    _packetType = kExtendedIjItem;
    return true;
}

bool
RTCPParserV2::ParseSDES()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _ptrRTCPData += 4; // Skip header

    _state = State_SDESChunk;
    _packetType = kSdes;
    return true;
}

bool
RTCPParserV2::ParseSDESChunk()
{
    if (_numberOfBlocks <= 0)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _numberOfBlocks--;

    // Find CName item in a SDES chunk.
    while (_ptrRTCPData < _ptrRTCPBlockEnd)
    {
        const ptrdiff_t dataLen = _ptrRTCPBlockEnd - _ptrRTCPData;
        if (dataLen < 4)
        {
            _state = State_TopLevel;

            EndCurrentBlock();
            return false;
        }

        uint32_t SSRC = *_ptrRTCPData++ << 24;
        SSRC += *_ptrRTCPData++ << 16;
        SSRC += *_ptrRTCPData++ << 8;
        SSRC += *_ptrRTCPData++;

        const bool foundCname = ParseSDESItem();
        if (foundCname)
        {
            _packet.CName.SenderSSRC = SSRC; // Add SSRC
            return true;
        }
    }
    _state = State_TopLevel;

    EndCurrentBlock();
    return false;
}

bool
RTCPParserV2::ParseSDESItem()
{
    // Find CName
    // Only the CNAME item is mandatory. RFC 3550 page 46
    bool foundCName = false;

    size_t itemOctetsRead = 0;
    while (_ptrRTCPData < _ptrRTCPBlockEnd)
    {
        const uint8_t tag = *_ptrRTCPData++;
        ++itemOctetsRead;

        if (tag == 0)
        {
            // End tag! 4 oct aligned
            while ((itemOctetsRead++ % 4) != 0)
            {
                ++_ptrRTCPData;
            }
            return foundCName;
        }

        if (_ptrRTCPData < _ptrRTCPBlockEnd)
        {
            const uint8_t len = *_ptrRTCPData++;
            ++itemOctetsRead;

            if (tag == 1)
            {
                // CNAME

                // Sanity
                if ((_ptrRTCPData + len) >= _ptrRTCPBlockEnd)
                {
                    _state = State_TopLevel;

                    EndCurrentBlock();
                    return false;
                }
                uint8_t i = 0;
                for (; i < len; ++i)
                {
                    const uint8_t c = _ptrRTCPData[i];
                    if ((c < ' ') || (c > '{') || (c == '%') || (c == '\\'))
                    {
                        // Illegal char
                        _state = State_TopLevel;

                        EndCurrentBlock();
                        return false;
                    }
                    _packet.CName.CName[i] = c;
                }
                // Make sure we are null terminated.
                _packet.CName.CName[i] = 0;
                _packetType = kSdesChunk;

                foundCName = true;
            }
            _ptrRTCPData += len;
            itemOctetsRead += len;
        }
    }

    // No end tag found!
    _state = State_TopLevel;

    EndCurrentBlock();
    return false;
}

bool
RTCPParserV2::ParseBYE()
{
    _ptrRTCPData += 4; // Skip header

    _state = State_BYEItem;

    return ParseBYEItem();
}

bool
RTCPParserV2::ParseBYEItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length < 4 || _numberOfBlocks == 0)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kBye;

    _packet.BYE.SenderSSRC = *_ptrRTCPData++ << 24;
    _packet.BYE.SenderSSRC += *_ptrRTCPData++ << 16;
    _packet.BYE.SenderSSRC += *_ptrRTCPData++ << 8;
    _packet.BYE.SenderSSRC += *_ptrRTCPData++;

    // we can have several CSRCs attached

    // sanity
    if (length >= 4 * _numberOfBlocks)
    {
        _ptrRTCPData += (_numberOfBlocks - 1) * 4;
    }
    _numberOfBlocks = 0;

    return true;
}
/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|reserved |   PT=XR=207   |             length            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                              SSRC                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
:                         report blocks                         :
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
bool RTCPParserV2::ParseXr()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length < 8)
    {
        EndCurrentBlock();
        return false;
    }

    _ptrRTCPData += 4; // Skip header

    _packet.XR.OriginatorSSRC = *_ptrRTCPData++ << 24;
    _packet.XR.OriginatorSSRC += *_ptrRTCPData++ << 16;
    _packet.XR.OriginatorSSRC += *_ptrRTCPData++ << 8;
    _packet.XR.OriginatorSSRC += *_ptrRTCPData++;

    _packetType = kXrHeader;
    _state = State_XRItem;
    return true;
}

/*  Extended report block format (RFC 3611).
BT: block type.
block length: length of report block in 32-bits words minus one (including
the header).
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|      BT       | type-specific |         block length          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
:             type-specific block contents                      :
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
bool RTCPParserV2::ParseXrItem() {
    const int kBlockHeaderLengthInBytes = 4;
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length < kBlockHeaderLengthInBytes) {
        _state = State_TopLevel;
        EndCurrentBlock();
        return false;
    }

    uint8_t block_type = *_ptrRTCPData++;
    _ptrRTCPData++;  // Ignore reserved.

    uint16_t block_length_in_4bytes = *_ptrRTCPData++ << 8;
    block_length_in_4bytes += *_ptrRTCPData++;

    switch (block_type) {
    case kBtReceiverReferenceTime:
        return ParseXrReceiverReferenceTimeItem(block_length_in_4bytes);
    case kBtDlrr:
        return ParseXrDlrr(block_length_in_4bytes);
    case kBtVoipMetric:
        return ParseXrVoipMetricItem(block_length_in_4bytes);
    default:
        return ParseXrUnsupportedBlockType(block_length_in_4bytes);
    }
}

/*  Receiver Reference Time Report Block.
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     BT=4      |   reserved    |       block length = 2        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|              NTP timestamp, most significant word             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|             NTP timestamp, least significant word             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
bool RTCPParserV2::ParseXrReceiverReferenceTimeItem(
    int block_length_4bytes) {
    const int kBlockLengthIn4Bytes = 2;
    const int kBlockLengthInBytes = kBlockLengthIn4Bytes * 4;
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (block_length_4bytes != kBlockLengthIn4Bytes ||
        length < kBlockLengthInBytes) {
        _state = State_TopLevel;
        EndCurrentBlock();
        return false;
    }

    _packet.XRReceiverReferenceTimeItem.NTPMostSignificant = *_ptrRTCPData++ << 24;
    _packet.XRReceiverReferenceTimeItem.NTPMostSignificant += *_ptrRTCPData++ << 16;
    _packet.XRReceiverReferenceTimeItem.NTPMostSignificant += *_ptrRTCPData++ << 8;
    _packet.XRReceiverReferenceTimeItem.NTPMostSignificant += *_ptrRTCPData++;

    _packet.XRReceiverReferenceTimeItem.NTPLeastSignificant = *_ptrRTCPData++ << 24;
    _packet.XRReceiverReferenceTimeItem.NTPLeastSignificant += *_ptrRTCPData++ << 16;
    _packet.XRReceiverReferenceTimeItem.NTPLeastSignificant += *_ptrRTCPData++ << 8;
    _packet.XRReceiverReferenceTimeItem.NTPLeastSignificant += *_ptrRTCPData++;

    _packetType = kXrReceiverReferenceTime;
    _state = State_XRItem;
    return true;
}

/*  DLRR Report Block.
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     BT=5      |   reserved    |         block length          |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                 SSRC_1 (SSRC of first receiver)               | sub-
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
|                         last RR (LRR)                         |   1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   delay since last RR (DLRR)                  |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                 SSRC_2 (SSRC of second receiver)              | sub-
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
:                               ...                             :   2
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/
bool RTCPParserV2::ParseXrDlrr(int block_length_4bytes) {
    const int kSubBlockLengthIn4Bytes = 3;
    if (block_length_4bytes < 0 ||
        (block_length_4bytes % kSubBlockLengthIn4Bytes) != 0) {
        _state = State_TopLevel;
        EndCurrentBlock();
        return false;
    }
    _packetType = kXrDlrrReportBlock;
    _state = State_XR_DLLRItem;
    _numberOfBlocks = block_length_4bytes / kSubBlockLengthIn4Bytes;
    return true;
}

bool RTCPParserV2::ParseXrDlrrItem() {
    if (_numberOfBlocks == 0) {
        _state = State_XRItem;
        return false;
    }
    const int kSubBlockLengthInBytes = 12;
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length < kSubBlockLengthInBytes) {
        _state = State_TopLevel;
        EndCurrentBlock();
        return false;
    }

    _packet.XRDLRRReportBlockItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.XRDLRRReportBlockItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.XRDLRRReportBlockItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.XRDLRRReportBlockItem.SSRC += *_ptrRTCPData++;

    _packet.XRDLRRReportBlockItem.LastRR = *_ptrRTCPData++ << 24;
    _packet.XRDLRRReportBlockItem.LastRR += *_ptrRTCPData++ << 16;
    _packet.XRDLRRReportBlockItem.LastRR += *_ptrRTCPData++ << 8;
    _packet.XRDLRRReportBlockItem.LastRR += *_ptrRTCPData++;

    _packet.XRDLRRReportBlockItem.DelayLastRR = *_ptrRTCPData++ << 24;
    _packet.XRDLRRReportBlockItem.DelayLastRR += *_ptrRTCPData++ << 16;
    _packet.XRDLRRReportBlockItem.DelayLastRR += *_ptrRTCPData++ << 8;
    _packet.XRDLRRReportBlockItem.DelayLastRR += *_ptrRTCPData++;

    _packetType = kXrDlrrReportBlockItem;
    --_numberOfBlocks;
    _state = State_XR_DLLRItem;
    return true;
}
/*  VoIP Metrics Report Block.
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     BT=7      |   reserved    |       block length = 8        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        SSRC of source                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   loss rate   | discard rate  | burst density |  gap density  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       burst duration          |         gap duration          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     round trip delay          |       end system delay        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| signal level  |  noise level  |     RERL      |     Gmin      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   R factor    | ext. R factor |    MOS-LQ     |    MOS-CQ     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   RX config   |   reserved    |          JB nominal           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          JB maximum           |          JB abs max           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

bool RTCPParserV2::ParseXrVoipMetricItem(int block_length_4bytes) {
    const int kBlockLengthIn4Bytes = 8;
    const int kBlockLengthInBytes = kBlockLengthIn4Bytes * 4;
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (block_length_4bytes != kBlockLengthIn4Bytes ||
        length < kBlockLengthInBytes) {
        _state = State_TopLevel;
        EndCurrentBlock();
        return false;
    }

    _packet.XRVOIPMetricItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.XRVOIPMetricItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.XRVOIPMetricItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.SSRC += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.lossRate = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.discardRate = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.burstDensity = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.gapDensity = *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.burstDuration = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.burstDuration += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.gapDuration = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.gapDuration += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.roundTripDelay = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.roundTripDelay += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.endSystemDelay = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.endSystemDelay += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.signalLevel = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.noiseLevel = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.RERL = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.Gmin = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.Rfactor = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.extRfactor = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.MOSLQ = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.MOSCQ = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.RXconfig = *_ptrRTCPData++;
    _ptrRTCPData++; // skip reserved

    _packet.XRVOIPMetricItem.JBnominal = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.JBnominal += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.JBmax = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.JBmax += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.JBabsMax = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.JBabsMax += *_ptrRTCPData++;

    _packetType = kXrVoipMetric;
    _state = State_XRItem;
    return true;
}

bool RTCPParserV2::ParseXrUnsupportedBlockType(
    int block_length_4bytes) {
    const int32_t kBlockLengthInBytes = block_length_4bytes * 4;
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length < kBlockLengthInBytes) {
        _state = State_TopLevel;
        EndCurrentBlock();
        return false;
    }
    // Skip block.
    _ptrRTCPData += kBlockLengthInBytes;
    _state = State_XRItem;
    return false;
}

bool
RTCPParserV2::ParseFBCommon(const RTCPCommonHeader& header)
{
    assert((header.PT == PT_RTPFB) || (header.PT == PT_PSFB)); // Parser logic check

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 12) // 4 * 3, RFC4585 section 6.1
    {
        EndCurrentBlock();
        return false;
    }

    _ptrRTCPData += 4; // Skip RTCP header

    uint32_t senderSSRC = *_ptrRTCPData++ << 24;
    senderSSRC += *_ptrRTCPData++ << 16;
    senderSSRC += *_ptrRTCPData++ << 8;
    senderSSRC += *_ptrRTCPData++;

    uint32_t mediaSSRC = *_ptrRTCPData++ << 24;
    mediaSSRC += *_ptrRTCPData++ << 16;
    mediaSSRC += *_ptrRTCPData++ << 8;
    mediaSSRC += *_ptrRTCPData++;

    if (header.PT == PT_RTPFB)
    {
        // Transport layer feedback

        switch (header.IC)
        {
        case 1:
        {
                  // NACK
                  _packetType = kRtpfbNack;
                  _packet.NACK.SenderSSRC = senderSSRC;
                  _packet.NACK.MediaSSRC = mediaSSRC;

                  _state = State_RTPFB_NACKItem;

                  return true;
        }
        case 2:
        {
                  // used to be ACK is this code point, which is removed
                  // conficts with http://tools.ietf.org/html/draft-levin-avt-rtcp-burst-00
                  break;
        }
        case 3:
        {
                  // TMMBR
                  _packetType = kRtpfbTmmbr;
                  _packet.TMMBR.SenderSSRC = senderSSRC;
                  _packet.TMMBR.MediaSSRC = mediaSSRC;

                  _state = State_RTPFB_TMMBRItem;

                  return true;
        }
        case 4:
        {
                  // TMMBN
                  _packetType = kRtpfbTmmbn;
                  _packet.TMMBN.SenderSSRC = senderSSRC;
                  _packet.TMMBN.MediaSSRC = mediaSSRC;

                  _state = State_RTPFB_TMMBNItem;

                  return true;
        }
        case 5:
        {
                  // RTCP-SR-REQ Rapid Synchronisation of RTP Flows
                  // draft-perkins-avt-rapid-rtp-sync-03.txt
                  // trigger a new RTCP SR
                  _packetType = kRtpfbSrReq;

                  // Note: No state transition, SR REQ is empty!
                  return true;
        }
        default:
            break;
        }
        EndCurrentBlock();
        return false;
    }
#ifdef SLICE_FCI
    else if (header.PT == PT_PSFB)
    {
        // Payload specific feedback
        switch (header.IC)
        {
        case 1:
            // PLI
            _packetType = kPsfbPli;
            _packet.PLI.SenderSSRC = senderSSRC;
            _packet.PLI.MediaSSRC = mediaSSRC;

            // Note: No state transition, PLI FCI is empty!
            return true;
        case 2:
            // SLI
            _packetType = kPsfbSli;
            _packet.SLI.SenderSSRC = senderSSRC;
            _packet.SLI.MediaSSRC = mediaSSRC;

            _state = State_PSFB_SLIItem;

            return true;
        case 3:
            _packetType = kPsfbRpsi;
            _packet.RPSI.SenderSSRC = senderSSRC;
            _packet.RPSI.MediaSSRC = mediaSSRC;

            _state = State_PSFB_RPSIItem;
            return true;
        case 4:
            // FIR
            _packetType = kPsfbFir;
            _packet.FIR.SenderSSRC = senderSSRC;
            _packet.FIR.MediaSSRC = mediaSSRC;

            _state = State_PSFB_FIRItem;
            return true;
        case 15:
            _packetType = kPsfbApp;
            _packet.PSFBAPP.SenderSSRC = senderSSRC;
            _packet.PSFBAPP.MediaSSRC = mediaSSRC;

            _state = State_PSFB_AppItem;
            return true;
        default:
            break;
        }

        EndCurrentBlock();
        return false;
    }
#endif
    else
    {
        assert(false);

        EndCurrentBlock();
        return false;
    }
}

bool
RTCPParserV2::ParseNACKItem()
{
    // RFC 4585 6.2.1. Generic NACK

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kRtpfbNackItem;

    _packet.NACKItem.PacketID = *_ptrRTCPData++ << 8;
    _packet.NACKItem.PacketID += *_ptrRTCPData++;

    _packet.NACKItem.BitMask = *_ptrRTCPData++ << 8;
    _packet.NACKItem.BitMask += *_ptrRTCPData++;

    return true;
}

bool
RTCPParserV2::ParseTMMBRItem()
{
    // RFC 5104 4.2.1. Temporary Maximum Media Stream Bit Rate Request (TMMBR)

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kRtpfbTmmbrItem;

    _packet.TMMBRItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.TMMBRItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.TMMBRItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.TMMBRItem.SSRC += *_ptrRTCPData++;

    uint8_t mxtbrExp = (_ptrRTCPData[0] >> 2) & 0x3F;

    uint32_t mxtbrMantissa = (_ptrRTCPData[0] & 0x03) << 15;
    mxtbrMantissa += (_ptrRTCPData[1] << 7);
    mxtbrMantissa += (_ptrRTCPData[2] >> 1) & 0x7F;

    uint32_t measuredOH = (_ptrRTCPData[2] & 0x01) << 8;
    measuredOH += _ptrRTCPData[3];

    _ptrRTCPData += 4; // Fwd read data

    _packet.TMMBRItem.MaxTotalMediaBitRate = ((mxtbrMantissa << mxtbrExp) / 1000);
    _packet.TMMBRItem.MeasuredOverhead = measuredOH;

    return true;
}

bool
RTCPParserV2::ParseTMMBNItem()
{
    // RFC 5104 4.2.2. Temporary Maximum Media Stream Bit Rate Notification (TMMBN)

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kRtpfbTmmbnItem;

    _packet.TMMBNItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.TMMBNItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.TMMBNItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.TMMBNItem.SSRC += *_ptrRTCPData++;

    uint8_t mxtbrExp = (_ptrRTCPData[0] >> 2) & 0x3F;

    uint32_t mxtbrMantissa = (_ptrRTCPData[0] & 0x03) << 15;
    mxtbrMantissa += (_ptrRTCPData[1] << 7);
    mxtbrMantissa += (_ptrRTCPData[2] >> 1) & 0x7F;

    uint32_t measuredOH = (_ptrRTCPData[2] & 0x01) << 8;
    measuredOH += _ptrRTCPData[3];

    _ptrRTCPData += 4; // Fwd read data

    _packet.TMMBNItem.MaxTotalMediaBitRate = ((mxtbrMantissa << mxtbrExp) / 1000);
    _packet.TMMBNItem.MeasuredOverhead = measuredOH;

    return true;
}



bool
RTCPParserV2::ParseAPP(const RTCPCommonHeader& header)
{
    ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 12) // 4 * 3, RFC 3550 6.7 APP: Application-Defined RTCP Packet
    {
        EndCurrentBlock();
        return false;
    }

    _ptrRTCPData += 4; // Skip RTCP header

    uint32_t senderSSRC = *_ptrRTCPData++ << 24;
    senderSSRC += *_ptrRTCPData++ << 16;
    senderSSRC += *_ptrRTCPData++ << 8;
    senderSSRC += *_ptrRTCPData++;

    uint32_t name = *_ptrRTCPData++ << 24;
    name += *_ptrRTCPData++ << 16;
    name += *_ptrRTCPData++ << 8;
    name += *_ptrRTCPData++;

    length = _ptrRTCPBlockEnd - _ptrRTCPData;

    _packetType = kApp;

    _packet.APP.SubType = header.IC;
    _packet.APP.Name = name;

    _state = State_AppItem;
    return true;
}

bool
RTCPParserV2::ParseAPPItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length < 4)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packetType = kAppItem;

    if (length > kRtcpAppCode_DATA_SIZE)
    {
        memcpy(_packet.APP.Data, _ptrRTCPData, kRtcpAppCode_DATA_SIZE);
        _packet.APP.Size = kRtcpAppCode_DATA_SIZE;
        _ptrRTCPData += kRtcpAppCode_DATA_SIZE;
    }
    else
    {
        memcpy(_packet.APP.Data, _ptrRTCPData, length);
        _packet.APP.Size = (uint16_t)length;
        _ptrRTCPData += length;
    }
    return true;
}

RTCPPacketIterator::RTCPPacketIterator(uint8_t* rtcpData,
    size_t rtcpDataLength)
    : _ptrBegin(rtcpData),
    _ptrEnd(rtcpData + rtcpDataLength),
    _ptrBlock(NULL) {
    memset(&_header, 0, sizeof(_header));
}

RTCPPacketIterator::~RTCPPacketIterator() {
}

const RTCPCommonHeader*
RTCPPacketIterator::Begin()
{
    _ptrBlock = _ptrBegin;

    return Iterate();
}

const RTCPCommonHeader*
RTCPPacketIterator::Iterate()
{
    const bool success = RTCPParseCommonHeader(_ptrBlock, _ptrEnd, _header);
    if (!success)
    {
        _ptrBlock = NULL;
        return NULL;
    }
    _ptrBlock += _header.LengthInOctets;

    if (_ptrBlock > _ptrEnd)
    {
        _ptrBlock = NULL;
        return  NULL;
    }

    return &_header;
}

const RTCPCommonHeader*
RTCPPacketIterator::Current()
{
    if (!_ptrBlock)
    {
        return NULL;
    }

    return &_header;
}
NackStats::NackStats()
: max_sequence_number_(0),
requests_(0),
unique_requests_(0) {}

NackStats::~NackStats() {}

void NackStats::ReportRequest(uint16_t sequence_number) {
    if (requests_ == 0 ||
        IsNewerSequenceNumber(sequence_number, max_sequence_number_)) {
        max_sequence_number_ = sequence_number;
        ++unique_requests_;
    }
    ++requests_;
}
RTCPPacketInformation::RTCPPacketInformation()
: rtcpPacketTypeFlags(0),
remoteSSRC(0),
nackSequenceNumbers(),
applicationSubType(0),
applicationName(0),
applicationData(),
applicationLength(0),
rtt(0),
interArrivalJitter(0),
sliPictureId(0),
rpsiPictureId(0),
receiverEstimatedMaxBitrate(0),
ntp_secs(0),
ntp_frac(0),
rtp_timestamp(0),
xr_originator_ssrc(0),
xr_dlrr_item(false),
VoIPMetric(NULL) {
}

RTCPPacketInformation::~RTCPPacketInformation()
{
    delete[] applicationData;
    delete VoIPMetric;
}

void
RTCPPacketInformation::AddVoIPMetric(const RTCPVoIPMetric* metric)
{
    VoIPMetric = new RTCPVoIPMetric();
    memcpy(VoIPMetric, metric, sizeof(RTCPVoIPMetric));
}

void RTCPPacketInformation::AddApplicationData(const uint8_t* data,
    const uint16_t size) {
    uint8_t* oldData = applicationData;
    uint16_t oldLength = applicationLength;

    // Don't copy more than kRtcpAppCode_DATA_SIZE bytes.
    uint16_t copySize = size;
    if (size > kRtcpAppCode_DATA_SIZE) {
        copySize = kRtcpAppCode_DATA_SIZE;
    }

    applicationLength += copySize;
    applicationData = new uint8_t[applicationLength];

    if (oldData)
    {
        memcpy(applicationData, oldData, oldLength);
        memcpy(applicationData + oldLength, data, copySize);
        delete[] oldData;
    }
    else
    {
        memcpy(applicationData, data, copySize);
    }
}

void
RTCPPacketInformation::ResetNACKPacketIdArray()
{
    nackSequenceNumbers.clear();
}

void
RTCPPacketInformation::AddNACKPacket(const uint16_t packetID)
{
    if (nackSequenceNumbers.size() >= kSendSideNackListSizeSanity) {
        return;
    }
    nackSequenceNumbers.push_back(packetID);
}

void
RTCPPacketInformation::AddReportInfo(
const RTCPReportBlockInformation& report_block_info)
{
    this->rtt = report_block_info.RTT;
    report_blocks.push_back(report_block_info.remoteReceiveBlock);
}

RTCPReportBlockInformation::RTCPReportBlockInformation() :
remoteReceiveBlock(),
remoteMaxJitter(0),
RTT(0),
minRTT(0),
maxRTT(0),
avgRTT(0),
numAverageCalcs(0)
{
    memset(&remoteReceiveBlock, 0, sizeof(remoteReceiveBlock));
}

RTCPReportBlockInformation::~RTCPReportBlockInformation()
{
}

RTCPReceiveInformation::RTCPReceiveInformation()
: lastTimeReceived(0),
lastFIRSequenceNumber(-1),
lastFIRRequest(0),
readyForDelete(false) {
}

RTCPReceiveInformation::~RTCPReceiveInformation() {
}

// Increase size of TMMBRSet if needed, and also take care of
// the _tmmbrSetTimeouts vector.
void RTCPReceiveInformation::VerifyAndAllocateTMMBRSet(
    const uint32_t minimumSize) {
    if (minimumSize > TmmbrSet.sizeOfSet()) {
        TmmbrSet.VerifyAndAllocateSetKeepingData(minimumSize);
        // make sure that our buffers are big enough
        _tmmbrSetTimeouts.reserve(minimumSize);
    }
}

void RTCPReceiveInformation::InsertTMMBRItem(
    const uint32_t senderSSRC,
    const RTCPPacketRTPFBTMMBRItem& TMMBRItem,
    const int64_t currentTimeMS) {
    // serach to see if we have it in our list
    for (uint32_t i = 0; i < TmmbrSet.lengthOfSet(); i++)  {
        if (TmmbrSet.Ssrc(i) == senderSSRC) {
            // we already have this SSRC in our list update it
            TmmbrSet.SetEntry(i,
                TMMBRItem.MaxTotalMediaBitRate,
                TMMBRItem.MeasuredOverhead,
                senderSSRC);
            _tmmbrSetTimeouts[i] = currentTimeMS;
            return;
        }
    }
    VerifyAndAllocateTMMBRSet(TmmbrSet.lengthOfSet() + 1);
    TmmbrSet.AddEntry(TMMBRItem.MaxTotalMediaBitRate,
        TMMBRItem.MeasuredOverhead,
        senderSSRC);
    _tmmbrSetTimeouts.push_back(currentTimeMS);
}

int32_t RTCPReceiveInformation::GetTMMBRSet(
    const uint32_t sourceIdx,
    const uint32_t targetIdx,
    TMMBRSet* candidateSet,
    const int64_t currentTimeMS) {
    if (sourceIdx >= TmmbrSet.lengthOfSet()) {
        return -1;
    }
    if (targetIdx >= candidateSet->sizeOfSet()) {
        return -1;
    }
    // use audio define since we don't know what interval the remote peer is using
    if (currentTimeMS - _tmmbrSetTimeouts[sourceIdx] >
        5 * RTCP_INTERVAL_AUDIO_MS) {
        // value timed out
        TmmbrSet.RemoveEntry(sourceIdx);
        _tmmbrSetTimeouts.erase(_tmmbrSetTimeouts.begin() + sourceIdx);
        return -1;
    }
    candidateSet->SetEntry(targetIdx,
        TmmbrSet.Tmmbr(sourceIdx),
        TmmbrSet.PacketOH(sourceIdx),
        TmmbrSet.Ssrc(sourceIdx));
    return 0;
}

void RTCPReceiveInformation::VerifyAndAllocateBoundingSet(
    const uint32_t minimumSize) {
    TmmbnBoundingSet.VerifyAndAllocateSet(minimumSize);
}

TMMBRSet::TMMBRSet() :
_sizeOfSet(0),
_lengthOfSet(0)
{
}

TMMBRSet::~TMMBRSet()
{
    _sizeOfSet = 0;
    _lengthOfSet = 0;
}

void
TMMBRSet::VerifyAndAllocateSet(uint32_t minimumSize)
{
    if (minimumSize > _sizeOfSet)
    {
        // make sure that our buffers are big enough
        _data.resize(minimumSize);
        _sizeOfSet = minimumSize;
    }
    // reset memory
    for (uint32_t i = 0; i < _sizeOfSet; i++)
    {
        _data.at(i).tmmbr = 0;
        _data.at(i).packet_oh = 0;
        _data.at(i).ssrc = 0;
    }
    _lengthOfSet = 0;
}

void
TMMBRSet::VerifyAndAllocateSetKeepingData(uint32_t minimumSize)
{
    if (minimumSize > _sizeOfSet)
    {
        {
            _data.resize(minimumSize);
        }
        _sizeOfSet = minimumSize;
    }
}

void TMMBRSet::SetEntry(unsigned int i,
    uint32_t tmmbrSet,
    uint32_t packetOHSet,
    uint32_t ssrcSet) {
    assert(i < _sizeOfSet);
    _data.at(i).tmmbr = tmmbrSet;
    _data.at(i).packet_oh = packetOHSet;
    _data.at(i).ssrc = ssrcSet;
    if (i >= _lengthOfSet) {
        _lengthOfSet = i + 1;
    }
}

void TMMBRSet::AddEntry(uint32_t tmmbrSet,
    uint32_t packetOHSet,
    uint32_t ssrcSet) {
    assert(_lengthOfSet < _sizeOfSet);
    SetEntry(_lengthOfSet, tmmbrSet, packetOHSet, ssrcSet);
}

void TMMBRSet::RemoveEntry(uint32_t sourceIdx) {
    assert(sourceIdx < _lengthOfSet);
    _data.erase(_data.begin() + sourceIdx);
    _lengthOfSet--;
    _data.resize(_sizeOfSet);  // Ensure that size remains the same.
}

void TMMBRSet::SwapEntries(uint32_t i, uint32_t j) {
    SetElement temp;
    temp = _data[i];
    _data[i] = _data[j];
    _data[j] = temp;
}

void TMMBRSet::ClearEntry(uint32_t idx) {
    SetEntry(idx, 0, 0, 0);
}

TMMBRHelp::TMMBRHelp()
: _criticalSection(CriticalSectionWrapper::CreateCriticalSection()),
_candidateSet(),
_boundingSet(),
_boundingSetToSend(),
_ptrIntersectionBoundingSet(NULL),
_ptrMaxPRBoundingSet(NULL) {
}

TMMBRHelp::~TMMBRHelp() {
    delete[] _ptrIntersectionBoundingSet;
    delete[] _ptrMaxPRBoundingSet;
    _ptrIntersectionBoundingSet = 0;
    _ptrMaxPRBoundingSet = 0;
    delete _criticalSection;
}

TMMBRSet*
TMMBRHelp::VerifyAndAllocateBoundingSet(uint32_t minimumSize)
{
    CriticalSectionScoped lock(_criticalSection);

    if (minimumSize > _boundingSet.sizeOfSet())
    {
        // make sure that our buffers are big enough
        if (_ptrIntersectionBoundingSet)
        {
            delete[] _ptrIntersectionBoundingSet;
            delete[] _ptrMaxPRBoundingSet;
        }
        _ptrIntersectionBoundingSet = new float[minimumSize];
        _ptrMaxPRBoundingSet = new float[minimumSize];
    }
    _boundingSet.VerifyAndAllocateSet(minimumSize);
    return &_boundingSet;
}

TMMBRSet* TMMBRHelp::BoundingSet() {
    return &_boundingSet;
}

int32_t
TMMBRHelp::SetTMMBRBoundingSetToSend(const TMMBRSet* boundingSetToSend,
const uint32_t maxBitrateKbit)
{
    CriticalSectionScoped lock(_criticalSection);

    if (boundingSetToSend == NULL)
    {
        _boundingSetToSend.clearSet();
        return 0;
    }

    VerifyAndAllocateBoundingSetToSend(boundingSetToSend->lengthOfSet());
    _boundingSetToSend.clearSet();
    for (uint32_t i = 0; i < boundingSetToSend->lengthOfSet(); i++)
    {
        // cap at our configured max bitrate
        uint32_t bitrate = boundingSetToSend->Tmmbr(i);
        if (maxBitrateKbit)
        {
            // do we have a configured max bitrate?
            if (bitrate > maxBitrateKbit)
            {
                bitrate = maxBitrateKbit;
            }
        }
        _boundingSetToSend.SetEntry(i, bitrate,
            boundingSetToSend->PacketOH(i),
            boundingSetToSend->Ssrc(i));
    }
    return 0;
}

int32_t
TMMBRHelp::VerifyAndAllocateBoundingSetToSend(uint32_t minimumSize)
{
    CriticalSectionScoped lock(_criticalSection);

    _boundingSetToSend.VerifyAndAllocateSet(minimumSize);
    return 0;
}

TMMBRSet*
TMMBRHelp::VerifyAndAllocateCandidateSet(uint32_t minimumSize)
{
    CriticalSectionScoped lock(_criticalSection);

    _candidateSet.VerifyAndAllocateSet(minimumSize);
    return &_candidateSet;
}

TMMBRSet*
TMMBRHelp::CandidateSet()
{
    return &_candidateSet;
}

TMMBRSet*
TMMBRHelp::BoundingSetToSend()
{
    return &_boundingSetToSend;
}

int32_t
TMMBRHelp::FindTMMBRBoundingSet(TMMBRSet*& boundingSet)
{
    CriticalSectionScoped lock(_criticalSection);

    // Work on local variable, will be modified
    TMMBRSet    candidateSet;
    candidateSet.VerifyAndAllocateSet(_candidateSet.sizeOfSet());

    // TODO(hta) Figure out if this should be lengthOfSet instead.
    for (uint32_t i = 0; i < _candidateSet.sizeOfSet(); i++)
    {
        if (_candidateSet.Tmmbr(i))
        {
            candidateSet.AddEntry(_candidateSet.Tmmbr(i),
                _candidateSet.PacketOH(i),
                _candidateSet.Ssrc(i));
        }
        else
        {
            // make sure this is zero if tmmbr = 0
            assert(_candidateSet.PacketOH(i) == 0);
            // Old code:
            // _candidateSet.ptrPacketOHSet[i] = 0;
        }
    }

    // Number of set candidates
    int32_t numSetCandidates = candidateSet.lengthOfSet();
    // Find bounding set
    uint32_t numBoundingSet = 0;
    if (numSetCandidates > 0)
    {
        numBoundingSet = FindTMMBRBoundingSet(numSetCandidates, candidateSet);
        if (numBoundingSet < 1 || (numBoundingSet > _candidateSet.sizeOfSet()))
        {
            return -1;
        }
        boundingSet = &_boundingSet;
    }
    return numBoundingSet;
}


int32_t
TMMBRHelp::FindTMMBRBoundingSet(int32_t numCandidates, TMMBRSet& candidateSet)
{
    CriticalSectionScoped lock(_criticalSection);

    uint32_t numBoundingSet = 0;
    VerifyAndAllocateBoundingSet(candidateSet.sizeOfSet());

    if (numCandidates == 1)
    {
        // TODO(hta): lengthOfSet instead of sizeOfSet?
        for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
        {
            if (candidateSet.Tmmbr(i) > 0)
            {
                _boundingSet.AddEntry(candidateSet.Tmmbr(i),
                    candidateSet.PacketOH(i),
                    candidateSet.Ssrc(i));
                numBoundingSet++;
            }
        }
        return (numBoundingSet == 1) ? 1 : -1;
    }

    // 1. Sort by increasing packetOH
    for (int i = candidateSet.sizeOfSet() - 1; i >= 0; i--)
    {
        for (int j = 1; j <= i; j++)
        {
            if (candidateSet.PacketOH(j - 1) > candidateSet.PacketOH(j))
            {
                candidateSet.SwapEntries(j - 1, j);
            }
        }
    }
    // 2. For tuples with same OH, keep the one w/ the lowest bitrate
    for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
    {
        if (candidateSet.Tmmbr(i) > 0)
        {
            // get min bitrate for packets w/ same OH
            uint32_t currentPacketOH = candidateSet.PacketOH(i);
            uint32_t currentMinTMMBR = candidateSet.Tmmbr(i);
            uint32_t currentMinIndexTMMBR = i;
            for (uint32_t j = i + 1; j < candidateSet.sizeOfSet(); j++)
            {
                if (candidateSet.PacketOH(j) == currentPacketOH)
                {
                    if (candidateSet.Tmmbr(j) < currentMinTMMBR)
                    {
                        currentMinTMMBR = candidateSet.Tmmbr(j);
                        currentMinIndexTMMBR = j;
                    }
                }
            }
            // keep lowest bitrate
            for (uint32_t j = 0; j < candidateSet.sizeOfSet(); j++)
            {
                if (candidateSet.PacketOH(j) == currentPacketOH
                    && j != currentMinIndexTMMBR)
                {
                    candidateSet.ClearEntry(j);
                }
            }
        }
    }
    // 3. Select and remove tuple w/ lowest tmmbr.
    // (If more than 1, choose the one w/ highest OH).
    uint32_t minTMMBR = 0;
    uint32_t minIndexTMMBR = 0;
    for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
    {
        if (candidateSet.Tmmbr(i) > 0)
        {
            minTMMBR = candidateSet.Tmmbr(i);
            minIndexTMMBR = i;
            break;
        }
    }

    for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
    {
        if (candidateSet.Tmmbr(i) > 0 && candidateSet.Tmmbr(i) <= minTMMBR)
        {
            // get min bitrate
            minTMMBR = candidateSet.Tmmbr(i);
            minIndexTMMBR = i;
        }
    }
    // first member of selected list
    _boundingSet.SetEntry(numBoundingSet,
        candidateSet.Tmmbr(minIndexTMMBR),
        candidateSet.PacketOH(minIndexTMMBR),
        candidateSet.Ssrc(minIndexTMMBR));

    // set intersection value
    _ptrIntersectionBoundingSet[numBoundingSet] = 0;
    // calculate its maximum packet rate (where its line crosses x-axis)
    _ptrMaxPRBoundingSet[numBoundingSet]
        = _boundingSet.Tmmbr(numBoundingSet) * 1000
        / float(8 * _boundingSet.PacketOH(numBoundingSet));
    numBoundingSet++;
    // remove from candidate list
    candidateSet.ClearEntry(minIndexTMMBR);
    numCandidates--;

    // 4. Discard from candidate list all tuple w/ lower OH
    // (next tuple must be steeper)
    for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
    {
        if (candidateSet.Tmmbr(i) > 0
            && candidateSet.PacketOH(i) < _boundingSet.PacketOH(0))
        {
            candidateSet.ClearEntry(i);
            numCandidates--;
        }
    }

    if (numCandidates == 0)
    {
        // Should be true already:_boundingSet.lengthOfSet = numBoundingSet;
        assert(_boundingSet.lengthOfSet() == numBoundingSet);
        return numBoundingSet;
    }

    bool getNewCandidate = true;
    int curCandidateTMMBR = 0;
    int curCandidateIndex = 0;
    int curCandidatePacketOH = 0;
    int curCandidateSSRC = 0;
    do
    {
        if (getNewCandidate)
        {
            // 5. Remove first remaining tuple from candidate list
            for (uint32_t i = 0; i < candidateSet.sizeOfSet(); i++)
            {
                if (candidateSet.Tmmbr(i) > 0)
                {
                    curCandidateTMMBR = candidateSet.Tmmbr(i);
                    curCandidatePacketOH = candidateSet.PacketOH(i);
                    curCandidateSSRC = candidateSet.Ssrc(i);
                    curCandidateIndex = i;
                    candidateSet.ClearEntry(curCandidateIndex);
                    break;
                }
            }
        }

        // 6. Calculate packet rate and intersection of the current
        // line with line of last tuple in selected list
        float packetRate
            = float(curCandidateTMMBR
            - _boundingSet.Tmmbr(numBoundingSet - 1)) * 1000
            / (8 * (curCandidatePacketOH
            - _boundingSet.PacketOH(numBoundingSet - 1)));

        // 7. If the packet rate is equal or lower than intersection of
        //    last tuple in selected list,
        //    remove last tuple in selected list & go back to step 6
        if (packetRate <= _ptrIntersectionBoundingSet[numBoundingSet - 1])
        {
            // remove last tuple and goto step 6
            numBoundingSet--;
            _boundingSet.ClearEntry(numBoundingSet);
            _ptrIntersectionBoundingSet[numBoundingSet] = 0;
            _ptrMaxPRBoundingSet[numBoundingSet] = 0;
            getNewCandidate = false;
        }
        else
        {
            // 8. If packet rate is lower than maximum packet rate of
            // last tuple in selected list, add current tuple to selected
            // list
            if (packetRate < _ptrMaxPRBoundingSet[numBoundingSet - 1])
            {
                _boundingSet.SetEntry(numBoundingSet,
                    curCandidateTMMBR,
                    curCandidatePacketOH,
                    curCandidateSSRC);
                _ptrIntersectionBoundingSet[numBoundingSet] = packetRate;
                _ptrMaxPRBoundingSet[numBoundingSet]
                    = _boundingSet.Tmmbr(numBoundingSet) * 1000
                    / float(8 * _boundingSet.PacketOH(numBoundingSet));
                numBoundingSet++;
            }
            numCandidates--;
            getNewCandidate = true;
        }

        // 9. Go back to step 5 if any tuple remains in candidate list
    } while (numCandidates > 0);

    return numBoundingSet;
}

bool TMMBRHelp::IsOwner(const uint32_t ssrc,
    const uint32_t length) const {
    CriticalSectionScoped lock(_criticalSection);

    if (length == 0) {
        // Empty bounding set.
        return false;
    }
    for (uint32_t i = 0;
        (i < length) && (i < _boundingSet.sizeOfSet()); ++i) {
        if (_boundingSet.Ssrc(i) == ssrc) {
            return true;
        }
    }
    return false;
}

bool TMMBRHelp::CalcMinBitRate(uint32_t* minBitrateKbit) const {
    CriticalSectionScoped lock(_criticalSection);

    if (_candidateSet.sizeOfSet() == 0) {
        // Empty bounding set.
        return false;
    }
    //std::numeric_limits<uint32_t>::max();
    *minBitrateKbit = 0xffffffff;

    for (uint32_t i = 0; i < _candidateSet.lengthOfSet(); ++i) {
        uint32_t curNetBitRateKbit = _candidateSet.Tmmbr(i);
        if (curNetBitRateKbit < MIN_VIDEO_BW_MANAGEMENT_BITRATE) {
            curNetBitRateKbit = MIN_VIDEO_BW_MANAGEMENT_BITRATE;
        }
        *minBitrateKbit = curNetBitRateKbit < *minBitrateKbit ?
        curNetBitRateKbit : *minBitrateKbit;
    }
    return true;
}
#ifdef SLICE_FCI
void
RTCPParserV2::IterateSLIItem()
{
    const bool success = ParseSLIItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IterateRPSIItem()
{
    const bool success = ParseRPSIItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IterateFIRItem()
{
    const bool success = ParseFIRItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IteratePsfbAppItem()
{
    const bool success = ParsePsfbAppItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPParserV2::IteratePsfbREMBItem()
{
    const bool success = ParsePsfbREMBItem();
    if (!success)
    {
        Iterate();
    }
}
bool
RTCPParserV2::ParseSLIItem()
{
    // RFC 5104 6.3.2.  Slice Loss Indication (SLI)
    /*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |            First        |        Number           | PictureID |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packetType = kPsfbSliItem;

    uint32_t buffer;
    buffer = *_ptrRTCPData++ << 24;
    buffer += *_ptrRTCPData++ << 16;
    buffer += *_ptrRTCPData++ << 8;
    buffer += *_ptrRTCPData++;

    _packet.SLIItem.FirstMB = uint16_t((buffer >> 19) & 0x1fff);
    _packet.SLIItem.NumberOfMB = uint16_t((buffer >> 6) & 0x1fff);
    _packet.SLIItem.PictureId = uint8_t(buffer & 0x3f);

    return true;
}

bool
RTCPParserV2::ParseFIRItem()
{
    // RFC 5104 4.3.1. Full Intra Request (FIR)

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kPsfbFirItem;

    _packet.FIRItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.FIRItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.FIRItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.FIRItem.SSRC += *_ptrRTCPData++;

    _packet.FIRItem.CommandSequenceNumber = *_ptrRTCPData++;
    _ptrRTCPData += 3; // Skip "Reserved" bytes.
    return true;
}

bool RTCPParserV2::ParseRPSIItem() {

    // RFC 4585 6.3.3.  Reference Picture Selection Indication (RPSI).
    //
    //  0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |      PB       |0| Payload Type|    Native RPSI bit string     |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |   defined per codec          ...                | Padding (0) |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4) {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if (length > 2 + RTCP_RPSI_DATA_SIZE) {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kPsfbRpsi;

    uint8_t padding_bits = *_ptrRTCPData++;
    _packet.RPSI.PayloadType = *_ptrRTCPData++;

    memcpy(_packet.RPSI.NativeBitString, _ptrRTCPData, length - 2);
    _ptrRTCPData += length - 2;

    _packet.RPSI.NumberOfValidBits =
        static_cast<uint16_t>(length - 2) * 8 - padding_bits;
    return true;
}

bool
RTCPParserV2::ParsePsfbAppItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if (*_ptrRTCPData++ != 'R')
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if (*_ptrRTCPData++ != 'E')
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if (*_ptrRTCPData++ != 'M')
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if (*_ptrRTCPData++ != 'B')
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packetType = kPsfbRemb;
    _state = State_PSFB_REMBItem;
    return true;
}

bool
RTCPParserV2::ParsePsfbREMBItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packet.REMBItem.NumberOfSSRCs = *_ptrRTCPData++;
    const uint8_t brExp = (_ptrRTCPData[0] >> 2) & 0x3F;

    uint32_t brMantissa = (_ptrRTCPData[0] & 0x03) << 16;
    brMantissa += (_ptrRTCPData[1] << 8);
    brMantissa += (_ptrRTCPData[2]);

    _ptrRTCPData += 3; // Fwd read data
    _packet.REMBItem.BitRate = (brMantissa << brExp);

    const ptrdiff_t length_ssrcs = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length_ssrcs < 4 * _packet.REMBItem.NumberOfSSRCs)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kPsfbRembItem;

    for (int i = 0; i < _packet.REMBItem.NumberOfSSRCs; i++)
    {
        _packet.REMBItem.SSRCs[i] = *_ptrRTCPData++ << 24;
        _packet.REMBItem.SSRCs[i] += *_ptrRTCPData++ << 16;
        _packet.REMBItem.SSRCs[i] += *_ptrRTCPData++ << 8;
        _packet.REMBItem.SSRCs[i] += *_ptrRTCPData++;
    }
    return true;
}
#endif
