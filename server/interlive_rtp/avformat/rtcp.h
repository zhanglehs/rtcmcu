#pragma once
/*
 * Author: gaosiyu@youku.com
 */
#include <stdint.h>
#include "assert.h"
#include <vector>
#include <map>
#include <list>
#include <set>
#include <string>
#include <string.h>
#include <cstddef>

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#define  WEBRTC_LINUX 1
#endif

#define MAX_RTCP_PACKET_COUNT 5
#undef DISALLOW_ASSIGN
#define DISALLOW_ASSIGN(TypeName) \
    void operator=(const TypeName&)

// A macro to disallow the evil copy constructor and operator= functions
// This should be used in the private: declarations for a class.
#undef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName)    \
    TypeName(const TypeName&);                    \
    DISALLOW_ASSIGN(TypeName)

// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
#undef DISALLOW_IMPLICIT_CONSTRUCTORS
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
    TypeName();                                    \
    DISALLOW_COPY_AND_ASSIGN(TypeName)


#define RTCP_CNAME_SIZE 256    // RFC 3550 page 44, including null termination
#define IP_PACKET_SIZE 1500    // we assume ethernet

#define EXCLUSIVE_LOCK_FUNCTION(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(exclusive_lock_function(__VA_ARGS__))
#define UNLOCK_FUNCTION(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(unlock_function(__VA_ARGS__))
#define THREAD_ANNOTATION_ATTRIBUTE__(x)  // no-op
#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))
#define MIN_VIDEO_BW_MANAGEMENT_BITRATE   30


enum { RTCP_INTERVAL_AUDIO_MS = 5000 };
enum { kSendSideNackListSizeSanity = 20000 };
enum { RTCP_MIN_FRAME_LENGTH_MS = 17 };
enum { RTCP_RPSI_DATA_SIZE = 30 };
enum { MAX_NUMBER_OF_REMB_FEEDBACK_SSRCS = 255 };
enum { kRtcpAppCode_DATA_SIZE = 32 * 4 };

const double kMagicNtpFractionalUnit = 4.294967296E+9;
const uint32_t kNtpJan1970 = 2208988800UL;
const double kNtpFracPerMs = 4.294967296E6;



namespace avformat
{
    class RTCPReportBlockInformation;
    class RTCPReceiveInformation;
    struct RTCPReportBlock;
    typedef std::map<uint32_t, RTCPReportBlockInformation*> ReportBlockInfoMap;
    typedef std::map<uint32_t, ReportBlockInfoMap> ReportBlockMap;
    typedef std::map<uint32_t, RTCPReceiveInformation*> ReceivedInfoMap;
    typedef std::list<RTCPReportBlock> ReportBlockList;
    struct RTCPCnameInformation {
        char name[RTCP_CNAME_SIZE];
    };
    struct RTCPPacketRR {
        uint32_t SenderSSRC;
        uint8_t NumberOfReportBlocks;
    };
    struct RTCPPacketSR {
        uint32_t SenderSSRC;
        uint8_t NumberOfReportBlocks;

        // sender info
        uint32_t NTPMostSignificant;
        uint32_t NTPLeastSignificant;
        uint32_t RTPTimestamp;
        uint32_t SenderPacketCount;
        uint32_t SenderOctetCount;
    };
    struct RTCPPacketReportBlockItem {
        // report block
        uint32_t SSRC;
        uint8_t FractionLost;
        uint32_t CumulativeNumOfPacketsLost;
        uint32_t ExtendedHighestSequenceNumber;
        uint32_t Jitter;
        uint32_t LastSR;
        uint32_t DelayLastSR;
    };
    struct RTCPPacketSDESCName {
        // RFC3550
        uint32_t SenderSSRC;
        char CName[RTCP_CNAME_SIZE];
    };
    
    struct RTCPPacketBYE {
        uint32_t SenderSSRC;
    };
    struct RTCPPacketRTPFBNACK {
        uint32_t SenderSSRC;
        uint32_t MediaSSRC;
    };
    struct RTCPPacketRTPFBNACKItem {
        // RFC4585
        uint16_t PacketID;
        uint16_t BitMask;
    };

    // generic name APP
    struct RTCPPacketAPP {
        uint8_t SubType;
        uint32_t Name;
        uint8_t Data[kRtcpAppCode_DATA_SIZE];
        uint16_t Size;
    };

    struct RTCPPacketPSFBAPP {
        uint32_t SenderSSRC;
        uint32_t MediaSSRC;
    };

    struct RTCPPacketPSFBREMBItem {
        uint32_t BitRate;
        uint8_t NumberOfSSRCs;
        uint32_t SSRCs[MAX_NUMBER_OF_REMB_FEEDBACK_SSRCS];
    };

    union RTCPPacket
    {
        RTCPPacketRR RR;
        RTCPPacketSR SR;
        RTCPPacketReportBlockItem ReportBlockItem;

        RTCPPacketSDESCName CName;
        RTCPPacketBYE BYE;

        RTCPPacketRTPFBNACK NACK;
        RTCPPacketRTPFBNACKItem NACKItem;

        RTCPPacketAPP APP;
        RTCPPacketPSFBAPP PSFBAPP;
        RTCPPacketPSFBREMBItem REMBItem;
    
    };

    enum RTCPPacketTypes {
        kInvalid,

        // RFC3550
        kRr,
        kSr,
        kReportBlockItem,

        kSdes,
        kSdesChunk,
        kBye,

        // RFC4585
        kRtpfbNack,
        kRtpfbNackItem,
        kApp,
        kAppItem,
        kPsfbRemb,
        kPsfbRembItem,
        kPsfbApp,
    };

    struct RTCPRawPacket {
        const uint8_t* _ptrPacketBegin;
        const uint8_t* _ptrPacketEnd;
    };

    struct RTCPModRawPacket {
        uint8_t* _ptrPacketBegin;
        uint8_t* _ptrPacketEnd;
    };

    struct RTCPCommonHeader {
        uint8_t V;   // Version
        bool P;      // Padding
        uint8_t IC;  // Item count/subtype
        uint8_t PT;  // Packet Type
        uint16_t LengthInOctets;
    };

    struct RTCPReportBlock {
        RTCPReportBlock()
        : remoteSSRC(0), sourceSSRC(0), fractionLost(0), cumulativeLost(0),
        extendedHighSeqNum(0), jitter(0), lastSR(0),
        delaySinceLastSR(0) {}

        RTCPReportBlock(uint32_t remote_ssrc,
            uint32_t source_ssrc,
            uint8_t fraction_lost,
            uint32_t cumulative_lost,
            uint32_t extended_high_sequence_number,
            uint32_t jitter,
            uint32_t last_sender_report,
            uint32_t delay_since_last_sender_report)
            : remoteSSRC(remote_ssrc),
            sourceSSRC(source_ssrc),
            fractionLost(fraction_lost),
            cumulativeLost(cumulative_lost),
            extendedHighSeqNum(extended_high_sequence_number),
            jitter(jitter),
            lastSR(last_sender_report),
            delaySinceLastSR(delay_since_last_sender_report) {}

        // Fields as described by RFC 3550 6.4.2.
        uint32_t remoteSSRC;  // SSRC of sender of this report.
        uint32_t sourceSSRC;  // SSRC of the RTP packet sender.
        uint8_t fractionLost;
        uint32_t cumulativeLost;  // 24 bits valid.
        uint32_t extendedHighSeqNum;
        uint32_t jitter;
        uint32_t lastSR;
        uint32_t delaySinceLastSR;
    };

    struct RTCPSenderInfo
    {
        uint32_t NTPseconds;
        uint32_t NTPfraction;
        uint32_t RTPtimeStamp;
        uint32_t sendPacketCount;
        uint32_t sendOctetCount;
    };

    struct RtcpStatistics {
        RtcpStatistics()
        : fraction_lost(0),
        cumulative_lost(0),
        extended_max_sequence_number(0),
        jitter(0) {}

        uint8_t fraction_lost;
        uint32_t cumulative_lost;
        uint32_t extended_max_sequence_number;
        uint32_t jitter;
    };

    
    

    //: uint8_t
    enum RTCPPT  {
        PT_IJ = 195,
        PT_SR = 200,
        PT_RR = 201,
        PT_SDES = 202,
        PT_BYE = 203,
        PT_APP = 204,
        PT_RTPFB = 205,
        PT_PSFB = 206,
        PT_XR = 207
    };
    //: uint8_t

    enum { kCommonFbFmtLength = 12 };
    enum { kReportBlockLength = 24 };
    enum RTCPPacketType {
        kRtcpReport = 0x0001,
        kRtcpSr = 0x0002,
        kRtcpRr = 0x0004,
        kRtcpSdes = 0x0008,
        kRtcpBye = 0x0010,
        kRtcpPli = 0x0020,
        kRtcpNack = 0x0040,
        kRtcpFir = 0x0080,
        kRtcpTmmbr = 0x0100,
        kRtcpTmmbn = 0x0200,
        kRtcpSrReq = 0x0400,
        kRtcpXrVoipMetric = 0x0800,
        kRtcpApp = 0x1000,
        kRtcpSli = 0x4000,
        kRtcpRpsi = 0x8000,
        kRtcpRemb = 0x10000,
        kRtcpTransmissionTimeOffset = 0x20000,
        kRtcpXrReceiverReferenceTime = 0x40000,
        kRtcpXrDlrrReportBlock = 0x80000
    };
    class Dlrr;
    class RawPacket;
    class Rrtr;
    class VoipMetric;

    // Class for building RTCP packets.
    //
    //  Example:
    //  ReportBlock report_block;
    //  report_block.To(234)
    //  report_block.FractionLost(10);
    //
    //  ReceiverReport rr;
    //  rr.From(123);
    //  rr.WithReportBlock(&report_block)
    //
    //  Fir fir;
    //  fir.From(123);
    //  fir.To(234)
    //  fir.WithCommandSeqNum(123);
    //
    //  size_t length = 0;                     // Builds an intra frame request
    //  uint8_t packet[kPacketSize];           // with sequence number 123.
    //  fir.Build(packet, &length, kPacketSize);
    //
    //  RawPacket packet = fir.Build();        // Returns a RawPacket holding
    //                                         // the built rtcp packet.
    //
    //  rr.Append(&fir)                        // Builds a compound RTCP packet with
    //  RawPacket packet = rr.Build();         // a receiver report, report block
    //                                         // and fir message.

    class RtcpPacket {
    public:
        virtual ~RtcpPacket() {}

        void Append(RtcpPacket* packet);

        RawPacket Build() const;

        void Build(uint8_t* packet, size_t* length, size_t max_length) const;

    protected:
        RtcpPacket() : kHeaderLength(4) {}

        virtual void Create(
            uint8_t* packet, size_t* length, size_t max_length) const = 0;

        const size_t kHeaderLength;

    private:

        void CreateAndAddAppended(
            uint8_t* packet, size_t* length, size_t max_length) const;

        std::vector<RtcpPacket*> appended_packets_;
    };

    class Empty : public RtcpPacket {
    public:
        Empty() : RtcpPacket() { }

        virtual ~Empty() {}

    protected:
        void Create(uint8_t* packet,
            size_t* length,
            size_t max_length) const;

    private:
        DISALLOW_COPY_AND_ASSIGN(Empty);
    };

    // From RFC 3550, RTP: A Transport Protocol for Real-Time Applications.
    //
    // RTCP report block (RFC 3550).
    //
    //   0                   1                   2                   3
    //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    //  |                 SSRC_1 (SSRC of first source)                 |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  | fraction lost |       cumulative number of packets lost       |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |           extended highest sequence number received           |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                      interarrival jitter                      |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                         last SR (LSR)                         |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                   delay since last SR (DLSR)                  |
    //  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

    class ReportBlock {
    public:
        ReportBlock() {
            // TODO(asapersson): Consider adding a constructor to struct.
            memset(&report_block_, 0, sizeof(report_block_));
        }

        ~ReportBlock() {}

        void To(uint32_t ssrc) {
            report_block_.SSRC = ssrc;
        }
        void WithFractionLost(uint8_t fraction_lost) {
            report_block_.FractionLost = fraction_lost;
        }
        void WithCumulativeLost(uint32_t cumulative_lost) {
            report_block_.CumulativeNumOfPacketsLost = cumulative_lost;
        }
        void WithExtHighestSeqNum(uint32_t ext_highest_seq_num) {
            report_block_.ExtendedHighestSequenceNumber = ext_highest_seq_num;
        }
        void WithJitter(uint32_t jitter) {
            report_block_.Jitter = jitter;
        }
        void WithLastSr(uint32_t last_sr) {
            report_block_.LastSR = last_sr;
        }
        void WithDelayLastSr(uint32_t delay_last_sr) {
            report_block_.DelayLastSR = delay_last_sr;
        }

    private:
        friend class SenderReport;
        friend class ReceiverReport;
        RTCPPacketReportBlockItem report_block_;
    };

    // RTCP sender report (RFC 3550).
    //
    //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |V=2|P|    RC   |   PT=SR=200   |             length            |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                         SSRC of sender                        |
    //  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    //  |              NTP timestamp, most significant word             |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |             NTP timestamp, least significant word             |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                         RTP timestamp                         |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                     sender's packet count                     |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                      sender's octet count                     |
    //  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    //  |                         report block(s)                       |
    //  |                            ....                               |

    class SenderReport : public RtcpPacket {
    public:
        SenderReport() : RtcpPacket() {
            memset(&sr_, 0, sizeof(sr_));
        }

        virtual ~SenderReport() {}

        void From(uint32_t ssrc) {
            sr_.SenderSSRC = ssrc;
        }
        void WithNtpSec(uint32_t sec) {
            sr_.NTPMostSignificant = sec;
        }
        void WithNtpFrac(uint32_t frac) {
            sr_.NTPLeastSignificant = frac;
        }
        void WithRtpTimestamp(uint32_t rtp_timestamp) {
            sr_.RTPTimestamp = rtp_timestamp;
        }
        void WithPacketCount(uint32_t packet_count) {
            sr_.SenderPacketCount = packet_count;
        }
        void WithOctetCount(uint32_t octet_count) {
            sr_.SenderOctetCount = octet_count;
        }
        void WithReportBlock(ReportBlock* block);

    protected:
        void Create(uint8_t* packet,
            size_t* length,
            size_t max_length) const;

    private:
        enum { kMaxNumberOfReportBlocks = 0x1f };

        size_t BlockLength() const {
            const size_t kSrHeaderLength = 8;
            const size_t kSenderInfoLength = 20;
            return kSrHeaderLength + kSenderInfoLength +
                report_blocks_.size() * kReportBlockLength;
        }

        RTCPPacketSR sr_;
        std::vector<RTCPPacketReportBlockItem> report_blocks_;

        DISALLOW_COPY_AND_ASSIGN(SenderReport);
    };

    //
    // RTCP receiver report (RFC 3550).
    //
    //   0                   1                   2                   3
    //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |V=2|P|    RC   |   PT=RR=201   |             length            |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                     SSRC of packet sender                     |
    //  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    //  |                         report block(s)                       |
    //  |                            ....                               |

    class ReceiverReport : public RtcpPacket {
    public:
        ReceiverReport() : RtcpPacket() {
            memset(&rr_, 0, sizeof(rr_));
        }

        virtual ~ReceiverReport() {}

        void From(uint32_t ssrc) {
            rr_.SenderSSRC = ssrc;
        }
        void WithReportBlock(ReportBlock* block);

    protected:
        void Create(uint8_t* packet,
            size_t* length,
            size_t max_length) const;

    private:
        enum { kMaxNumberOfReportBlocks = 0x1f };

        size_t BlockLength() const {
            const size_t kRrHeaderLength = 8;
            return kRrHeaderLength + report_blocks_.size() * kReportBlockLength;
        }

        RTCPPacketRR rr_;
        std::vector<RTCPPacketReportBlockItem> report_blocks_;

        DISALLOW_COPY_AND_ASSIGN(ReceiverReport);
    };

    // Source Description (SDES) (RFC 3550).
    //
    //         0                   1                   2                   3
    //         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // header |V=2|P|    SC   |  PT=SDES=202  |             length            |
    //        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    // chunk  |                          SSRC/CSRC_1                          |
    //   1    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //        |                           SDES items                          |
    //        |                              ...                              |
    //        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    // chunk  |                          SSRC/CSRC_2                          |
    //   2    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //        |                           SDES items                          |
    //        |                              ...                              |
    //        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    //
    // Canonical End-Point Identifier SDES Item (CNAME)
    //
    //    0                   1                   2                   3
    //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //   |    CNAME=1    |     length    | user and domain name        ...
    //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    class Sdes : public RtcpPacket {
    public:
        Sdes() : RtcpPacket() {}

        virtual ~Sdes() {}

        void WithCName(uint32_t ssrc, std::string cname);

        struct Chunk {
            uint32_t ssrc;
            std::string name;
            int null_octets;
        };


    protected:
        void Create(uint8_t* packet,
            size_t* length,
            size_t max_length) const;

    private:
        enum { kMaxNumberOfChunks = 0x1f };

        size_t BlockLength() const;

        std::vector<Chunk> chunks_;

        DISALLOW_COPY_AND_ASSIGN(Sdes);
    };

    //
    // Bye packet (BYE) (RFC 3550).
    //
    //        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //       |V=2|P|    SC   |   PT=BYE=203  |             length            |
    //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //       |                           SSRC/CSRC                           |
    //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //       :                              ...                              :
    //       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    // (opt) |     length    |               reason for leaving            ...
    //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    class Bye : public RtcpPacket {
    public:
        Bye() : RtcpPacket() {
            memset(&bye_, 0, sizeof(bye_));
        }

        virtual ~Bye() {}

        void From(uint32_t ssrc) {
            bye_.SenderSSRC = ssrc;
        }
        void WithCsrc(uint32_t csrc);

    protected:
        void Create(uint8_t* packet,
            size_t* length,
            size_t max_length) const;

    private:
        enum { kMaxNumberOfCsrcs = 0x1f - 1 };

        size_t BlockLength() const {
            size_t source_count = 1 + csrcs_.size();
            return kHeaderLength + 4 * source_count;
        }

        RTCPPacketBYE bye_;
        std::vector<uint32_t> csrcs_;

        DISALLOW_COPY_AND_ASSIGN(Bye);
    };

    // Application-Defined packet (APP) (RFC 3550).
    //
    //   0                   1                   2                   3
    //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |V=2|P| subtype |   PT=APP=204  |             length            |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                           SSRC/CSRC                           |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                          name (ASCII)                         |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                   application-dependent data                ...
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    class App : public RtcpPacket {
    public:
        App()
            : RtcpPacket(),
            ssrc_(0) {
            memset(&app_, 0, sizeof(app_));
        }

        virtual ~App() {}

        void From(uint32_t ssrc) {
            ssrc_ = ssrc;
        }
        void WithSubType(uint8_t subtype) {
            assert(subtype <= 0x1f);
            app_.SubType = subtype;
        }
        void WithName(uint32_t name) {
            app_.Name = name;
        }
        void WithData(const uint8_t* data, uint16_t data_length) {
            assert(data);
            assert(data_length <= kRtcpAppCode_DATA_SIZE);
            assert(data_length % 4 == 0);
            memcpy(app_.Data, data, data_length);
            app_.Size = data_length;
        }

    protected:
        void Create(uint8_t* packet,
            size_t* length,
            size_t max_length) const;

    private:
        size_t BlockLength() const {
            return 12 + app_.Size;
        }

        uint32_t ssrc_;
        RTCPPacketAPP app_;

        DISALLOW_COPY_AND_ASSIGN(App);
    };

    // Generic NACK (RFC 4585).
    //
    // FCI:
    //    0                   1                   2                   3
    //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //   |            PID                |             BLP               |
    //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    class Nack : public RtcpPacket {
    public:
        Nack() : RtcpPacket() {
            memset(&nack_, 0, sizeof(nack_));
        }

        virtual ~Nack() {}

        void From(uint32_t ssrc) {
            nack_.SenderSSRC = ssrc;
        }
        void To(uint32_t ssrc) {
            nack_.MediaSSRC = ssrc;
        }
        void WithList(const uint16_t* nack_list, int length);

    protected:
        void Create(uint8_t* packet,
            size_t* length,
            size_t max_length) const;

    private:
        size_t BlockLength() const {
            size_t fci_length = 4 * nack_fields_.size();
            return kCommonFbFmtLength + fci_length;
        }

        RTCPPacketRTPFBNACK nack_;
        std::vector<RTCPPacketRTPFBNACKItem> nack_fields_;

        DISALLOW_COPY_AND_ASSIGN(Nack);
    };

    // Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
    //
    //    0                   1                   2                   3
    //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //   |V=2|P| FMT=15  |   PT=206      |             length            |
    //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //   |                  SSRC of packet sender                        |
    //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //   |                  SSRC of media source                         |
    //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //   |  Unique identifier 'R' 'E' 'M' 'B'                            |
    //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //   |  Num SSRC     | BR Exp    |  BR Mantissa                      |
    //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //   |   SSRC feedback                                               |
    //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //   |  ...

    class Remb : public RtcpPacket {
    public:
        Remb() : RtcpPacket() {
            memset(&remb_, 0, sizeof(remb_));
            memset(&remb_item_, 0, sizeof(remb_item_));
        }

        virtual ~Remb() {}

        void From(uint32_t ssrc) {
            remb_.SenderSSRC = ssrc;
        }
        void AppliesTo(uint32_t ssrc);

        void WithBitrateBps(uint32_t bitrate_bps) {
            remb_item_.BitRate = bitrate_bps;
        }

    protected:
        void Create(uint8_t* packet,
            size_t* length,
            size_t max_length) const;

    private:
        enum { kMaxNumberOfSsrcs = 0xff };

        size_t BlockLength() const {
            return (remb_item_.NumberOfSSRCs + 5) * 4;
        }

        RTCPPacketPSFBAPP remb_;
        RTCPPacketPSFBREMBItem remb_item_;

        DISALLOW_COPY_AND_ASSIGN(Remb);
    };

    // Class holding a RTCP packet.
    //
    // Takes a built rtcp packet.
    //  RawPacket raw_packet(buffer, length);
    //
    // To access the raw packet:
    //  raw_packet.buffer();         - pointer to the raw packet
    //  raw_packet.buffer_length();  - the length of the raw packet

    class RawPacket {
    public:
        RawPacket(const uint8_t* packet, size_t length) {
            assert(length <= IP_PACKET_SIZE);
            memcpy(buffer_, packet, length);
            buffer_length_ = length;
        }

        const uint8_t* buffer() {
            return buffer_;
        }
        size_t buffer_length() const {
            return buffer_length_;
        }

    private:
        size_t buffer_length_;
        uint8_t buffer_[IP_PACKET_SIZE];
    };

    
    class RTCPReportBlockInformation
    {
    public:
        RTCPReportBlockInformation();
        ~RTCPReportBlockInformation();

        // Statistics
        RTCPReportBlock remoteReceiveBlock;
        uint32_t        remoteMaxJitter;

        // RTT
        int64_t  RTT;
        int64_t  minRTT;
        int64_t  maxRTT;
        int64_t  avgRTT;
        uint32_t numAverageCalcs;
    };


    class RTCPPacketInformation
    {
    public:
        RTCPPacketInformation();
        ~RTCPPacketInformation();


        void AddApplicationData(const uint8_t* data,
            const uint16_t size);

        void AddNACKPacket(const uint16_t packetID);
        void ResetNACKPacketIdArray();
        void AddReportInfo(const RTCPReportBlockInformation& report_block_info);
        void reset();

        uint32_t  rtcpPacketTypeFlags; // RTCPPacketTypeFlags bit field
        uint32_t  remoteSSRC;

        //std::list<uint16_t> nackSequenceNumbers;
        //uint16_t *nackSequenceNumbers;
        //uint32_t nackSequenceNumbersCount;
        std::vector<uint16_t> nackList;

        uint8_t   applicationSubType;
        uint32_t  applicationName;
        uint8_t*  applicationData;
        uint16_t  applicationLength;

        ReportBlockList report_blocks;
        int64_t rtt;

        uint32_t  interArrivalJitter;

        uint8_t   sliPictureId;
        uint64_t  rpsiPictureId;
        uint32_t  receiverEstimatedMaxBitrate;

        uint32_t ntp_secs;
        uint32_t ntp_frac;
        uint32_t rtp_timestamp;

        uint32_t sendOctetCount;
        uint32_t sendPacketCount;

        RTCPCnameInformation cnameInfo;

        bool xr_dlrr_item;

    private:
        DISALLOW_COPY_AND_ASSIGN(RTCPPacketInformation);
    };



    class RTCPReceiveInformation
    {
    public:
        RTCPReceiveInformation();
        ~RTCPReceiveInformation();

        int64_t lastTimeReceived;

        // FIR
        int32_t lastFIRSequenceNumber;
        int64_t lastFIRRequest;

        bool            readyForDelete;
    private:
    };
/************************************************************************/
/*    parser                                                            */
/************************************************************************/
    bool RTCPParseCommonHeader(const uint8_t* ptrDataBegin,
        const uint8_t* ptrDataEnd,
        RTCPCommonHeader& parsedHeader);

    class RTCPParserV2 {
    public:
        RTCPParserV2(
            const uint8_t* rtcpData,
            size_t rtcpDataLength,
            bool rtcpReducedSizeEnable);  // Set to true, to allow non-compound RTCP!
        ~RTCPParserV2();

        RTCPPacketTypes PacketType() const;
        const RTCPPacket& Packet() const;
        const RTCPRawPacket& RawPacket() const;
        ptrdiff_t LengthLeft() const;

        bool IsValid() const;

        RTCPPacketTypes Begin();
        RTCPPacketTypes Iterate();

    private:
        enum  ParseState {
            State_TopLevel,            // Top level packet
            State_ReportBlockItem,     // SR/RR report block
            State_SDESChunk,           // SDES chunk
            State_BYEItem,             // BYE item
            State_RTPFB_NACKItem,      // NACK FCI item
            State_AppItem,
            State_PSFB_AppItem,
            State_PSFB_REMBItem
        };

    private:
        void IterateTopLevel();
        void IterateReportBlockItem();
        void IterateSDESChunk();
        void IterateBYEItem();
        void IterateNACKItem();
        void IterateAppItem();
        void IteratePsfbAppItem();
        void IteratePsfbREMBItem();

        void Validate();
        void EndCurrentBlock();

        bool ParseRR();
        bool ParseSR();
        bool ParseReportBlockItem();

        bool ParseSDES();
        bool ParseSDESChunk();
        bool ParseSDESItem();

        bool ParseBYE();
        bool ParseBYEItem();

        bool ParseNACKItem();

        bool ParseAPP(const RTCPCommonHeader& header);
        bool ParseAPPItem();

        bool ParsePsfbAppItem();
        bool ParsePsfbREMBItem();

        bool ParseFBCommon(const RTCPCommonHeader& header);

    private:
        const uint8_t* const _ptrRTCPDataBegin;
        const bool _RTCPReducedSizeEnable;
        const uint8_t* const _ptrRTCPDataEnd;

        bool _validPacket;
        const uint8_t* _ptrRTCPData;
        const uint8_t* _ptrRTCPBlockEnd;

        ParseState _state;
        uint8_t _numberOfBlocks;

        RTCPPacketTypes _packetType;
        RTCPPacket _packet;
    };

    class RTCPPacketIterator {
    public:
        RTCPPacketIterator(uint8_t* rtcpData, size_t rtcpDataLength);
        ~RTCPPacketIterator();

        const RTCPCommonHeader* Begin();
        const RTCPCommonHeader* Iterate();
        const RTCPCommonHeader* Current();

    private:
        uint8_t* const _ptrBegin;
        uint8_t* const _ptrEnd;

        uint8_t* _ptrBlock;

        RTCPCommonHeader _header;
    };

    inline bool IsNewerSequenceNumber(uint16_t sequence_number,
        uint16_t prev_sequence_number) {
        return sequence_number != prev_sequence_number &&
            static_cast<uint16_t>(sequence_number - prev_sequence_number) < 0x8000;
    }
    
    class Clock {
    public:
        virtual ~Clock() {}

        //// Return a timestamp in milliseconds relative to some arbitrary source; the
        //// source is fixed for this clock.
        virtual int64_t TimeInMilliseconds() const = 0;

        //// Return a timestamp in microseconds relative to some arbitrary source; the
        //// source is fixed for this clock.
        virtual int64_t TimeInMicroseconds() const = 0;

        //// Retrieve an NTP absolute timestamp in seconds and fractions of a second.
        virtual void CurrentNtp(uint32_t& seconds, uint32_t& fractions) const = 0;

        //// Retrieve an NTP absolute timestamp in milliseconds.
        virtual int64_t CurrentNtpInMilliseconds() const = 0;

        // Converts an NTP timestamp to a millisecond timestamp.
        static int64_t NtpToMs(uint32_t seconds, uint32_t fractions);

        // Returns an instance of the real-time system clock implementation.
        static avformat::Clock* GetRealTimeClock();
    };

    
    class RTCP //: public TMMBRHelp 
    {
    public:
        RTCP(Clock* clock);
        ~RTCP();
        RawPacket generate_rtcp_packet(RtcpPacket *rtcp_packet);
        void parse_rtcp_packet(const uint8_t *data, uint32_t len, RTCPPacketInformation& rtcpPacketInformation);
    protected:
        void UpdateReceiveInformation(
            RTCPReceiveInformation& receiveInformation);

        void HandleSenderReceiverReport(
            RTCPParserV2& rtcpParser,
            RTCPPacketInformation& rtcpPacketInformation);

        void HandleReportBlock(
            const RTCPPacket& rtcpPacket,
            RTCPPacketInformation& rtcpPacketInformation,
            uint32_t remoteSSRC);

        void HandleSDES(RTCPParserV2& rtcpParser,
            RTCPPacketInformation& rtcpPacketInformation);

        void HandleSDESChunk(RTCPParserV2& rtcpParser,
            RTCPPacketInformation& rtcpPacketInformation);

        void HandleNACK(RTCPParserV2& rtcpParser,
            RTCPPacketInformation& rtcpPacketInformation);

        void HandleNACKItem(const RTCPPacket& rtcpPacket,
            RTCPPacketInformation& rtcpPacketInformation);

        void HandleBYE(RTCPParserV2& rtcpParser,
            RTCPPacketInformation& rtcpPacketInformation);

        void HandleAPP(RTCPParserV2& rtcpParser,
            RTCPPacketInformation& rtcpPacketInformation);

        void HandleAPPItem(RTCPParserV2& rtcpParser,
            RTCPPacketInformation& rtcpPacketInformation);

        void HandlePsfbApp(RTCPParserV2& rtcpParser,
            RTCPPacketInformation& rtcpPacketInformation);
        void HandleREMBItem(
            RTCPParserV2& rtcpParser,
            RTCPPacketInformation& rtcpPacketInformation);

        //RTCPReportBlockInformation* GetReportBlockInformation(
        //    uint32_t remote_ssrc, uint32_t source_ssrc) const;

        //RTCPReportBlockInformation* CreateOrGetReportBlockInformation(
        //    uint32_t remote_ssrc, uint32_t source_ssrc);
        RTCPCnameInformation* CreateCnameInformation(uint32_t remoteSSRC);
    private:
        uint32_t main_ssrc_;
        uint32_t _remoteSSRC;
        uint32_t _lastReceivedSRNTPsecs;
        uint32_t _lastReceivedSRNTPfrac;

        ReportBlockMap _receivedReportBlockMap;
        Clock* const _clock;
    };

    void CreateHeader(uint8_t count_or_format,  
        uint8_t packet_type,
        size_t length,
        uint8_t* buffer,
        size_t* pos);

    void AssignUWord8(uint8_t* buffer, size_t* offset, uint8_t value);
    void AssignUWord16(uint8_t* buffer, size_t* offset, uint16_t value);
    void AssignUWord24(uint8_t* buffer, size_t* offset, uint32_t value);
    void AssignUWord32(uint8_t* buffer, size_t* offset, uint32_t value);
}

