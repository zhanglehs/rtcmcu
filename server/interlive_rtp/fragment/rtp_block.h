/**
* @file
* @brief
* @author   songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/07/24
* @see
*/


#pragma once

#include <stdint.h>
#include "avformat/rtp.h"

namespace fragment {

  class RTPBlock {
  public:
    RTPBlock();
    RTPBlock(const avformat::RTP_FIXED_HEADER* rtp, uint16_t len);
    ~RTPBlock();

    avformat::RTP_FIXED_HEADER* set(const avformat::RTP_FIXED_HEADER*, uint16_t);
    avformat::RTP_FIXED_HEADER* get(uint16_t&);
    bool is_valid();
    void finalize();
    uint16_t get_seq() const;
    uint32_t get_timestamp() const;

  protected:
    avformat::RTP_FIXED_HEADER* _rtp_header;
    uint16_t _rtp_len;
  };

}
