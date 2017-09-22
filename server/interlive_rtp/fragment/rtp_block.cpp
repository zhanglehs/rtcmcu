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


#include "rtp_block.h"
#include <string.h>
#include <algorithm>
#include "utils/memory.h"

using namespace std;
using namespace avformat;

namespace fragment
{

    RTPBlock::RTPBlock()
    {
        _rtp_header = NULL;
        _rtp_len = 0;
        _max_len = 2048;
    }

    int RTPBlock::new_count = 0;
    int RTPBlock::delete_count = 0;

    RTPBlock::RTPBlock(const RTP_FIXED_HEADER* rtp, uint16_t len)
    {
        _rtp_header = NULL;
        _rtp_len = 0;
        _max_len = 2048;
        set(rtp, len);
    }

    int RTPBlock::get_new_count()
    {
        return new_count;
    }

    int RTPBlock::get_delete_count()
    {
        return delete_count;
    }

    void RTPBlock::finalize()
    {
        if (_rtp_header != NULL)
        {
            delete_count++;
            mfree(_rtp_header);
            _rtp_header = NULL;
        }

        _rtp_len = 0;
    }

    RTPBlock::~RTPBlock()
    {
    }

    RTP_FIXED_HEADER* RTPBlock::init()
    {
        if (_rtp_header == NULL)
        {
            _rtp_header = (RTP_FIXED_HEADER*)mmalloc(_max_len * sizeof(char) );
            new_count++;
        }
        _rtp_len = 0;
        return _rtp_header;
    }

    RTP_FIXED_HEADER* RTPBlock::set(const RTP_FIXED_HEADER* rtp, uint16_t len)
    {
        if (rtp == NULL)
        {
            return NULL;
        }

        init();
        _rtp_len = min(len, _max_len);
        memcpy(_rtp_header, rtp, _rtp_len);

        return _rtp_header;
    }

    RTP_FIXED_HEADER* RTPBlock::get(uint16_t& len)
    {
        if (_rtp_len == 0)
        {
            len = 0;
            return NULL;
        }

        len = _rtp_len;
        return _rtp_header;
    }

    uint32_t RTPBlock::get_ssrc() const
    {
        return _rtp_header->get_ssrc();
    }

    uint16_t RTPBlock::get_seq() const
    {
        return _rtp_header->get_seq();
    }

    uint32_t RTPBlock::get_timestamp() const
    {
        return _rtp_header->get_rtp_timestamp();
    }

    RTPAVType RTPBlock::get_payload_type()
    {
        return (RTPAVType)_rtp_header->payload;
    }

    bool RTPBlock::is_valid()
    {
        if (_rtp_len == 0 || _rtp_header == NULL)
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    void RTPBlock::set_invalid()
    {
        _rtp_len = 0;
    }

    uint16_t RTPBlock::len()
    {
        return _rtp_len;
    }


}
