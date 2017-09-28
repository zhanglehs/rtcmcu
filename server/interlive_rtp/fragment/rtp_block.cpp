#include "rtp_block.h"

#include <string.h>
#include <stdlib.h>

using namespace avformat;

#define MAX_RTP_LEN 2048

namespace fragment {

  RTPBlock::RTPBlock() {
    _rtp_header = NULL;
    _rtp_len = 0;
  }

  RTPBlock::RTPBlock(const RTP_FIXED_HEADER* rtp, uint16_t len) {
    _rtp_header = NULL;
    _rtp_len = 0;
    set(rtp, len);
  }

  void RTPBlock::finalize() {
    if (_rtp_header != NULL) {
      free(_rtp_header);
      _rtp_header = NULL;
    }
    _rtp_len = 0;
  }

  RTPBlock::~RTPBlock() {
  }

  RTP_FIXED_HEADER* RTPBlock::set(const RTP_FIXED_HEADER* rtp, uint16_t len) {
    if (rtp == NULL || len == 0 || len > MAX_RTP_LEN) {
      return NULL;
    }

    if (_rtp_header == NULL) {
      _rtp_header = (RTP_FIXED_HEADER*)malloc(MAX_RTP_LEN);
    }
    _rtp_len = len;
    memcpy(_rtp_header, rtp, _rtp_len);

    return _rtp_header;
  }

  RTP_FIXED_HEADER* RTPBlock::get(uint16_t& len) {
    if (_rtp_len == 0) {
      len = 0;
      return NULL;
    }

    len = _rtp_len;
    return _rtp_header;
  }

  uint16_t RTPBlock::get_seq() const {
    return _rtp_header->get_seq();
  }

  uint32_t RTPBlock::get_timestamp() const {
    return _rtp_header->get_rtp_timestamp();
  }

  bool RTPBlock::is_valid() {
    if (_rtp_len == 0 || _rtp_header == NULL) {
      return false;
    }
    else {
      return true;
    }
  }

}
