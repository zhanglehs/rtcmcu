/***********************************************************************
* author: zhangcan
* email: zhangcan@youku.com
* date: 2015-02-13
* copyright:youku.com
************************************************************************/

#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "base_client.h"

class FLVMiniBlockClient : public BaseClient{
public:
    FLVMiniBlockClient(struct event_base * main_base, void *bind);
    virtual ~FLVMiniBlockClient();

    enum FLV_CLIENT_REQUEST_STATE {
        FLV_START = 1,
        FLV_STREAM_HEADER = 2,
        FLV_STREAM_FIRST_TAG = 3,
        FLV_STREAM_TAG = 4,
        FLV_STOP = 5 
    };

    virtual bool is_state_stop();
    virtual void set_init_stream_state();

    virtual void set_init_timeoffset(uint32_t ts);

    virtual void request_data();
private:
    void request_live_data();
    void set_stream_state(FLVMiniBlockClient::FLV_CLIENT_REQUEST_STATE st){ _state = st; }
    FLVMiniBlockClient::FLV_CLIENT_REQUEST_STATE get_stream_state(){ return _state; }
private:
    FLVMiniBlockClient::FLV_CLIENT_REQUEST_STATE _state;

};

// class for requesting flv
/*class FLVClient : public BaseClient {
public:
    FLVClient(struct event_base * main_base, void *bind) : BaseClient(main_base, bind) {
		
    }
    virtual ~FLVClient() {}

    enum FLV_CLIENT_REQUEST_STATE {
        FLV_START = 1,
        FLV_STREAM_HEADER = 2,
        FLV_STREAM_FIRST_TAG = 3,
        FLV_STREAM_WAIT_VKEY = 4,
        FLV_STREAM_TAG = 5,
        FLV_FRAGMENT_TRANSFER = 6,
        FLV_STOP = 7
    };

    virtual bool is_state_stop()
    {
        return _state == FLVClient::FLV_STOP ? true : false;
    }
    virtual void set_init_stream_state() {
        set_stream_state(FLVClient::FLV_START);
    }

    virtual void set_init_timeoffset(uint32_t ts) {
        timeoffset = ts;
    }

    virtual void request_data();
private:
    void request_live_data(bool is_audio_only);
    void request_live_fragment_data();
    void set_stream_state(FLVClient::FLV_CLIENT_REQUEST_STATE st){ _state = st; }
    FLVClient::FLV_CLIENT_REQUEST_STATE get_stream_state() { return _state; }
private:
    FLVClient::FLV_CLIENT_REQUEST_STATE _state;
    uint32_t _quick_start_block_seq;
    uint32_t _quick_start_base_timestamp;
};

// class for requesting hls
class HLSClient : public BaseClient {
public:
    HLSClient(struct event_base * main_base, void *bind) : BaseClient(main_base, bind) {

    }
    virtual ~HLSClient() {}

    enum HLS_CLIENT_REQUEST_STATE {
        HLS_START = 1,
        HLS_FRAGMENT_TRANSFER = 2,
        HLS_STOP = 3
    };

    virtual bool is_state_stop()
    {
        return _state == HLSClient::HLS_STOP ? true : false;
    }
    virtual void set_init_stream_state() {
        set_stream_state(HLSClient::HLS_START);
    }
    virtual void set_init_timeoffset(uint32_t ts) {
        timeoffset = ts;
    }
    virtual void request_data();
private:
    void request_m3u8_data();
    void request_ts_data();
    void set_stream_state(HLSClient::HLS_CLIENT_REQUEST_STATE st){ _state = st; }
    HLSClient::HLS_CLIENT_REQUEST_STATE get_stream_state() { return _state; }
private:
    HLSClient::HLS_CLIENT_REQUEST_STATE _state;
};

// class for requesting p2p 
class P2PClient : public BaseClient {
public:
    P2PClient(struct event_base * main_base, void *bind) : BaseClient(main_base, bind) {

    }
    virtual ~P2PClient() {}

    enum P2P_CLIENT_REQUEST_STATE {
        P2P_START = 1,
        P2P_FRAGMENT_FIRST_TRANSFER = 2,
        P2P_FRAGMENT_SECOND_TRANSFER = 3,
        P2P_FRAGMENT_THIRD_TRANSFER = 4,
        P2P_FRAGMENT_TRANSFER = 5,
        P2P_BODY_FINISHED = 6,
        P2P_ERROR_404 = 7,
        P2P_ERROR_500 = 8,
        P2P_STOP = 9
    };

    virtual bool is_state_stop()
    {
        return _state == P2PClient::P2P_STOP ? true : false;
    }
    virtual void set_init_stream_state() {
        set_stream_state(P2PClient::P2P_START);
    }
    virtual void set_init_timeoffset(uint32_t ts) {
        timeoffset = ts;
    }
    virtual void request_data();
private:
    void request_metadata_data();
    void request_timerange_data();
    void request_random_block_data();
    void request_sequential_block_data();
    void request_live_data();
    void set_stream_state(P2PClient::P2P_CLIENT_REQUEST_STATE st){ _state = st; }
    P2PClient::P2P_CLIENT_REQUEST_STATE get_stream_state() { return _state; }
private:
    P2PClient::P2P_CLIENT_REQUEST_STATE _state;
};*/

// class for requesting sdp
class SDPClient : public BaseClient { 
public:
    SDPClient(struct event_base * main_base, void *bind) : BaseClient(main_base, bind) {

    }
    virtual ~SDPClient() {}

    enum SDP_CLIENT_REQUEST_STATE {
        SDP_START = 1,
        SDP_FRAGMENT_TRANSFER = 2,
        SDP_STOP = 3
    };

    virtual bool is_state_stop()
    {
        return _state == SDPClient::SDP_STOP ? true : false;
    }
    virtual void set_init_stream_state() {
        set_stream_state(SDPClient::SDP_START); 
    }

    virtual void set_init_timeoffset(uint32_t ts) {
        timeoffset = ts;
    }

    virtual void request_data();
private:
    void request_sdp_data();
    void set_stream_state(SDPClient::SDP_CLIENT_REQUEST_STATE st){ _state = st; }
    SDPClient::SDP_CLIENT_REQUEST_STATE get_stream_state() { return _state; }
private:
    SDPClient::SDP_CLIENT_REQUEST_STATE _state;
};


#endif /* _CLIENT_H_ */
