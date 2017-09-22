/**
* @file
* @brief
* @author   gaosiyu
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>gaosiyu@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/09/16
* @see
*/
#ifndef JITTER_BUFFER_H
#define JITTER_BUFFER_H

#include <set>
#include <queue>
#include "../fragment/rtp_block.h"
#include "../avformat/rtcp.h"
using namespace std;

#define BUFFER_THRESHOLD 5000

namespace media_manager {

    enum BUFFER_STATE {
        INIT,
        SENDING,
        DISPOSED
    };

    class JitterBlockInfo
    {
    public:
        uint32_t rtp_time;
        uint16_t seq;
    public:
        JitterBlockInfo();
        bool operator==(const JitterBlockInfo& right);
        bool operator!=(const JitterBlockInfo& right);
        bool update(fragment::RTPBlock *rtpblock);
        bool updateLatest(fragment::RTPBlock *rtpblock);
        void reset();
    };

    class DLLEXPORT JitterBuffer {
        struct cmp{
            bool operator()(fragment::RTPBlock * a, fragment::RTPBlock * b){
                return jitter_is_newer_seq(a->get_seq(), b->get_seq());
            }
        };

        class SequenceNumberLessThan {
        public:
            bool operator() (const uint16_t& sequence_number1,
                const uint16_t& sequence_number2) const {
                return jitter_is_newer_seq(sequence_number2, sequence_number1);
            }
        };

        typedef priority_queue<fragment::RTPBlock *, vector<fragment::RTPBlock *>, cmp> RTPPACKET_QUEUE;
        typedef std::set<uint16_t, SequenceNumberLessThan> LOST_QUEUE;

    private:
        RTPPACKET_QUEUE  _queue;
        LOST_QUEUE _lost_queue;
        BUFFER_STATE _state;
        JitterBlockInfo *_jitter_block;
        JitterBlockInfo *_last_queue_block;
        JitterBlockInfo *_last_out_block;
        uint32_t _buffer_duration;
        uint32_t _time_base;
        uint32_t _max_buffer_time;
        bool     _is_lost_queue_dirty;
    public:
        JitterBuffer();
        ~JitterBuffer();
        void SetRTPPacket(fragment::RTPBlock *rtpblock);
        fragment::RTPBlock *GetRTPPacket();
        void setBufferDuration(uint32_t duration);
        void setTimeBase(uint32_t time_base);
        void _reset();
        static bool jitter_is_newer_seq(uint16_t seq, uint16_t preseq);
    };

}
#endif
