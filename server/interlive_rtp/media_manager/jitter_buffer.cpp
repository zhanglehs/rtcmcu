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

#include "../util/log.h"
#include "jitter_buffer.h"
#include "../avformat/rtcp.h"
#include <stdlib.h>

namespace media_manager {

  JitterBlockInfo::JitterBlockInfo() {
    seq = 0;
    rtp_time = 0;
  }

  bool JitterBlockInfo::update(fragment::RTPBlock *rtpblock) {
    if (rtpblock) {
      rtp_time = rtpblock->get_timestamp();
      seq = rtpblock->get_seq();
      return true;
    }
    return false;
  }

  void JitterBlockInfo::reset() {
    rtp_time = 0;
    seq = 0;
  }

  bool JitterBlockInfo::updateLatest(fragment::RTPBlock *rtpblock) {
    if (rtpblock && (JitterBuffer::jitter_is_newer_seq(rtpblock->get_seq(), seq) || seq == 0)) {
      seq = rtpblock->get_seq();
      rtp_time = rtpblock->get_timestamp();
      return true;
    }
    return false;
  }

  bool JitterBlockInfo::operator==(const JitterBlockInfo& right) {
    return seq == right.seq && rtp_time == right.rtp_time;
  }

  bool JitterBlockInfo::operator!=(const JitterBlockInfo& right) {
    return seq != right.seq || rtp_time != right.rtp_time;
  }

  //////////////////////////////////////////////////////////////////////////

  JitterBuffer::JitterBuffer() {
    _buffer_duration = 0;
    _time_base = 0;
    _max_buffer_time = 0;
    _jitter_block = new JitterBlockInfo();
    _last_queue_block = new JitterBlockInfo();
    _last_out_block = new JitterBlockInfo();
    _is_lost_queue_dirty = true;
    _state = INIT;
  }

  JitterBuffer::~JitterBuffer() {
    _reset();
    if (_jitter_block) {
      delete _jitter_block;
    }

    if (_last_queue_block) {
      delete _last_queue_block;
    }

    if (_last_out_block) {
      delete _last_out_block;
    }
    INF("close jitter buffer time_base %d max_buffer_time %d", _time_base, _max_buffer_time);
    _state = DISPOSED;
  }

  void JitterBuffer::SetRTPPacket(fragment::RTPBlock *rtpblock) {
    if (_time_base == 0) {
      return;
    }

    if (!rtpblock->is_valid()) {
      DBG("SetRTPPacket  skipped,rtpheader NULL");
      return;
    }

    if (_state == INIT) {
      if (_last_out_block->seq > 0 && jitter_is_newer_seq(_last_out_block->seq, rtpblock->get_seq())) {
        DBG("recv %d too late last %d,droped need buffer %d", rtpblock->get_seq(), _last_out_block->seq,
          (_last_out_block->rtp_time - rtpblock->get_timestamp()) / _time_base);
        rtpblock->finalize();
        delete rtpblock;
        rtpblock = NULL;
      }
      else {
        _queue.push(rtpblock);
        _jitter_block->update(rtpblock);
        _state = SENDING;
        _last_queue_block->updateLatest(rtpblock);
      }
    }
    else if (_state == SENDING) {
      if (abs(int(rtpblock->get_timestamp() - _last_queue_block->rtp_time)) >= int(20 * _time_base)) {
        _reset();
        _last_out_block->reset();
        SetRTPPacket(rtpblock);
        return;
      }
      if (jitter_is_newer_seq(rtpblock->get_seq(), _last_queue_block->seq)) {
        for (uint16_t i = _last_queue_block->seq + 1; jitter_is_newer_seq(rtpblock->get_seq(), i); i++) {
          _lost_queue.insert(_lost_queue.end(), i);
        }
      }

      if (_last_out_block->seq > 0 && jitter_is_newer_seq(_last_out_block->seq, rtpblock->get_seq())) {
        uint32_t buffer_time = (_last_queue_block->rtp_time - rtpblock->get_timestamp()) / (_time_base / 1000);
        DBG("recv %d too late last %d droped,timebase %d ,need buffer %d", rtpblock->get_seq(), _last_out_block->seq, _time_base,
          buffer_time);

        if (*(_jitter_block) != *(_last_out_block)) {
          _max_buffer_time = max(_max_buffer_time, buffer_time);
        }

        rtpblock->finalize();
        delete rtpblock;
        rtpblock = NULL;
      }
      else if (static_cast<uint16_t>(_jitter_block->seq + 1) == rtpblock->get_seq()) {
        _queue.push(rtpblock);
        if (!_lost_queue.empty()) {
          DBG("recv retrans %d ,timebase %d ts %d queue size %d", rtpblock->get_seq(), _time_base, rtpblock->get_timestamp(), (int)_lost_queue.size());
          if (_lost_queue.erase(rtpblock->get_seq()) > 0) {
            if (_lost_queue.empty()) {
              memcpy(_jitter_block, _last_queue_block, sizeof(JitterBlockInfo));
              DBG("lost queue empty last seq %d ts %d out seq %d ts %d", _last_queue_block->seq, _last_queue_block->rtp_time, _last_out_block->seq, _last_out_block->rtp_time);
            }
            else {
              _jitter_block->seq = *(_lost_queue.begin()) - 1;
              DBG("update jetter by lost queue %d %d %d", (int)rtpblock->get_seq(), (int)_jitter_block->seq, (int)_lost_queue.size());
            }
          }
          else {
            DBG("lost queue not contain %d %d", rtpblock->get_seq(), _lost_queue.empty() ? 0 : *(_lost_queue.begin()));
          }
          DBG("recv retrans %d ts %d ,timebase %d jitter %d %d", rtpblock->get_seq(), rtpblock->get_timestamp(), _time_base, _jitter_block->seq, _jitter_block->rtp_time);
        }
        else {
          _jitter_block->update(rtpblock);
        }
      }
      else {
        TRC("recv other lost %d %d %d", rtpblock->get_seq(), _time_base, _jitter_block->seq);
        _lost_queue.erase(rtpblock->get_seq());
        _queue.push(rtpblock);
      }

      _last_queue_block->updateLatest(rtpblock);
    }
  }

  fragment::RTPBlock *JitterBuffer::GetRTPPacket() {
    fragment::RTPBlock * result = NULL;
    if (_state == SENDING) {
      while (!_queue.empty()) {
        if (*(_jitter_block) != *(_last_queue_block)
          && ((_last_queue_block->rtp_time - _queue.top()->get_timestamp()) / (_time_base / 1000) < _buffer_duration)) {
          break;
        }

        result = _queue.top();
        _queue.pop();
        while (result  && result->get_seq() == _last_out_block->seq) {
          result->finalize();
          delete result;
          if (!_queue.empty()) {
            result = _queue.top();
            _queue.pop();
          }
          else {
            result = NULL;
            break;
          }
        }

        if (result) {
          _last_out_block->seq = result->get_seq();
          _last_out_block->rtp_time = result->get_timestamp();
        }

        if (_last_out_block->seq == _jitter_block->seq) {
          if (_lost_queue.empty()) {
            _jitter_block->seq = _last_queue_block->seq;
            _jitter_block->rtp_time = _last_queue_block->rtp_time;
          }
          else {
            _lost_queue.erase(_last_out_block->seq);
            _jitter_block->seq = *(_lost_queue.begin());
          }
        }

        break;
      }
      while (!_lost_queue.empty() && jitter_is_newer_seq(_last_out_block->seq, *(_lost_queue.begin()))) {
        DBG("remove form lost queue %d out %d timebase %d", *(_lost_queue.begin()), _last_out_block->seq, _time_base);
        _lost_queue.erase(_lost_queue.begin());
        if (_lost_queue.empty()) {
          _jitter_block->seq = _last_queue_block->seq;
          _jitter_block->rtp_time = _last_queue_block->rtp_time;
        }
        else {
          _jitter_block->seq = *(_lost_queue.begin());
        }
      }
    }

    if (_queue.size() > BUFFER_THRESHOLD) {
      WRN("buffer too large ,rebuffer toggled, check rate %d and ts last %d top %d correct",
        _time_base, _last_queue_block->rtp_time, _queue.empty() ? 0 : _queue.top()->get_timestamp());
      _reset();
    }
    return result;
  }

  void JitterBuffer::_reset() {
    while (!_queue.empty()) {
      fragment::RTPBlock *block = _queue.top();
      _queue.pop();
      block->finalize();
      delete block;

    }
    _lost_queue.clear();
    _is_lost_queue_dirty = true;
    _jitter_block->reset();
    _last_queue_block->reset();
    _state = INIT;
  }

  void JitterBuffer::setBufferDuration(uint32_t duration) {
    this->_buffer_duration = duration;
  }

  void JitterBuffer::setTimeBase(uint32_t time_base) {
    this->_time_base = time_base;
  }

  bool JitterBuffer::jitter_is_newer_seq(uint16_t seq, uint16_t preseq) {
    return (seq != preseq) &&
      static_cast<uint16_t>(seq - preseq) < 0x8000;
  }
}
