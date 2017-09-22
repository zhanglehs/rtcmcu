/*
 *
 *
 */

#ifndef __WHITELIST_MANAGER_H
#define __WHITELIST_MANAGER_H

#include <list>
#include <utils/common.h>
#include <common/proto_define.h> // for definition of stream_info
#include "streamid.h"

class WhiteListItem;
class player_stream;
//class Uploader;
typedef __gnu_cxx::hash_map<StreamId_Ext, WhiteListItem> WhiteListMap;

class WhiteListItem
{
public:
    WhiteListItem();
    WhiteListItem(const StreamId_Ext& stream_id, const timeval start_ts);
    WhiteListItem(const WhiteListItem& right);

public:
    StreamId_Ext    streamid;
    timeval         stream_start_ts;
    boolean         is_idle;
//    player_stream   *player_ptr;
//    Uploader        *uploader_ptr;
    uint32_t        idle_sec;
    boolean         stream_mng_created;
    boolean         cache_manager_created;
};

enum WhitelistEvent
{
    WHITE_LIST_EV_START = 1,
    WHITE_LIST_EV_STOP  = 1 << 1
};

class WhitelistObserver
{
public:
    virtual void update(const StreamId_Ext &streamid, WhitelistEvent ev) = 0;
protected:
    virtual ~WhitelistObserver(){}
};

void target_insert_list_seat(stream_info& info);
void target_close_list_seat(uint32_t streamid);
r2p_keepalive* build_r2p_keepalive(r2p_keepalive* rka);
f2p_keepalive* build_f2p_keepalive(f2p_keepalive* ka);

class WhitelistManager
{
    private:
        struct OB_EV
        {
            OB_EV(WhitelistObserver* ob, WhitelistEvent ev)
                :observer(ob), event(ev)
            {}
            OB_EV(const OB_EV &right)
            {
                observer = right.observer;
                event    = right.event;
            }
            OB_EV & operator=(const OB_EV& right)
            {
                observer = right.observer;
                event    = right.event;

                return *this;
            }

            WhitelistObserver   *observer;
            WhitelistEvent      event;
        };

    public:
        typedef std::list<OB_EV> ObserverList;

        WhitelistManager(){}

        int add_observer(WhitelistObserver *observer, WhitelistEvent interest_ev);
        int del_observer(WhitelistObserver *);

        WhiteListMap* get_white_list();
        bool in(const StreamId_Ext& id);

        //player_stream * player_req_stream(uint32_t streamid);
        //player_stream * player_get_stream(uint32_t streamid);

//        Uploader      * uploader_req_stream(uint32_t streamid);
//        Uploader      * uploader_req_stream_v2(StreamId_Ext streamid);

        //int     retrieve_streamid(uint32_t ids[], int array_size);
        //boolean tb_is_src_stream(uint32_t streamid);
        boolean tb_is_src_stream_v2(StreamId_Ext streamid);
        void    add_stream(stream_info& info);
        void    del_stream(uint32_t streamid);
        void del_stream(const StreamId_Ext& streamid);

        r2p_keepalive* build_r2p_keepalive(r2p_keepalive* rka);
        f2p_keepalive* build_f2p_keepalive(f2p_keepalive* ka);

        void player_walk(void (*walk) (player_stream *, void *), void *data);

        int  notify_need_stream(uint32_t streamid);
        void clear();

    private:
        WhiteListMap _white_list;
        ObserverList _observer_list;
};

#endif

