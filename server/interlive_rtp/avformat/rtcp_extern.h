#ifndef RTCP_HEADER
#define RTCP_HEADER
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

	struct RTCPPacketExtendedJitterReportItem {
		// RFC 5450
		uint32_t Jitter;
	};

	struct RTCPPacketBYE {
		uint32_t SenderSSRC;
	};
	struct RTCPPacketXR {
		// RFC 3611
		uint32_t OriginatorSSRC;
	};
	struct RTCPPacketXRReceiverReferenceTimeItem {
		// RFC 3611 4.4
		uint32_t NTPMostSignificant;
		uint32_t NTPLeastSignificant;
	};
	struct RTCPPacketXRDLRRReportBlockItem {
		// RFC 3611 4.5
		uint32_t SSRC;
		uint32_t LastRR;
		uint32_t DelayLastRR;
	};
	struct RTCPPacketXRVOIPMetricItem {
		// RFC 3611 4.7
		uint32_t SSRC;
		uint8_t lossRate;
		uint8_t discardRate;
		uint8_t burstDensity;
		uint8_t gapDensity;
		uint16_t burstDuration;
		uint16_t gapDuration;
		uint16_t roundTripDelay;
		uint16_t endSystemDelay;
		uint8_t signalLevel;
		uint8_t noiseLevel;
		uint8_t RERL;
		uint8_t Gmin;
		uint8_t Rfactor;
		uint8_t extRfactor;
		uint8_t MOSLQ;
		uint8_t MOSCQ;
		uint8_t RXconfig;
		uint16_t JBnominal;
		uint16_t JBmax;
		uint16_t JBabsMax;
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

	struct RTCPPacketRTPFBTMMBR {
		uint32_t SenderSSRC;
		uint32_t MediaSSRC;  // zero!
	};
	struct RTCPPacketRTPFBTMMBRItem {
		// RFC5104
		uint32_t SSRC;
		uint32_t MaxTotalMediaBitRate;  // In Kbit/s
		uint32_t MeasuredOverhead;
	};

	struct RTCPPacketRTPFBTMMBN {
		uint32_t SenderSSRC;
		uint32_t MediaSSRC;  // zero!
	};
	struct RTCPPacketRTPFBTMMBNItem {
		// RFC5104
		uint32_t SSRC;  // "Owner"
		uint32_t MaxTotalMediaBitRate;
		uint32_t MeasuredOverhead;
	};
#ifdef SLICE_FCI
	struct RTCPPacketPSFBFIR {
		uint32_t SenderSSRC;
		uint32_t MediaSSRC;  // zero!
	};
	struct RTCPPacketPSFBFIRItem {
		// RFC5104
		uint32_t SSRC;
		uint8_t CommandSequenceNumber;
	};

	struct RTCPPacketPSFBPLI {
		// RFC4585
		uint32_t SenderSSRC;
		uint32_t MediaSSRC;
	};

	struct RTCPPacketPSFBSLI {
		// RFC4585
		uint32_t SenderSSRC;
		uint32_t MediaSSRC;
	};
	struct RTCPPacketPSFBSLIItem {
		// RFC4585
		uint16_t FirstMB;
		uint16_t NumberOfMB;
		uint8_t PictureId;
	};
	struct RTCPPacketPSFBRPSI {
		// RFC4585
		uint32_t SenderSSRC;
		uint32_t MediaSSRC;
		uint8_t PayloadType;
		uint16_t NumberOfValidBits;
		uint8_t NativeBitString[RTCP_RPSI_DATA_SIZE];
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
#endif
	// generic name APP
	struct RTCPPacketAPP {
		uint8_t SubType;
		uint32_t Name;
		uint8_t Data[kRtcpAppCode_DATA_SIZE];
		uint16_t Size;
	};

	union RTCPPacket {
		RTCPPacketRR RR;
		RTCPPacketSR SR;
		RTCPPacketReportBlockItem ReportBlockItem;

		RTCPPacketSDESCName CName;
		RTCPPacketBYE BYE;

		RTCPPacketExtendedJitterReportItem ExtendedJitterReportItem;

		RTCPPacketRTPFBNACK NACK;
		RTCPPacketRTPFBNACKItem NACKItem;

#ifdef SLICE_FCI
		RTCPPacketPSFBPLI PLI;
		RTCPPacketPSFBSLI SLI;
		RTCPPacketPSFBSLIItem SLIItem;
		RTCPPacketPSFBRPSI RPSI;
		RTCPPacketPSFBAPP PSFBAPP;
		RTCPPacketPSFBREMBItem REMBItem;
#endif

		RTCPPacketRTPFBTMMBR TMMBR;
		RTCPPacketRTPFBTMMBRItem TMMBRItem;
		RTCPPacketRTPFBTMMBN TMMBN;
		RTCPPacketRTPFBTMMBNItem TMMBNItem;
#ifdef SLICE_FCI
		RTCPPacketPSFBFIR FIR;
		RTCPPacketPSFBFIRItem FIRItem;
#endif
		RTCPPacketXR XR;
		RTCPPacketXRReceiverReferenceTimeItem XRReceiverReferenceTimeItem;
		RTCPPacketXRDLRRReportBlockItem XRDLRRReportBlockItem;
		RTCPPacketXRVOIPMetricItem XRVOIPMetricItem;

		RTCPPacketAPP APP;
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

		// RFC5450
		kExtendedIj,
		kExtendedIjItem,

		// RFC4585
		kRtpfbNack,
		kRtpfbNackItem,

		kPsfbPli,
		kPsfbRpsi,
		kPsfbSli,
		kPsfbSliItem,
		kPsfbApp,
		kPsfbRemb,
		kPsfbRembItem,

		// RFC5104
		kRtpfbTmmbr,
		kRtpfbTmmbrItem,
		kRtpfbTmmbn,
		kRtpfbTmmbnItem,
		kPsfbFir,
		kPsfbFirItem,

		// draft-perkins-avt-rapid-rtp-sync
		kRtpfbSrReq,

		// RFC 3611
		kXrHeader,
		kXrReceiverReferenceTime,
		kXrDlrrReportBlock,
		kXrDlrrReportBlockItem,
		kXrVoipMetric,

		kApp,
		kAppItem,
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

	struct RTCPVoIPMetric {
		// RFC 3611 4.7
		uint8_t lossRate;
		uint8_t discardRate;
		uint8_t burstDensity;
		uint8_t gapDensity;
		uint16_t burstDuration;
		uint16_t gapDuration;
		uint16_t roundTripDelay;
		uint16_t endSystemDelay;
		uint8_t signalLevel;
		uint8_t noiseLevel;
		uint8_t RERL;
		uint8_t Gmin;
		uint8_t Rfactor;
		uint8_t extRfactor;
		uint8_t MOSLQ;
		uint8_t MOSCQ;
		uint8_t RXconfig;
		uint16_t JBnominal;
		uint16_t JBmax;
		uint16_t JBabsMax;
	};

	struct RtcpReceiveTimeInfo {
		// Fields as described by RFC 3611 4.5.
		uint32_t sourceSSRC;
		uint32_t lastRR;
		uint32_t delaySinceLastRR;
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
	// Extended report blocks, RFC 3611.
	enum RtcpXrBlockType  {
		kBtReceiverReferenceTime = 4,
		kBtDlrr = 5,
		kBtVoipMetric = 7
	};

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

	// Transmission Time Offsets in RTP Streams (RFC 5450).
	//
	//      0                   1                   2                   3
	//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	// hdr |V=2|P|    RC   |   PT=IJ=195   |             length            |
	//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//     |                      inter-arrival jitter                     |
	//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//     .                                                               .
	//     .                                                               .
	//     .                                                               .
	//     |                      inter-arrival jitter                     |
	//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//
	//  If present, this RTCP packet must be placed after a receiver report
	//  (inside a compound RTCP packet), and MUST have the same value for RC
	//  (reception report count) as the receiver report.

	class Ij : public RtcpPacket {
	public:
		Ij() : RtcpPacket() {}

		virtual ~Ij() {}

		void WithJitterItem(uint32_t jitter);

	protected:
		void Create(uint8_t* packet,
			size_t* length,
			size_t max_length) const;

	private:
		enum { kMaxNumberOfIjItems = 0x1f };

		size_t BlockLength() const {
			return kHeaderLength + 4 * ij_items_.size();
		}

		std::vector<uint32_t> ij_items_;

		DISALLOW_COPY_AND_ASSIGN(Ij);
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

	// Temporary Maximum Media Stream Bit Rate Request (TMMBR) (RFC 5104).
	//
	// FCI:
	//
	//    0                   1                   2                   3
	//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   |                              SSRC                             |
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   | MxTBR Exp |  MxTBR Mantissa                 |Measured Overhead|
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	class Tmmbr : public RtcpPacket {
	public:
		Tmmbr() : RtcpPacket() {
			memset(&tmmbr_, 0, sizeof(tmmbr_));
			memset(&tmmbr_item_, 0, sizeof(tmmbr_item_));
		}

		virtual ~Tmmbr() {}

		void From(uint32_t ssrc) {
			tmmbr_.SenderSSRC = ssrc;
		}
		void To(uint32_t ssrc) {
			tmmbr_item_.SSRC = ssrc;
		}
		void WithBitrateKbps(uint32_t bitrate_kbps) {
			tmmbr_item_.MaxTotalMediaBitRate = bitrate_kbps;
		}
		void WithOverhead(uint16_t overhead) {
			assert(overhead <= 0x1ff);
			tmmbr_item_.MeasuredOverhead = overhead;
		}

	protected:
		void Create(uint8_t* packet,
			size_t* length,
			size_t max_length) const;

	private:
		size_t BlockLength() const {
			const size_t kFciLen = 8;
			return kCommonFbFmtLength + kFciLen;
		}

		RTCPPacketRTPFBTMMBR tmmbr_;
		RTCPPacketRTPFBTMMBRItem tmmbr_item_;

		DISALLOW_COPY_AND_ASSIGN(Tmmbr);
	};

	// Temporary Maximum Media Stream Bit Rate Notification (TMMBN) (RFC 5104).
	//
	// FCI:
	//
	//    0                   1                   2                   3
	//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   |                              SSRC                             |
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   | MxTBR Exp |  MxTBR Mantissa                 |Measured Overhead|
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	class Tmmbn : public RtcpPacket {
	public:
		Tmmbn() : RtcpPacket() {
			memset(&tmmbn_, 0, sizeof(tmmbn_));
		}

		virtual ~Tmmbn() {}

		void From(uint32_t ssrc) {
			tmmbn_.SenderSSRC = ssrc;
		}
		void WithTmmbr(uint32_t ssrc, uint32_t bitrate_kbps, uint16_t overhead);

	protected:
		void Create(uint8_t* packet,
			size_t* length,
			size_t max_length) const;

	private:
		enum { kMaxNumberOfTmmbrs = 50 };

		size_t BlockLength() const {
			const size_t kFciLen = 8;
			return kCommonFbFmtLength + kFciLen * tmmbn_items_.size();
		}

		RTCPPacketRTPFBTMMBN tmmbn_;
		std::vector<RTCPPacketRTPFBTMMBRItem> tmmbn_items_;

		DISALLOW_COPY_AND_ASSIGN(Tmmbn);
	};

	// From RFC 3611: RTP Control Protocol Extended Reports (RTCP XR).
	//
	// Format for XR packets:
	//
	//   0                   1                   2                   3
	//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |V=2|P|reserved |   PT=XR=207   |             length            |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |                              SSRC                             |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  :                         report blocks                         :
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	class Xr : public RtcpPacket {
	public:
		typedef std::vector<RTCPPacketXRDLRRReportBlockItem> DlrrBlock;
		Xr() : RtcpPacket() {
			memset(&xr_header_, 0, sizeof(xr_header_));
		}

		virtual ~Xr() {}

		void From(uint32_t ssrc) {
			xr_header_.OriginatorSSRC = ssrc;
		}
		void WithRrtr(Rrtr* rrtr);
		void WithDlrr(Dlrr* dlrr);
		void WithVoipMetric(VoipMetric* voip_metric);

	protected:
		void Create(uint8_t* packet,
			size_t* length,
			size_t max_length) const;

	private:
		enum { kMaxNumberOfRrtrBlocks = 50 };
		enum { kMaxNumberOfDlrrBlocks = 50 };
		enum { kMaxNumberOfVoipMetricBlocks = 50 };

		size_t BlockLength() const {
			const size_t kXrHeaderLength = 8;
			return kXrHeaderLength + RrtrLength() + DlrrLength() + VoipMetricLength();
		}

		size_t RrtrLength() const {
			const size_t kRrtrBlockLength = 12;
			return kRrtrBlockLength * rrtr_blocks_.size();
		}

		size_t DlrrLength() const;

		size_t VoipMetricLength() const {
			const size_t kVoipMetricBlockLength = 36;
			return kVoipMetricBlockLength * voip_metric_blocks_.size();
		}

		RTCPPacketXR xr_header_;
		std::vector<RTCPPacketXRReceiverReferenceTimeItem> rrtr_blocks_;
		std::vector<DlrrBlock> dlrr_blocks_;
		std::vector<RTCPPacketXRVOIPMetricItem> voip_metric_blocks_;

		DISALLOW_COPY_AND_ASSIGN(Xr);
	};

	// Receiver Reference Time Report Block (RFC 3611).
	//
	//   0                   1                   2                   3
	//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |     BT=4      |   reserved    |       block length = 2        |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |              NTP timestamp, most significant word             |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |             NTP timestamp, least significant word             |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	class Rrtr {
	public:
		Rrtr() {
			memset(&rrtr_block_, 0, sizeof(rrtr_block_));
		}
		~Rrtr() {}

		void WithNtpSec(uint32_t sec) {
			rrtr_block_.NTPMostSignificant = sec;
		}
		void WithNtpFrac(uint32_t frac) {
			rrtr_block_.NTPLeastSignificant = frac;
		}

	private:
		friend class Xr;
		RTCPPacketXRReceiverReferenceTimeItem rrtr_block_;

		DISALLOW_COPY_AND_ASSIGN(Rrtr);
	};

	// DLRR Report Block (RFC 3611).
	//
	//   0                   1                   2                   3
	//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |     BT=5      |   reserved    |         block length          |
	//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	//  |                 SSRC_1 (SSRC of first receiver)               | sub-
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
	//  |                         last RR (LRR)                         |   1
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |                   delay since last RR (DLRR)                  |
	//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	//  |                 SSRC_2 (SSRC of second receiver)              | sub-
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
	//  :                               ...                             :   2

	class Dlrr {
	public:
		Dlrr() {}
		~Dlrr() {}

		void WithDlrrItem(uint32_t ssrc, uint32_t last_rr, uint32_t delay_last_rr);

	private:
		friend class Xr;
		enum { kMaxNumberOfDlrrItems = 100 };

		std::vector<RTCPPacketXRDLRRReportBlockItem> dlrr_block_;

		DISALLOW_COPY_AND_ASSIGN(Dlrr);
	};

	// VoIP Metrics Report Block (RFC 3611).
	//
	//   0                   1                   2                   3
	//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |     BT=7      |   reserved    |       block length = 8        |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |                        SSRC of source                         |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |   loss rate   | discard rate  | burst density |  gap density  |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |       burst duration          |         gap duration          |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |     round trip delay          |       end system delay        |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  | signal level  |  noise level  |     RERL      |     Gmin      |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |   R factor    | ext. R factor |    MOS-LQ     |    MOS-CQ     |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |   RX config   |   reserved    |          JB nominal           |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |          JB maximum           |          JB abs max           |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	class VoipMetric {
	public:
		VoipMetric() {
			memset(&metric_, 0, sizeof(metric_));
		}
		~VoipMetric() {}

		void To(uint32_t ssrc) { metric_.SSRC = ssrc; }
		void LossRate(uint8_t loss_rate) { metric_.lossRate = loss_rate; }
		void DiscardRate(uint8_t discard_rate) { metric_.discardRate = discard_rate; }
		void BurstDensity(uint8_t burst_density) {
			metric_.burstDensity = burst_density;
		}
		void GapDensity(uint8_t gap_density) { metric_.gapDensity = gap_density; }
		void BurstDuration(uint16_t burst_duration) {
			metric_.burstDuration = burst_duration;
		}
		void GapDuration(uint16_t gap_duration) {
			metric_.gapDuration = gap_duration;
		}
		void RoundTripDelay(uint16_t round_trip_delay) {
			metric_.roundTripDelay = round_trip_delay;
		}
		void EndSystemDelay(uint16_t end_system_delay) {
			metric_.endSystemDelay = end_system_delay;
		}
		void SignalLevel(uint8_t signal_level) { metric_.signalLevel = signal_level; }
		void NoiseLevel(uint8_t noise_level) { metric_.noiseLevel = noise_level; }
		void Rerl(uint8_t rerl) { metric_.RERL = rerl; }
		void Gmin(uint8_t gmin) { metric_.Gmin = gmin; }
		void Rfactor(uint8_t rfactor) { metric_.Rfactor = rfactor; }
		void ExtRfactor(uint8_t extrfactor) { metric_.extRfactor = extrfactor; }
		void MosLq(uint8_t moslq) { metric_.MOSLQ = moslq; }
		void MosCq(uint8_t moscq) { metric_.MOSCQ = moscq; }
		void RxConfig(uint8_t rxconfig) { metric_.RXconfig = rxconfig; }
		void JbNominal(uint16_t jbnominal) { metric_.JBnominal = jbnominal; }
		void JbMax(uint16_t jbmax) { metric_.JBmax = jbmax; }
		void JbAbsMax(uint16_t jbabsmax) { metric_.JBabsMax = jbabsmax; }

	private:
		friend class Xr;
		RTCPPacketXRVOIPMetricItem metric_;

		DISALLOW_COPY_AND_ASSIGN(VoipMetric);
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

	
	class NackStats {
	public:
		NackStats();
		~NackStats();

		// Updates stats with requested sequence number.
		// This function should be called for each NACK request to calculate the
		// number of unique NACKed RTP packets.
		void ReportRequest(uint16_t sequence_number);

		// Gets the number of NACKed RTP packets.
		uint32_t requests() const { return requests_; }

		// Gets the number of unique NACKed RTP packets.
		uint32_t unique_requests() const { return unique_requests_; }

	private:
		uint16_t max_sequence_number_;
		uint32_t requests_;
		uint32_t unique_requests_;
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


	class TMMBRSet
	{
	public:
		TMMBRSet();
		~TMMBRSet();

		void VerifyAndAllocateSet(uint32_t minimumSize);
		void VerifyAndAllocateSetKeepingData(uint32_t minimumSize);
		// Number of valid data items in set.
		uint32_t lengthOfSet() const { return _lengthOfSet; }
		// Presently allocated max size of set.
		uint32_t sizeOfSet() const { return _sizeOfSet; }
		void clearSet() {
			_lengthOfSet = 0;
		}
		uint32_t Tmmbr(int i) const {
			return _data.at(i).tmmbr;
		}
		uint32_t PacketOH(int i) const {
			return _data.at(i).packet_oh;
		}
		uint32_t Ssrc(int i) const {
			return _data.at(i).ssrc;
		}
		void SetEntry(unsigned int i,
			uint32_t tmmbrSet,
			uint32_t packetOHSet,
			uint32_t ssrcSet);

		void AddEntry(uint32_t tmmbrSet,
			uint32_t packetOHSet,
			uint32_t ssrcSet);

		// Remove one entry from table, and move all others down.
		void RemoveEntry(uint32_t sourceIdx);

		void SwapEntries(uint32_t firstIdx,
			uint32_t secondIdx);

		// Set entry data to zero, but keep it in table.
		void ClearEntry(uint32_t idx);

	private:
		class SetElement {
		public:
			SetElement() : tmmbr(0), packet_oh(0), ssrc(0) {}
			uint32_t tmmbr;
			uint32_t packet_oh;
			uint32_t ssrc;
		};

		std::vector<SetElement> _data;
		// Number of places allocated.
		uint32_t    _sizeOfSet;
		// NUmber of places currently in use.
		uint32_t    _lengthOfSet;
	};

	class RTCPPacketInformation
	{
	public:
		RTCPPacketInformation();
		~RTCPPacketInformation();

		void AddVoIPMetric(const RTCPVoIPMetric*  metric);

		void AddApplicationData(const uint8_t* data,
			const uint16_t size);

		void AddNACKPacket(const uint16_t packetID);
		void ResetNACKPacketIdArray();

		void AddReportInfo(const RTCPReportBlockInformation& report_block_info);

		uint32_t  rtcpPacketTypeFlags; // RTCPPacketTypeFlags bit field
		uint32_t  remoteSSRC;

		std::list<uint16_t> nackSequenceNumbers;

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

		uint32_t xr_originator_ssrc;
		bool xr_dlrr_item;
		RTCPVoIPMetric*  VoIPMetric;

	private:
		DISALLOW_COPY_AND_ASSIGN(RTCPPacketInformation);
	};



	class RTCPReceiveInformation
	{
	public:
		RTCPReceiveInformation();
		~RTCPReceiveInformation();

		void VerifyAndAllocateBoundingSet(const uint32_t minimumSize);
		void VerifyAndAllocateTMMBRSet(const uint32_t minimumSize);

		void InsertTMMBRItem(const uint32_t senderSSRC,
			const RTCPPacketRTPFBTMMBRItem& TMMBRItem,
			const int64_t currentTimeMS);

		// get
		int32_t GetTMMBRSet(const uint32_t sourceIdx,
			const uint32_t targetIdx,
			TMMBRSet* candidateSet,
			const int64_t currentTimeMS);

		int64_t lastTimeReceived;

		// FIR
		int32_t lastFIRSequenceNumber;
		int64_t lastFIRRequest;

		// TMMBN
		TMMBRSet        TmmbnBoundingSet;

		// TMMBR
		TMMBRSet        TmmbrSet;

		bool            readyForDelete;
	private:
		std::vector<int64_t> _tmmbrSetTimeouts;
	};
#ifdef SLICE_FCI
	// RFC 4585: Feedback format.
	//
	// Common packet format:
	//
	//    0                   1                   2                   3
	//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   |V=2|P|   FMT   |       PT      |          length               |
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   |                  SSRC of packet sender                        |
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   |                  SSRC of media source                         |
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   :            Feedback Control Information (FCI)                 :
	//   :

	// Picture loss indication (PLI) (RFC 4585).
	//
	// FCI: no feedback control information.

	class Pli : public RtcpPacket {
	public:
		Pli() : RtcpPacket() {
			memset(&pli_, 0, sizeof(pli_));
		}

		virtual ~Pli() {}

		void From(uint32_t ssrc) {
			pli_.SenderSSRC = ssrc;
		}
		void To(uint32_t ssrc) {
			pli_.MediaSSRC = ssrc;
		}

	protected:
		void Create(uint8_t* packet,
			size_t* length,
			size_t max_length) const;

	private:
		size_t BlockLength() const {
			return kCommonFbFmtLength;
		}

		RTCPPacketPSFBPLI pli_;

		DISALLOW_COPY_AND_ASSIGN(Pli);
	};

	// Slice loss indication (SLI) (RFC 4585).
	//
	// FCI:
	//    0                   1                   2                   3
	//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   |            First        |        Number           | PictureID |
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	class Sli : public RtcpPacket {
	public:
		Sli() : RtcpPacket() {
			memset(&sli_, 0, sizeof(sli_));
			memset(&sli_item_, 0, sizeof(sli_item_));
		}

		virtual ~Sli() {}

		void From(uint32_t ssrc) {
			sli_.SenderSSRC = ssrc;
		}
		void To(uint32_t ssrc) {
			sli_.MediaSSRC = ssrc;
		}
		void WithFirstMb(uint16_t first_mb) {
			assert(first_mb <= 0x1fff);
			sli_item_.FirstMB = first_mb;
		}
		void WithNumberOfMb(uint16_t number_mb) {
			assert(number_mb <= 0x1fff);
			sli_item_.NumberOfMB = number_mb;
		}
		void WithPictureId(uint8_t picture_id) {
			assert(picture_id <= 0x3f);
			sli_item_.PictureId = picture_id;
		}

	protected:
		void Create(uint8_t* packet,
			size_t* length,
			size_t max_length) const;

	private:
		size_t BlockLength() const {
			const size_t kFciLength = 4;
			return kCommonFbFmtLength + kFciLength;
		}

		RTCPPacketPSFBSLI sli_;
		RTCPPacketPSFBSLIItem sli_item_;

		DISALLOW_COPY_AND_ASSIGN(Sli);
	};

	// Reference picture selection indication (RPSI) (RFC 4585).
	//
	// FCI:
	//
	//    0                   1                   2                   3
	//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   |      PB       |0| Payload Type|    Native RPSI bit string     |
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   |   defined per codec          ...                | Padding (0) |
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	class Rpsi : public RtcpPacket {
	public:
		Rpsi()
			: RtcpPacket(),
			padding_bytes_(0) {
			memset(&rpsi_, 0, sizeof(rpsi_));
		}

		virtual ~Rpsi() {}

		void From(uint32_t ssrc) {
			rpsi_.SenderSSRC = ssrc;
		}
		void To(uint32_t ssrc) {
			rpsi_.MediaSSRC = ssrc;
		}
		void WithPayloadType(uint8_t payload) {
			assert(payload <= 0x7f);
			rpsi_.PayloadType = payload;
		}
		void WithPictureId(uint64_t picture_id);

	protected:
		void Create(uint8_t* packet,
			size_t* length,
			size_t max_length) const;

	private:
		size_t BlockLength() const {
			size_t fci_length = 2 + (rpsi_.NumberOfValidBits / 8) + padding_bytes_;
			return kCommonFbFmtLength + fci_length;
		}

		uint8_t padding_bytes_;
		RTCPPacketPSFBRPSI rpsi_;

		DISALLOW_COPY_AND_ASSIGN(Rpsi);
	};

	// Full intra request (FIR) (RFC 5104).
	//
	// FCI:
	//
	//    0                   1                   2                   3
	//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   |                              SSRC                             |
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//   | Seq nr.       |    Reserved                                   |
	//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	class Fir : public RtcpPacket {
	public:
		Fir() : RtcpPacket() {
			memset(&fir_, 0, sizeof(fir_));
			memset(&fir_item_, 0, sizeof(fir_item_));
		}

		virtual ~Fir() {}

		void From(uint32_t ssrc) {
			fir_.SenderSSRC = ssrc;
		}
		void To(uint32_t ssrc) {
			fir_item_.SSRC = ssrc;
		}
		void WithCommandSeqNum(uint8_t seq_num) {
			fir_item_.CommandSequenceNumber = seq_num;
		}

	protected:
		void Create(uint8_t* packet,
			size_t* length,
			size_t max_length) const;

	private:
		size_t BlockLength() const {
			const size_t kFciLength = 8;
			return kCommonFbFmtLength + kFciLength;
		}

		RTCPPacketPSFBFIR fir_;
		RTCPPacketPSFBFIRItem fir_item_;
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
#endif
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
			State_ExtendedJitterItem,  // Extended jitter report item

			/************************************************************************/
			/* Feedback Control Information(FCI)                                    */
			/************************************************************************/
			State_RTPFB_NACKItem,      // NACK FCI item
			State_RTPFB_TMMBRItem,     // TMMBR FCI item
			State_RTPFB_TMMBNItem,     // TMMBN FCI item
#ifdef SLICE_FCI
			State_PSFB_SLIItem,        // SLI FCI item
			State_PSFB_RPSIItem,       // RPSI FCI item
			State_PSFB_FIRItem,        // FIR FCI item
			State_PSFB_AppItem,        // Application specific FCI item
			State_PSFB_REMBItem,       // Application specific REMB item
#endif
			State_XRItem,
			State_XR_DLLRItem,
			State_AppItem
		};

	private:
		void IterateTopLevel();
		void IterateReportBlockItem();
		void IterateSDESChunk();
		void IterateBYEItem();
		void IterateExtendedJitterItem();
		void IterateNACKItem();
		void IterateTMMBRItem();
		void IterateTMMBNItem();
#ifdef SLICE_FCI
		void IterateSLIItem();
		void IterateRPSIItem();
		void IterateFIRItem();
		void IteratePsfbAppItem();
		void IteratePsfbREMBItem();
#endif

		void IterateAppItem();
		void IterateXrItem();
		void IterateXrDlrrItem();

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

		bool ParseIJ();
		bool ParseIJItem();

		bool ParseXr();
		bool ParseXrItem();
		bool ParseXrReceiverReferenceTimeItem(int block_length_4bytes);
		bool ParseXrDlrr(int block_length_4bytes);
		bool ParseXrDlrrItem();
		bool ParseXrVoipMetricItem(int block_length_4bytes);
		bool ParseXrUnsupportedBlockType(int block_length_4bytes);

		bool ParseFBCommon(const RTCPCommonHeader& header);
		bool ParseNACKItem();
		bool ParseTMMBRItem();
		bool ParseTMMBNItem();
#ifdef SLICE_FCI
		bool ParseSLIItem();
		bool ParseRPSIItem();
		bool ParseFIRItem();
		bool ParsePsfbAppItem();
		bool ParsePsfbREMBItem();
#endif
		bool ParseAPP(const RTCPCommonHeader& header);
		bool ParseAPPItem();

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

	class CriticalSectionWrapper {
	public:
		// Factory method, constructor disabled
		static CriticalSectionWrapper* CreateCriticalSection();

		virtual ~CriticalSectionWrapper() {}

		// Tries to grab lock, beginning of a critical section. Will wait for the
		// lock to become available if the grab failed.
		virtual void Enter() EXCLUSIVE_LOCK_FUNCTION() = 0;

		// Returns a grabbed lock, end of critical section.
		virtual void Leave() UNLOCK_FUNCTION() = 0;
	};

	// RAII extension of the critical section. Prevents Enter/Leave mismatches and
	// provides more compact critical section syntax.
	class  CriticalSectionScoped {
	public:
		explicit CriticalSectionScoped(CriticalSectionWrapper* critsec)
			EXCLUSIVE_LOCK_FUNCTION(critsec)
			: ptr_crit_sec_(critsec) {
				ptr_crit_sec_->Enter();
			}

		~CriticalSectionScoped() UNLOCK_FUNCTION() { ptr_crit_sec_->Leave(); }

	private:
		CriticalSectionWrapper* ptr_crit_sec_;
	};

	class TMMBRHelp
	{
	public:
		TMMBRHelp();
		virtual ~TMMBRHelp();

		TMMBRSet* BoundingSet(); // used for debuging
		TMMBRSet* CandidateSet();
		TMMBRSet* BoundingSetToSend();

		TMMBRSet* VerifyAndAllocateCandidateSet(const uint32_t minimumSize);
		int32_t FindTMMBRBoundingSet(TMMBRSet*& boundingSet);
		int32_t SetTMMBRBoundingSetToSend(
			const TMMBRSet* boundingSetToSend,
			const uint32_t maxBitrateKbit);

		bool IsOwner(const uint32_t ssrc, const uint32_t length) const;

		bool CalcMinBitRate(uint32_t* minBitrateKbit) const;

	protected:
		TMMBRSet*   VerifyAndAllocateBoundingSet(uint32_t minimumSize);
		int32_t VerifyAndAllocateBoundingSetToSend(uint32_t minimumSize);

		int32_t FindTMMBRBoundingSet(int32_t numCandidates, TMMBRSet& candidateSet);

	private:
		CriticalSectionWrapper* _criticalSection;
		TMMBRSet                _candidateSet;
		TMMBRSet                _boundingSet;
		TMMBRSet                _boundingSetToSend;

		float*                  _ptrIntersectionBoundingSet;
		float*                  _ptrMaxPRBoundingSet;
	};
#ifdef WIN32
	class CriticalSectionWindows : public CriticalSectionWrapper {
	public:
		CriticalSectionWindows();

		virtual ~CriticalSectionWindows();

		virtual void Enter();
		virtual void Leave();

	private:
		CRITICAL_SECTION crit;

		friend class ConditionVariableEventWin;
		friend class ConditionVariableNativeWin;
	};
#else
	class CriticalSectionPosix : public CriticalSectionWrapper {
	public:
		CriticalSectionPosix();

		~CriticalSectionPosix();

		void Enter() ;
		void Leave() ;

	private:
		pthread_mutex_t mutex_;
		friend class ConditionVariablePosix;
	};
#endif

	
	
	
	class RtcpStatisticsCallback {
	public:
		virtual ~RtcpStatisticsCallback() {}

		virtual void StatisticsUpdated(const RtcpStatistics& statistics,
			uint32_t ssrc) = 0;
		virtual void CNameChanged(const char* cname, uint32_t ssrc) = 0;
	};
	struct RtcpPacketTypeCounter {
		RtcpPacketTypeCounter()
		: first_packet_time_ms(-1),
		nack_packets(0),
		fir_packets(0),
		pli_packets(0),
		nack_requests(0),
		unique_nack_requests(0) {}

		void Add(const RtcpPacketTypeCounter& other) {
			nack_packets += other.nack_packets;
			fir_packets += other.fir_packets;
			pli_packets += other.pli_packets;
			nack_requests += other.nack_requests;
			unique_nack_requests += other.unique_nack_requests;
			if (other.first_packet_time_ms != -1 &&
				(other.first_packet_time_ms < first_packet_time_ms ||
				first_packet_time_ms == -1)) {
				// Use oldest time.
				first_packet_time_ms = other.first_packet_time_ms;
			}
		}

		int64_t TimeSinceFirstPacketInMs(int64_t now_ms) const {
			return (first_packet_time_ms == -1) ? -1 : (now_ms - first_packet_time_ms);
		}

		int UniqueNackRequestsInPercent() const {
			if (nack_requests == 0) {
				return 0;
			}
			return static_cast<int>(
				(unique_nack_requests * 100.0f / nack_requests) + 0.5f);
		}

		int64_t first_packet_time_ms;  // Time when first packet is sent/received.
		uint32_t nack_packets;   // Number of RTCP NACK packets.
		uint32_t fir_packets;    // Number of RTCP FIR packets.
		uint32_t pli_packets;    // Number of RTCP PLI packets.
		uint32_t nack_requests;  // Number of NACKed RTP packets.
		uint32_t unique_nack_requests;  // Number of unique NACKed RTP packets.
	};
	class RtcpPacketTypeCounterObserver {
	public:
		virtual ~RtcpPacketTypeCounterObserver() {}
		virtual void RtcpPacketTypesCounterUpdated(
			uint32_t ssrc,
			const RtcpPacketTypeCounter& packet_counter) = 0;
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

	
	class RTCP : public TMMBRHelp {
	public:
		RTCP(Clock* clock);
		~RTCP();
		RawPacket generate_rtcp_packet(RtcpPacket *rtcp_packet);
		void parse_rtcp_pacet(const uint8_t *data, uint32_t len, RTCPPacketInformation& rtcpPacketInformation);
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

		void HandleSDES(RTCPParserV2& rtcpParser);

		void HandleSDESChunk(RTCPParserV2& rtcpParser);

		void HandleXrHeader(RTCPParserV2& parser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleXrReceiveReferenceTime(
			RTCPParserV2& parser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleXrDlrrReportBlock(
			RTCPParserV2& parser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleXrDlrrReportBlockItem(
			const RTCPPacket& packet,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleXRVOIPMetric(
			RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleNACK(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleNACKItem(const RTCPPacket& rtcpPacket,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleBYE(RTCPParserV2& rtcpParser);
#ifdef SLICE_FCI
		void HandlePLI(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleSLI(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleSLIItem(const RTCPPacket& rtcpPacket,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleRPSI(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandlePsfbApp(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);
#endif
		void HandleREMBItem(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleIJ(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleIJItem(const RTCPPacket& rtcpPacket,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleTMMBR(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleTMMBRItem(RTCPReceiveInformation& receiveInfo,
			const RTCPPacket& rtcpPacket,
			RTCPPacketInformation& rtcpPacketInformation,
			uint32_t senderSSRC);

		void HandleTMMBN(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleSR_REQ(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleTMMBNItem(RTCPReceiveInformation& receiveInfo,
			const RTCPPacket& rtcpPacket);

		void HandleFIR(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleFIRItem(RTCPReceiveInformation* receiveInfo,
			const RTCPPacket& rtcpPacket,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleAPP(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		void HandleAPPItem(RTCPParserV2& rtcpParser,
			RTCPPacketInformation& rtcpPacketInformation);

		RTCPReportBlockInformation* GetReportBlockInformation(
			uint32_t remote_ssrc, uint32_t source_ssrc) const;
		//EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPReceiver);
		int32_t TMMBRReceived(uint32_t size,
			uint32_t accNumCandidates,
			TMMBRSet* candidateSet) const;
		RTCPReceiveInformation* GetReceiveInformation(uint32_t remoteSSRC);
		RTCPReceiveInformation* CreateReceiveInformation(
			uint32_t remoteSSRC);
		RTCPCnameInformation* GetCnameInformation(
			uint32_t remoteSSRC) const;
		RTCPReportBlockInformation* CreateOrGetReportBlockInformation(
			uint32_t remote_ssrc,
			uint32_t source_ssrc);
		RTCPCnameInformation* CreateCnameInformation(uint32_t remoteSSRC);
	private:

		std::map<uint32_t, RTCPCnameInformation*> _receivedCnameMap;
		CriticalSectionWrapper* _criticalSectionRTCPReceiver;
		RtcpStatisticsCallback* stats_callback_ GUARDED_BY(_criticalSectionFeedbacks);
		std::set<uint32_t> registered_ssrcs_;
		const bool receiver_only_;
		RtcpPacketTypeCounterObserver* const packet_type_counter_observer_;
		RtcpPacketTypeCounter packet_type_counter_;
		NackStats nack_stats_;
		uint32_t main_ssrc_;
		uint32_t _remoteSSRC;
		RtcpReceiveTimeInfo _remoteXRReceiveTimeInfo;
		RTCPSenderInfo _remoteSenderInfo;
		uint32_t _lastReceivedXRNTPsecs;
		uint32_t _lastReceivedXRNTPfrac;
		uint32_t _lastReceivedSRNTPsecs;
		uint32_t _lastReceivedSRNTPfrac;
		int64_t _lastReceivedRrMs;
		int64_t _lastIncreasedSequenceNumberMs;
		CriticalSectionWrapper* _criticalSectionFeedbacks;
		ReceivedInfoMap _receivedInfoMap;
		ReportBlockMap _receivedReportBlockMap
			GUARDED_BY(_criticalSectionRTCPReceiver);
		Clock* const _clock;
		int64_t xr_rr_rtt_ms_;
		//RtcpStatisticsCallback* stats_callback_ GUARDED_BY(_criticalSectionFeedbacks);
	};
}
#endif
