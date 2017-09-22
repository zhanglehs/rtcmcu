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


#include "rtp_block_cache.h"
#include "cache_watcher.h"
#include "media_manager_rtp_interface.h"
#include "util/log.h"
#include "time.h"
#include "media_manager_state.h"
#include <assert.h>
#include <algorithm>
#include "cache_manager.h"

using namespace fragment;
using namespace avformat;
using namespace std;

namespace media_manager
{


    RTPCircularCache::RTPCircularCache(StreamId_Ext& stream_id)
    {
        _media_manager = NULL;
        _stream_id = stream_id;
        _ssrc = 0;
        _last_push_relative_timestamp_ms = 0;
        set_max_size(256 * 16);
    }

    RTPCircularCache::~RTPCircularCache()
    {
        set_max_size(0);
    }

    int32_t RTPCircularCache::set_manager(MediaManagerRTPInterface* media_manager_interface)
    {
        _media_manager = media_manager_interface;
        return 0;
    }

    uint32_t RTPCircularCache::set_max_size(uint32_t max_size)
    {
        _max_size = max_size;

        _adjust();

        return _max_size;
    }

    // if left >= right
    bool seq_ge(uint16_t left, uint16_t right)
    {
        if (left - right < 0x8000)
        {
            return true;
        }
        return false;
    }

    // if left > right
    bool seq_gt(uint16_t left, uint16_t right)
    {
        if ((left != right) && ((uint16_t)(left - right) < 0x8000))
        {
            return true;
        }
        return false;
    }

    // if left < right
    bool seq_lt(uint16_t left, uint16_t right)
    {
        if ((left != right) && ((uint16_t)(right - left) < 0x8000))
        {
            return true;
        }
        return false;
    }

    void RTPCircularCache::_push_back(const avformat::RTP_FIXED_HEADER* rtp, uint16_t len)
    {
        RTPBlock block(rtp, len);
        _circular_cache.push_back(block);

        uint32_t delta_tick = (uint32_t)(block.get_timestamp() - _last_push_tick_timestamp);

        if(_last_push_tick_timestamp > block.get_timestamp()) {
	    WRN("WRONG TIMESTAMP block ts %d last tick %d ssrc %d len %d streamid %s",block.get_timestamp(),_last_push_tick_timestamp,rtp->get_ssrc(),len,_stream_id.unparse().c_str());
	}

        _last_push_relative_timestamp_ms += (uint64_t)delta_tick * 1000 / _sample_rate;

        _last_push_seq = block.get_seq();
        _last_push_tick_timestamp = block.get_timestamp();

        _set_push_active();
    }

    int32_t RTPCircularCache::empty_count()
    {
        int count = 0;
        for (unsigned int i = 0; i < _circular_cache.size(); i++)
        {
            if (!_circular_cache[i].is_valid())
            {
                count++;
            }
        }

        return count;
    }

    int32_t RTPCircularCache::set_rtp(const RTP_FIXED_HEADER* rtp, uint16_t len, int32_t& status)
    {
        if (_sample_rate == 0)
        {
            return 1;
        }

        if (rtp == NULL || len < sizeof(RTP_FIXED_HEADER))
        {
            WRN("rtp invalid, stream_id: %s", _stream_id.unparse().c_str());
            status = RTP_CACHE_RTP_INVALID;
            return -1;
        }

        if (_ssrc == 0)
        {
            _ssrc = rtp->get_ssrc();
            _last_push_tick_timestamp = rtp->get_rtp_timestamp();
            INF("update ssrc %u payload %u ver %u", rtp->get_ssrc(), rtp->payload, rtp->version);
        }

        if (rtp->get_ssrc() != _ssrc)
        {
            WRN("rtp ssrc not equal, stream_id: %s, _ssrc: %u, new_ssrc: %u", 
                _stream_id.unparse().c_str(), _ssrc, rtp->get_ssrc());
            status = RTP_CACHE_RTP_INVALID;
            return -1;
        }

        uint16_t seq = rtp->get_seq();

        if (_circular_cache.size() == 0)
        {
            _push_back(rtp, len);
            status = STATUS_SUCCESS;
            return 0;
        }

        uint16_t latest_len = 0;
        avformat::RTP_FIXED_HEADER* latest
          = _circular_cache.back().get(latest_len);
        if (latest && _sample_rate > 0
          && abs(int(latest->get_rtp_timestamp() - rtp->get_rtp_timestamp()))
          >= int(20 * _sample_rate)) {
          clean();
          _push_back(rtp, len);

          status = STATUS_SUCCESS;
          return 0;
        }

        // this is a circular cache
        // expect_next_max = back.seq + _max_size
        //                         3                      1                    2
        // ||---------|-------------------------|-------------------|------------------||
        //          front.seq                back.seq         expect_next_max
        //            |       cache             |     large seq     |      small seq
        //
        // 1. if back.seq        <  new_block.seq < expect_next_max,  push back and fill empty block
        // 2. if expect_next_max <= new_block.seq < front.seq,         reset cache,
        // 3. if front.seq       <= new_block.seq <= back.seq,         fill in.
        //

        uint16_t back_seq = _circular_cache.back().get_seq();
        uint16_t front_seq = _circular_cache.front().get_seq();
        uint16_t expect_next_max = back_seq + _max_size;

        // 1. if back.seq        < new_block.seq <= expect_next_max,  push back and fill empty block
        if (seq != back_seq && (uint16_t)(seq - back_seq) < _max_size)
        {
            for (uint16_t idx = back_seq + 1; idx != seq; idx++)
            {
                RTPBlock block(NULL, 0);
                _circular_cache.push_back(block);
            }
            _push_back(rtp, len);
            _adjust();

            status = STATUS_SUCCESS;
            return 0;
        }

        // 2. if expect_next_max < new_block.seq < front.seq,         reset cache,
        if ((uint16_t)(seq - expect_next_max) < (uint16_t)(front_seq - expect_next_max))
        {
            clean();
            _push_back(rtp, len);

            status = STATUS_SUCCESS;
            return 0;
        }

        // 3. if front.seq       <= new_block.seq <= back.seq,         fill in.
        if ((uint16_t)(seq - front_seq) <= (uint16_t)(back_seq - front_seq))
        {
            _fill_in(rtp, len);
            status = STATUS_SUCCESS;
            return 0;
        }

        ERR("unknown error, stream_id: %s, rtp seq:%u, rtp len: %u, cache_size: %d",
            _stream_id.unparse().c_str(), rtp->get_seq(), len, (int)_circular_cache.size());
        status = RTP_CACHE_UNKNOWN_ERROR;
        assert(0);
        return -1;
    }

    int RTPCircularCache::_fill_in(const RTP_FIXED_HEADER* rtp, uint16_t len)
    {
        uint16_t seq = rtp->get_seq();
        uint16_t front_seq = _circular_cache.front().get_seq();
        uint32_t idx = (uint16_t)(seq - front_seq);

        if (idx >= _circular_cache.size())
        {
            ERR("unknown error, stream_id: %s, rtp seq:%u, front_seq: %u, cache_size: %d",
              _stream_id.unparse().c_str(), rtp->get_seq(), front_seq, (int)_circular_cache.size());
            clean();
            assert(0);
            return -1;
        }

        RTPBlock& block = _circular_cache[idx];
        if (!block.is_valid())
        {
            block.set(rtp, len);
        }
        else
        {
            TRC("this block is already set. stream_id: %s, seq: %u", _stream_id.unparse().c_str(), seq);
        }

        return 0;
    }

    void RTPCircularCache::_set_push_active()
    {
        _push_active = time(NULL);
    }

    uint16_t RTPCircularCache::size()
    {
        return _circular_cache.size();
    }

    uint16_t RTPCircularCache::max_size()
    {
        return _max_size;
    }

    uint32_t RTPCircularCache::get_ssrc()
    {
        return _ssrc;
    }

    uint32_t RTPCircularCache::_adjust()
    {
        while (_circular_cache.size() > _max_size)
        {
            _circular_cache.front().finalize();
            _circular_cache.pop_front();

        }

        while (_circular_cache.size() > 0 && !_circular_cache.front().is_valid())
        {
            _circular_cache.front().finalize();
            _circular_cache.pop_front();
        }

        return _circular_cache.size();
    }

    void RTPCircularCache::clean()
    {
        while (_circular_cache.size() > 0)
        {
            RTPBlock& block = _circular_cache.back();
            block.finalize();
            _circular_cache.pop_back();
        }
    }

    void RTPCircularCache::on_timer()
    {

    }

    time_t RTPCircularCache::get_push_active()
    {
        return _push_active;
    }

    uint64_t RTPCircularCache::get_last_push_relative_timestamp_ms()
    {
        return _last_push_relative_timestamp_ms;
    }

    void RTPCircularCache::reset()
    {
        clean();
        clean();
        _ssrc = 0;
        _sample_rate = 0;
        _av_type = RTP_AV_NULL;
        _media_type = RTP_MEDIA_NULL;
        _last_push_relative_timestamp_ms = 0;
    }

    RTP_FIXED_HEADER* RTPCircularCache::get_latest(uint16_t& len, int32_t& status_code)
    {
        if (_circular_cache.size() == 0)
        {
            INF("cache empty");
            status_code = RTP_CACHE_CIRCULAR_CACHE_EMPTY;
            len = 0;
            return NULL;
        }

        RTPBlock& block = _circular_cache.back();

        if (block.is_valid())
        {
            int32_t status = 0;
            StreamStore* store = ((CacheManager *)_media_manager)->get_stream_store(_stream_id, status);
            if (store && status == STATUS_SUCCESS)
            {
                store->set_req_active();
            }

            status_code = STATUS_SUCCESS;

            return block.get(len);
        }

        ERR("unknown error, stream_id: %s, front_seq: %u, cache_size: %d",
          _stream_id.unparse().c_str(), _circular_cache.front().get_seq(), (int)_circular_cache.size());
        len = 0;
        status_code = RTP_CACHE_UNKNOWN_ERROR;
        assert(0);
        return NULL;
    }

    RTP_FIXED_HEADER* RTPCircularCache::get_by_seq(uint16_t seq, uint16_t& len, int32_t& status_code, bool return_next_valid_packet)
    {
        len = 0;

        if (_circular_cache.size() == 0)
        {
            INF("cache empty");
            status_code = RTP_CACHE_CIRCULAR_CACHE_EMPTY;
            len = 0;
            return NULL;
        }

        uint16_t back_seq = _circular_cache.back().get_seq();
        uint16_t front_seq = _circular_cache.front().get_seq();

        if (seq_gt(seq, back_seq))
        {
            status_code = RTP_CACHE_SEQ_TOO_LARGE;
            return NULL;
        }

        if (seq_lt(seq, front_seq))
        {
            status_code = RTP_CACHE_SEQ_TOO_SMALL;
            return NULL;
        }

        uint16_t idx = seq - front_seq;
        RTPBlock& block = _circular_cache[idx];

        if (block.is_valid())
        {
            if (seq == block.get_seq())
            {
                int32_t status = 0;
                StreamStore* store = ((CacheManager *)_media_manager)->get_stream_store(_stream_id, status);
                if (store && status == STATUS_SUCCESS)
                {
                    store->set_req_active();
                }
                status_code = STATUS_SUCCESS;
                return block.get(len);
            }
            else
            {
                ERR("unknown error, stream_id: %s, rtp seq:%u, front_seq: %u, cache_size: %d",
                  _stream_id.unparse().c_str(), seq, front_seq, (int)_circular_cache.size());
                len = 0;
                status_code = RTP_CACHE_UNKNOWN_ERROR;
                assert(0);
                return NULL;
            }
        }

        if (!return_next_valid_packet)
        {
            status_code = RTP_CACHE_NO_THIS_SEQ;
            len = 0;
            return NULL;
        }
        else
        {
            for (idx = seq - front_seq; idx < _circular_cache.size(); idx++)
            {
                RTPBlock& block = _circular_cache[idx];
                if (block.is_valid())
                {
                    status_code = STATUS_SUCCESS;
                    return block.get(len);
                }
            }
        }

        ERR("unknown error, stream_id: %s, rtp seq:%u, front_seq: %u, cache_size: %d",
          _stream_id.unparse().c_str(), seq, front_seq, (int)_circular_cache.size());
        len = 0;
        status_code = RTP_CACHE_UNKNOWN_ERROR;
        assert(0);
        return NULL;
    }

    RTP_FIXED_HEADER* RTPCircularCache::get_next_by_seq(uint16_t seq, uint16_t& len, int32_t& status_code)
    {
        seq++;
        return get_by_seq(seq, len, status_code, true);
    }

    uint32_t RTPCircularCache::set_sample_rate(uint32_t sample_rate)
    {
        return _sample_rate = sample_rate;
    }

    uint32_t RTPCircularCache::set_max_duration_ms(uint32_t max_duration)
    {
        return _max_duration = max_duration;
    }

    bool  RTPCircularCache::is_inited () {
        return _sample_rate > 0 && _max_duration > 0 && _max_size > 0;
    }



    RTPMediaCache::RTPMediaCache(StreamId_Ext& stream_id)
        :_stream_id(stream_id),
        _sdp(NULL),
        _push_timeout_seconds(10),
        _push_active(0),
        _media_manager(NULL),
        _last_push_relative_timestamp_ms(0)
    {
        _video_cache = new RTPCircularCache(stream_id);
        _audio_cache = new RTPCircularCache(stream_id);

        _ntp_type = avformat::RTP_AV_NULL;
        _ntp_secs = 0;
        _ntp_frac = 0;
        _ntp_rtp = 0;
    }

    RTPMediaCache::~RTPMediaCache()
    {

        if (_sdp != NULL)
        {
            delete _sdp;
            _sdp = NULL;
        }

        if (_audio_cache != NULL)
        {
            delete _audio_cache;
            _audio_cache = NULL;
        }

        if (_video_cache != NULL)
        {
            delete _video_cache;
            _video_cache = NULL;
        }
    }

    int32_t RTPMediaCache::set_manager(MediaManagerRTPInterface* media_manager_interface)
    {
        _media_manager = media_manager_interface;
        if (_video_cache)
        {
            _video_cache->set_manager(_media_manager);
        }
        
        if (_audio_cache)
        {
            _audio_cache->set_manager(_media_manager);
        }

        return 0;
    }

    int32_t RTPMediaCache::set_sdp(std::string& sdp)
    {
        INF("set sdp, sid=%s", _stream_id.unparse().c_str());

        if (_sdp != NULL)
        {
            /*if (_sdp->get_sdp_str() == sdp)
            {
                if (_media_manager != NULL)
                {
                    _media_manager->notify_watcher(_stream_id, CACHE_WATCHING_SDP);
                }
                return 0;
            }
            else
            {*/
                delete _sdp;
                _sdp = NULL;
            //}
        }

        _sdp = new SdpInfo();
        _sdp->load(sdp.c_str(), sdp.length());

        _reset_cache();

        if (_media_manager != NULL)
        {
            _media_manager->notify_watcher(_stream_id, CACHE_WATCHING_SDP);
        }

        return 0;
    }

    int32_t RTPMediaCache::set_sdp(const char* sdp, int32_t len)
    {
        string sdp_str = sdp;
        return set_sdp(sdp_str);
    }

    SdpInfo* RTPMediaCache::get_sdp()
    {
        int32_t status = 0;
        StreamStore* store = ((CacheManager *)_media_manager)->get_stream_store(_stream_id, status);
        if (store && status == STATUS_SUCCESS)
        {
            store->set_req_active();
        }
        return _sdp;
    }

    SdpInfo* RTPMediaCache::get_sdp(int32_t& status_code)
    {
        if (_sdp == NULL)
        {
            status_code = STATUS_NO_DATA;
        }
        else
        {
            int32_t status = 0;
            StreamStore* store = ((CacheManager *)_media_manager)->get_stream_store(_stream_id, status);
            if (store && status == STATUS_SUCCESS)
            {
                store->set_req_active();
            }
            status_code = STATUS_SUCCESS;
        }
        return _sdp;
    }

    int32_t RTPMediaCache::_reset_cache()
    {
        if (_audio_cache != NULL)
        {
            _audio_cache->reset();
        }

        if (_video_cache != NULL)
        {
            _video_cache->reset();
        }

        const vector<rtp_media_info*>& _media_infos = _sdp->get_media_infos();

        for (unsigned int i = 0; i < _media_infos.size(); i++)
        {
            RTPMediaType media_type;
            rtp_media_info* media_info = _media_infos[i];
            switch (media_info->payload_type)
            {
            case RTP_AV_H264:
                media_type = RTP_MEDIA_VIDEO;
                break;
            case RTP_AV_AAC:
            case RTP_AV_AAC_GXH:
            case RTP_AV_AAC_MAIN:
            case RTP_AV_MP3:
                media_type = RTP_MEDIA_AUDIO;
                break;
            default:
                media_type = RTP_MEDIA_NULL;
                break;
            }

            switch (media_type)
            {
            case avformat::RTP_MEDIA_NULL:
                break;
            case avformat::RTP_MEDIA_AUDIO:
                    _audio_cache->set_sample_rate(media_info->rate);
                    _audio_cache->set_max_size(1000);
                    _audio_cache->set_max_duration_ms(10000);
                break;
            case avformat::RTP_MEDIA_VIDEO:
                    _video_cache->set_sample_rate(media_info->rate);
                    _video_cache->set_max_size(3000);
                    _video_cache->set_max_duration_ms(10000);
                break;
            case avformat::RTP_MEDIA_BOTH:
                break;
            default:
                break;
            }
        }

        return 0;
    }

    RTPCircularCache* RTPMediaCache::get_cache_by_ssrc(uint32_t ssrc)
    {
        if (_audio_cache != NULL)
        {
            if (_audio_cache->get_ssrc() == ssrc)
            {
                return _audio_cache;
            }
        }

        if (_video_cache != NULL)
        {
            if (_video_cache->get_ssrc() == ssrc)
            {
                return _video_cache;
            }
        }

        return NULL;
    }

    RTPCircularCache* RTPMediaCache::get_cache_by_media(avformat::RTPMediaType media_type)
    {
        switch (media_type)
        {
        case avformat::RTP_MEDIA_NULL:
            break;
        case avformat::RTP_MEDIA_AUDIO:
            return _audio_cache->is_inited() ? _audio_cache : NULL;
            break;
        case avformat::RTP_MEDIA_VIDEO:
            return _video_cache->is_inited() ? _video_cache : NULL;
            break;
        case avformat::RTP_MEDIA_BOTH:
            break;
        default:
            break;
        }

        return NULL;
    }

    RTPCircularCache* RTPMediaCache::get_cache_by_codec(avformat::RTPAVType av_type)
    {
        return NULL;
    }

    RTPCircularCache* RTPMediaCache::get_audio_cache()
    {
        return _audio_cache->is_inited()?_audio_cache:NULL;
    }

    RTPCircularCache* RTPMediaCache::get_video_cache()
    {
        return _video_cache->is_inited() ? _video_cache:NULL;
    }

    void RTPMediaCache::on_timer()
    {
        //time_t now = time(NULL);

        //uint64_t active_second = now;

        //RTPCircularCache* audio_cache = get_audio_cache();
        //if (audio_cache != NULL && audio_cache->size() > 0)
        //{
        //    active_second = min(now, audio_cache->get_push_active());
        //}

        //RTPCircularCache* video_cache = get_video_cache();
        //if (video_cache != NULL && video_cache->size() > 0)
        //{
        //    active_second = min(now, video_cache->get_push_active());
        //}

        /*if (now - active_second > _push_timeout_seconds)
        {
            if (audio_cache != NULL)
            {
                audio_cache->reset();
            }

            if (video_cache != NULL)
            {
                video_cache->reset();
            }
        }*/
    }

    bool RTPMediaCache::rtp_empty()
    {
        bool audio_empty = false;
        bool video_empty = false;

        RTPCircularCache* audio_cache = get_audio_cache();
        if (audio_cache == NULL || audio_cache->size() == 0)
        {
            audio_empty = true;
        }

        RTPCircularCache* video_cache = get_video_cache();
        if (video_cache == NULL || video_cache->size() == 0)
        {
            video_empty = true;
        }

        return audio_empty && video_empty;
    }


    void RTPMediaCache::adjust_cache_size()
    {

    }

    RTP_FIXED_HEADER* RTPMediaCache::get_latest_by_media_item(RTPMediaItem& item, uint16_t& len, int32_t& status_code)
    {
        RTP_FIXED_HEADER* rtp = NULL;
        len = 0;

        RTPCircularCache* circualr_cache[2];
        circualr_cache[0] = _audio_cache->is_inited() ? _audio_cache : NULL;
        circualr_cache[1] = _video_cache->is_inited() ? _video_cache : NULL;

        for (int i = 0; i < 2; i++)
        {
            if (circualr_cache[i] != NULL)
            {
                rtp = circualr_cache[i]->get_latest(len, status_code);
                if (status_code == STATUS_SUCCESS)
                {
                    item.items[i].seq = rtp->get_seq();
                    item.items[i].ssrc = rtp->get_ssrc();

                    return rtp;
                }
            }
        }

        status_code = RTP_CACHE_MEDIA_CACHE_EMPTY;
        return NULL;
    }

    RTP_FIXED_HEADER* RTPMediaCache::get_next_by_media_item(RTPMediaItem& item, uint16_t& len, int32_t& status_code)
    {
        RTP_FIXED_HEADER* rtp = NULL;
        len = 0;

        RTPCircularCache* circualr_cache[2];
        circualr_cache[0] = _audio_cache->is_inited() ? _audio_cache : NULL;
        circualr_cache[1] = _video_cache->is_inited() ? _video_cache : NULL;

        for (int i = 0; i < 2; i++)
        {
            if (circualr_cache[i] != NULL)
            {
                if (item.items[i].ssrc == 0)
                {
                    rtp = circualr_cache[i]->get_latest(len, status_code);
                    if (status_code == STATUS_SUCCESS)
                    {
                        item.items[i].seq = rtp->get_seq();
                        item.items[i].ssrc = rtp->get_ssrc();

                        return rtp;
                    }
                    else
                    {
                        continue;
                    }
                }
                else
                {
                    rtp = circualr_cache[i]->get_next_by_seq(item.items[i].seq, len, status_code);

                    switch (status_code)
                    {
                    case STATUS_SUCCESS:
                        item.items[i].seq = rtp->get_seq();
                        item.items[i].ssrc = rtp->get_ssrc();

                        return rtp;

                    case STATUS_TOO_SMALL:
                        rtp = circualr_cache[i]->get_latest(len, status_code);
                        if (status_code == STATUS_SUCCESS)
                        {
                            item.items[i].seq = rtp->get_seq();
                            item.items[i].ssrc = rtp->get_ssrc();

                            return rtp;
                        }
                        else
                        {
                            continue;
                        }
                        break;

                    case STATUS_TOO_LARGE:
                    case RTP_CACHE_CIRCULAR_CACHE_EMPTY:
                        continue;
                    default:
                        break;
                    }
                }
            }
        }

        return NULL;
    }

    int32_t RTPMediaCache::set_rtp(const RTP_FIXED_HEADER* rtp, uint16_t len, int32_t& status)
    {
        if (rtp == NULL || len < sizeof(RTP_FIXED_HEADER))
        {
            status = RTP_CACHE_RTP_INVALID;
            return status;
        }

        RTPAVType type = (RTPAVType)rtp->payload;

        if (_audio_cache == NULL && _video_cache == NULL)
        {
            WRN("both audio and video cache is NULL, streamid: %s, rtp media type: %u, rtp seq: %u",
                _stream_id.unparse().c_str(), type, rtp->get_seq());
            status = RTP_CACHE_SET_RTP_FAILED;
            return status;
        }
        
        switch (type)
        {
        case RTP_AV_MP3:
        case RTP_AV_AAC:
        case RTP_AV_AAC_GXH:
        case RTP_AV_AAC_MAIN:
            if (_audio_cache == NULL)
            {
                WRN("audio cache is NULL, streamid: %s, rtp media type: %u, rtp seq: %u",
                    _stream_id.unparse().c_str(), type, rtp->get_seq());
                status = RTP_CACHE_SET_RTP_FAILED;
            }
            else
            {
                _audio_cache->set_rtp(rtp, len, status);
                if (status == STATUS_SUCCESS && _media_manager != NULL)
                    _media_manager->notify_watcher(_stream_id, CACHE_WATCHING_RTP_BLOCK);
            }
            break;
        case RTP_AV_H264:
            if (_video_cache == NULL)
            {
                WRN("video cache is NULL, streamid: %s, rtp media type: %u, rtp seq: %u",
                    _stream_id.unparse().c_str(), type, rtp->get_seq());
                status = RTP_CACHE_SET_RTP_FAILED;
            }
            else
            {
                _video_cache->set_rtp(rtp, len, status);
                if (status == STATUS_SUCCESS && _media_manager != NULL)
                    _media_manager->notify_watcher(_stream_id, CACHE_WATCHING_RTP_BLOCK);
            }
            break;
        case RTP_AV_ALL:
        case RTP_AV_NULL:
        default:
            // ERROR
            status = RTP_CACHE_SET_RTP_FAILED;
	        ERR("set rtp failed , invailed payload type %u",type);
        }

        if (status == STATUS_SUCCESS)
        {
            set_push_active_time();
            if (_audio_cache != NULL && _audio_cache->is_inited())
            {
                _last_push_relative_timestamp_ms = MAX(_last_push_relative_timestamp_ms, _audio_cache->get_last_push_relative_timestamp_ms());
            }

            if (_video_cache != NULL && _video_cache->is_inited())
            {
                _last_push_relative_timestamp_ms = MAX(_last_push_relative_timestamp_ms, _video_cache->get_last_push_relative_timestamp_ms());
            }
        }
        return status;
    }

    int32_t RTPMediaCache::set_uploader_ntp(avformat::RTPAVType type, uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp) {
      _ntp_type = type;
      _ntp_secs = ntp_secs;
      _ntp_frac = ntp_frac;
      _ntp_rtp = rtp;
      if (_media_manager != NULL) {
        _media_manager->notify_watcher(_stream_id, CACHE_WATCHING_RTP_BLOCK);
      }
      _ntp_type = RTP_AV_NULL;
      return 0;
    }

    int32_t RTPMediaCache::get_uploader_ntp(avformat::RTPAVType &type, uint32_t &ntp_secs, uint32_t &ntp_frac, uint32_t &rtp) {
      type = _ntp_type;
      ntp_secs = _ntp_secs;
      ntp_frac = _ntp_frac;
      rtp = _ntp_rtp;
      return 0;
    }

    time_t RTPMediaCache::set_push_active_time()
    {
        _push_active = time(NULL);
        return _push_active;
    }

    time_t RTPMediaCache::get_push_active_time()
    {
        return _push_active;
    }

    uint64_t RTPMediaCache::get_last_push_relative_timestamp_ms()
    {
        return _last_push_relative_timestamp_ms;
    }
}
