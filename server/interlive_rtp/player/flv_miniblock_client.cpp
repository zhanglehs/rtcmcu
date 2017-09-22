
#include "client.h"
#include "util/util.h"
#include "util/log.h"
#include "util/access.h"
#include "fragment/fragment.h"
#include "connection.h"
#include "target.h"

using namespace fragment;

#define HTTP_CONNECTION_HEADER "Connection: Close\r\n"
#define NOCACHE_HEADER "Expires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n"
#define WRITE_MAX (64 * 1024)

FLVMiniBlockClient::FLVMiniBlockClient(struct event_base * main_base, void *bind)
    :BaseClient(main_base, bind)
{
}

FLVMiniBlockClient::~FLVMiniBlockClient()
{}

bool FLVMiniBlockClient::is_state_stop()
{
    return _state == FLV_STOP ? true : false;
}

void FLVMiniBlockClient::set_init_stream_state()
{
    set_stream_state(FLV_START);
}

void FLVMiniBlockClient::set_init_timeoffset(uint32_t ts) {
    timeoffset = ts;
}

void FLVMiniBlockClient::request_data()
{
    switch (get_play_request().get_play_request_type())
    {
    case PlayRequest::RequestFlvMiniBlock:
        request_live_data();
        break;
    default:
        break;
    }
}

void FLVMiniBlockClient::request_live_data()
{
    connection *c = get_connection();
    PlayRequest &play_req = get_play_request();

    if (get_stream_state() == FLV_START)
    {
        ACCESS("player-live %s:%6hu %s %s?%s 200",
            c->remote_ip, c->remote_port, play_req.get_method().c_str(), play_req.get_path().c_str(), play_req.get_query().c_str());
        // reponse header
        char rsp[1024];
        int used = snprintf(rsp, sizeof(rsp),
            "HTTP/1.1 200 OK\r\nServer: Youku Live Forward\r\n"
            "Content-Type: video/x-flv\r\n"
            NOCACHE_HEADER
            HTTP_CONNECTION_HEADER "\r\n");
        if (0 != buffer_append_ptr(c->w, rsp, used))
        {
            DBG("player-live buffer_append_ptr failed. closing connection...#%d(%s:%hu)", c->fd, c->remote_ip, c->remote_port);
            set_stream_state(FLV_STOP);
        }
        set_stream_state(FLV_STREAM_HEADER);
    }
    else if (get_stream_state() == FLV_STREAM_HEADER)
    {
        int status_code = 0;

        StreamId_Ext streamid = play_req.get_sid();
        FLVHeader flvheader(streamid);

        if (!_cmng->get_miniblock_flv_header(play_req.get_sid(), flvheader, status_code))
        {
            WRN("req live header failed, streamid= %s, status code= %d",
                play_req.get_sid().unparse().c_str(), status_code);
            return;
        }
        else if (FLV_FLAG_BOTH == play_req.header_flag){
            flvheader.copy_header_to_buffer(c->w);
        }
        else if (FLV_FLAG_AUDIO == play_req.header_flag)
        {
            flvheader.copy_audio_header_to_buffer(c->w);
        }
        else if (FLV_FLAG_VIDEO == play_req.header_flag){
            flvheader.copy_video_header_to_buffer(c->w);
        }

        set_stream_state(FLV_STREAM_FIRST_TAG);
    }
    else if (get_stream_state() == FLV_STREAM_FIRST_TAG)
    {
        int status_code;

        FLVMiniBlock* block = _cmng->get_latest_miniblock(play_req.get_sid(), status_code);
        if (status_code >= 0)
        {
            int bid = block->get_seq();
            play_req.set_global_block_seq(bid + 1);
            block->copy_payload_to_buffer(c->w, timeoffset, play_req.header_flag);
            set_stream_state(FLV_STREAM_TAG);
        }
        else
        {
            switch_to_hungry();
        }
    }
    else if (get_stream_state() == FLV_STREAM_TAG) 
    {
        int status_code = 0;
        int count = 0;
        while (status_code >= 0){
            FLVMiniBlock* block = _cmng->get_miniblock_by_seq(play_req.get_sid(), play_req.get_global_block_seq_(), status_code);
            if (status_code >= 0)
            {
                int bid = block->get_seq();
                play_req.set_global_block_seq(bid + 1);
                block->copy_payload_to_buffer(c->w, timeoffset, play_req.header_flag);
                set_stream_state(FLV_STREAM_TAG);
                count++;
            }
        }
        
        if (count == 0)
        {
            switch_to_hungry();
        }
    }
}
