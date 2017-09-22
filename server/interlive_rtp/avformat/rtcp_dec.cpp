/*
 * Author: gaosiyu@youku.com
 */

#include "rtcp.h"
#include <iostream>
using namespace std;
using namespace avformat;
#define LOG(sev) cout
 
/************************************************************************/
/* parser                                                               */
/************************************************************************/
namespace avformat
{
    bool RTCPParseCommonHeader(const uint8_t* ptrDataBegin,
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
        _packetType(kInvalid)
    {
        Validate();
    }

    RTCPParserV2::~RTCPParserV2()
    {
    }

    ptrdiff_t RTCPParserV2::LengthLeft() const
    {
        return (_ptrRTCPDataEnd - _ptrRTCPData);
    }

    RTCPPacketTypes RTCPParserV2::PacketType() const
    {
        return _packetType;
    }

    const RTCPPacket& RTCPParserV2::Packet() const
    {
        return _packet;
    }

    RTCPPacketTypes RTCPParserV2::Begin()
    {
        _ptrRTCPData = _ptrRTCPDataBegin;

        return Iterate();
    }

    RTCPPacketTypes RTCPParserV2::Iterate()
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
            case State_RTPFB_NACKItem:
                IterateNACKItem();
                break;
            case State_AppItem:
                IterateAppItem();
                break;
            case State_PSFB_AppItem:
                IteratePsfbAppItem();
                break;
            case State_PSFB_REMBItem:
                IteratePsfbREMBItem();
                break;
            default:
                assert(false); // Invalid state!
                break;
            }
        }
        return _packetType;
    }

    void RTCPParserV2::IterateTopLevel()
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

            default:
                // Not supported! Skip!
                EndCurrentBlock();
                break;
            }
        }
    }

    void RTCPParserV2::IterateReportBlockItem()
    {
        const bool success = ParseReportBlockItem();
        if (!success)
        {
            Iterate();
        }
    }

    void RTCPParserV2::IterateSDESChunk()
    {
        const bool success = ParseSDESChunk();
        if (!success)
        {
            Iterate();
        }
    }

    void RTCPParserV2::IterateBYEItem()
    {
        const bool success = ParseBYEItem();
        if (!success)
        {
            Iterate();
        }
    }

    void RTCPParserV2::IterateNACKItem()
    {
        const bool success = ParseNACKItem();
        if (!success)
        {
            Iterate();
        }
    }

    void RTCPParserV2::IterateAppItem()
    {
        const bool success = ParseAPPItem();
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

    void RTCPParserV2::Validate()
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

    bool RTCPParserV2::IsValid() const
    {
        return _validPacket;
    }

    void RTCPParserV2::EndCurrentBlock()
    {
        _ptrRTCPData = _ptrRTCPBlockEnd;
    }



    bool RTCPParserV2::ParseRR()
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

    bool RTCPParserV2::ParseSR()
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

    bool RTCPParserV2::ParseReportBlockItem()
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

    bool RTCPParserV2::ParseSDES()
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

    bool RTCPParserV2::ParseSDESChunk()
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

    bool RTCPParserV2::ParseSDESItem()
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

    bool RTCPParserV2::ParseBYE()
    {
        _ptrRTCPData += 4; // Skip header

        _state = State_BYEItem;

        return ParseBYEItem();
    }

    bool RTCPParserV2::ParseBYEItem()
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


    bool RTCPParserV2::ParseFBCommon(const RTCPCommonHeader& header)
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
            default:
                break;
            }
            EndCurrentBlock();
            return false;
        }
        else if (header.PT == PT_PSFB) {
            switch (header.IC)
            {
            case 15:
            {
                       _packetType = kPsfbApp;
                       _packet.PSFBAPP.SenderSSRC = senderSSRC;
                       _packet.PSFBAPP.MediaSSRC = mediaSSRC;

                       _state = State_PSFB_AppItem;
                       return true;
            }
            default:
                break;
            }
            EndCurrentBlock();
            return false;
        }
        else
        {
            assert(false);

            EndCurrentBlock();
            return false;
        }
    }

    bool RTCPParserV2::ParseNACKItem()
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



    bool RTCPParserV2::ParseAPP(const RTCPCommonHeader& header)
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

    bool RTCPParserV2::ParseAPPItem()
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

    RTCPPacketIterator::RTCPPacketIterator(uint8_t* rtcpData, size_t rtcpDataLength)
        : _ptrBegin(rtcpData),
        _ptrEnd(rtcpData + rtcpDataLength),
        _ptrBlock(NULL)
    {
        memset(&_header, 0, sizeof(_header));
    }

    RTCPPacketIterator::~RTCPPacketIterator()
    {
    }

    const RTCPCommonHeader* RTCPPacketIterator::Begin()
    {
        _ptrBlock = _ptrBegin;

        return Iterate();
    }

    const RTCPCommonHeader* RTCPPacketIterator::Iterate()
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

    const RTCPCommonHeader* RTCPPacketIterator::Current()
    {
        if (!_ptrBlock)
        {
            return NULL;
        }

        return &_header;
    }

    RTCPPacketInformation::RTCPPacketInformation()
        : rtcpPacketTypeFlags(0),
        remoteSSRC(0),
        applicationSubType(0),
        applicationName(0),
        applicationData(NULL),
        applicationLength(0),
        rtt(0),
        interArrivalJitter(0),
        sliPictureId(0),
        rpsiPictureId(0),
        receiverEstimatedMaxBitrate(0),
        ntp_secs(0),
        ntp_frac(0),
        rtp_timestamp(0),
        xr_dlrr_item(false)
    {

    }

    void RTCPPacketInformation::reset() {
        rtcpPacketTypeFlags = 0;
        remoteSSRC = 0;
        applicationSubType = 0;
        applicationName = 0;
        if (applicationData)
        {
            delete[] applicationData;
        }
        applicationData = NULL;
        applicationLength = 0;
        rtt = 0;
        interArrivalJitter = 0;
        sliPictureId = 0;
        rpsiPictureId = 0;
        receiverEstimatedMaxBitrate = 0;
        ntp_secs = 0;
        ntp_frac = 0;
        rtp_timestamp = 0;
        xr_dlrr_item = false;
        report_blocks.clear();
        ResetNACKPacketIdArray();
        memset(cnameInfo.name, 0, RTCP_CNAME_SIZE);
    }

    RTCPPacketInformation::~RTCPPacketInformation()
    {
        report_blocks.clear();
        if (applicationData)
        {
            delete[] applicationData;
        }
    }

    void RTCPPacketInformation::AddApplicationData(const uint8_t* data,
        const uint16_t size)
    {
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

    void RTCPPacketInformation::ResetNACKPacketIdArray()
    {
        nackList.clear();
    }

    void RTCPPacketInformation::AddNACKPacket(const uint16_t packetID)
    {
        if (nackList.size() >= kSendSideNackListSizeSanity) {
            return;
        }
        nackList.push_back(packetID);
    }

    void RTCPPacketInformation::AddReportInfo(
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
        readyForDelete(false)
    {
    }

    RTCPReceiveInformation::~RTCPReceiveInformation()
    {
    }

}
