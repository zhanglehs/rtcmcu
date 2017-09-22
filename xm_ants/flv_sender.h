/*
 * a sender send flv data to receiver
 * author: hechao@youku.com
 * create: 2014/3/10
 *
 */

#ifndef __FLV_SENDER_H__
#define __FLV_SENDER_H__

#include <string>
#include <utils/buffer.hpp>
#include <utils/buffer_queue.h>

#include "levent.h"
#include "global_var.h"
#include "streamid.h"

class AdminServer;

enum FLVSenderState
{
    FSStateInit = 0,
    FSStateConnecting,
    FSStateConnected,
    FSStateReqSending, //u2r_req_state
    FSStateReqSent,
    FSStateRspReceiving, // u2r_rsp_state
    FSStateRspReceived,
    FSStateWaitingData, // waiting for data from ant
    FSStateStartStreaming,
    FSStateStreaming,
    FSStateReqSending_v2, //u2r_req_state
    FSStateReqSent_v2,
    FSStateRspReceiving_v2, // u2r_rsp_state
    FSStateRspReceived_v2,
    FSStateWaitingData_v2, // waiting for data from ant
    FSStateStartStreaming_v2,
    FSStateStreaming_v2,
    FSStateEnd
};

class FLVSender
{
    public:
        FLVSender();
        
        int init(const std::string receiver_ip,
                uint16_t receiver_port,
                StreamId_Ext& stream_id,
                AdminServer *admin);
        FLVSender(const std::string receiver_ip,
                uint16_t receiver_port,
                StreamId_Ext& stream_id,
                AdminServer *admin);
        virtual ~FLVSender();

        int init2();
        int start();

        void set_event_base(struct event_base *base);
        void set_buffer(lcdn::base::BufferQueue * cache)
        {
            _flv_cache = cache;
        }

        int on_connected();
        int on_sent();
        int on_received();
        int on_need_send();

        FLVSenderState get_state()
        {
            return _state;
        }

        void on_timer(const int fd, const short event);

        void event_handler_impl(const int fd, const short event);

        void event_handler_impl_v1(const int fd, const short event);

        void event_handler_impl_v2(const int fd, const short event);

        // only called be outside
        void stop();

        virtual int force_exit()
        {

            LOG_INFO(g_logger, "FLVSender: force_exit, stream_id=" << this->_stream_id.unparse());
            this->stop();

            this->_force_exit = false;
            return 0;
        }

        uint8_t id_version;

        uint64_t get_total_sent_bytes();

    private:
        void _read_handler_impl_v1(const int fd, const short event);
        void _write_handler_impl_v1(const int fd, const short event);

        void _read_handler_impl_v2(const int fd, const short event);
        void _write_handler_impl_v2(const int fd, const short event);

        void _disable_write();
        void _enable_write();

        void _start_timer();
        void _start_timer(uint32_t sec, uint32_t usec);
        void _stop_timer();

        void _stop(bool restart);
        int _idle_time();

    private:
        std::string _receiver_ip;
        uint16_t _receiver_port;
        StreamId_Ext _stream_id;
        int32_t _sock;

        FLVSenderState _state;
        struct event_base *_event_base;
        struct event _event;
        struct event _timer_event;
        bool _timer_enable;
        bool _event_enable;

        lcdn::base::BufferQueue *_flv_cache;
        lcdn::base::Buffer * _send_buf;
        lcdn::base::Buffer * _receive_buf;

        AdminServer *_admin;

        bool _trigger_off;  // true means off; false means on
        int _last_active_time;
        bool _force_exit;

        uint64_t _total_sent_bytes;
};


#endif

