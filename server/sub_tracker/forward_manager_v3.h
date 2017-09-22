/*
 * manage for stream_forward map
 *
 * author: duanbingnan@youku.com
 * create: 2015.05.05
 *
 */

#ifndef FORWARD_MANAGER_H
#define FORWARD_MANAGER_H

#include <list>
#include <map>
#include <set>
#include <stdint.h>
#include <time.h>
#include <event.h>
#include <common/proto.h>
#include <common/protobuf/InterliveServerStat.pb.h>
#include <common/protobuf/st2t_fpinfo_packet.pb.h>
#include <common/protobuf/f2t_register_req.pb.h>
#include <utils/buffer.hpp>
#include <util/list.h>
#include <util/ketama_hash.h>
#include <json.h>

static const uint16_t ASN_TEL = 1000;
static const uint16_t ASN_CNC = 2000;
static const uint16_t ASN_CNC_RTP = 4000;
static const uint16_t ASN_TEL_RTP = 3000;
static const uint16_t ASN_PRIVATE = 5000;

static const uint16_t LEVEL0 = 0;
static const uint16_t LEVEL1 = 1;
static const uint16_t LEVEL2 = 2;
static const uint16_t LEVEL3 = 3;

/* start of stream map definition */
typedef struct
{
    uint32_t alias_ip;
    uint16_t alias_asn;
    uint16_t alias_region;
}alias_t;

typedef uint64_t ForwardID;

enum SCORE_TYPE
{
    NET_SPEED = 0,
    CPU_RATE
};


class ForwardServerStat
{
    public:
      ForwardServerStat()
        :sys_uptime(0),
         sys_cpu_cores(0),
         sys_cpu_idle(0),
         sys_mem_total(0),
         sys_mem_total_free(0),
         sys_net_rx(0),
         sys_net_tx(0),
         proc_cpu_use(0),
         proc_sleepavg(0),
         proc_vmpeak(0),
         proc_vmsize(0),
         proc_vmrss(0),
         proc_total_uptime(0),
         proc_process_name(""),
         buss_fsv2_stream_count(0),
         buss_fsv2_total_session(0),
         buss_fsv2_active_session(0),
         buss_fsv2_connection_count(0),
         buss_fcv2_stream_count(0),
         buss_uploader_connection_count(0),
         buss_player_connection_count(0),
         buss_player_online_cnt(0){}
    public:
        uint64_t    sys_uptime;
        int32_t     sys_cpu_cores;
        int32_t     sys_cpu_idle;
        int32_t     sys_mem_total;
        int32_t     sys_mem_total_free;
        uint64_t    sys_net_rx;
        uint64_t    sys_net_tx;
        int32_t     proc_cpu_use;
        int32_t     proc_sleepavg;
        int32_t     proc_vmpeak;
        int32_t     proc_vmsize;
        int32_t     proc_vmrss;
        uint64_t    proc_total_uptime;
        std::string proc_process_name;
        int32_t     buss_fsv2_stream_count;
        int32_t     buss_fsv2_total_session;
        int32_t     buss_fsv2_active_session;
        int32_t     buss_fsv2_connection_count;
        int32_t     buss_fcv2_stream_count;
        int32_t     buss_uploader_connection_count;
        int32_t     buss_player_connection_count;
        int32_t     buss_player_online_cnt;
};

class ForwardServer
{
    public:
        ForwardServer()
        :load_score(-1)
        {}

        ForwardServer(const ForwardServer &o)
        {
            fid = o.fid;
            fstat = o.fstat;
            ip  = o.ip;
            backend_port    = o.backend_port;
            player_port     = o.player_port;
            asn     = o.asn;
            region  = o.region;
            topo_level  = o.topo_level;
            last_keep   = o.last_keep;
            shutdown    = shutdown;
            load_score = o.load_score;
            group_id  = o.group_id;
        }

        void update_load_score(InterliveServerStat& p, SCORE_TYPE type);

    public:
        ForwardID         fid;
        ForwardServerStat fstat;
        uint32_t          ip;
        uint16_t          backend_port;
        uint16_t          player_port;
        uint16_t          asn;
        uint16_t          region;
        uint16_t          topo_level;        /* the topo level in forward hierarchy */
        time_t            last_keep;
        int               shutdown;
        alias_t           aliases[2];
        uint32_t          load_score;
        uint32_t          group_id;
};

class StreamForwardListNode
{
    public:
        StreamForwardListNode(ForwardID fs_id, uint16_t l, uint16_t k, uint32_t s):fid(fs_id), level(l), region(k), stream_cnt(s){}
        StreamForwardListNode(const StreamForwardListNode &o)
        {
            fid     = o.fid;
            level   = o.level;
            region  = o.region;
        }
    public:
        ForwardID   fid;
        uint16_t    level;         /* the level in stream chain */
        uint16_t    region;        /* the region in stream chain*/
        uint32_t    stream_cnt;    /* the same stream count*/
};

class StreamMapNode
{
    public:
        StreamMapNode()
            :stream_id(""), last_active_time(time(0)),has_level_0(false){}
        StreamMapNode(std::string& sid)
            :stream_id(sid), last_active_time(time(0)),has_level_0(false){}
        StreamMapNode(std::string& sid, time_t t)
            :stream_id(sid),last_active_time(t), has_level_0(false){}
    public:
        std::string stream_id;
        time_t   last_active_time;
        bool     has_level_0;
        std::list<StreamForwardListNode> forward_list;
};

class ForwardServerMapNode
{
    public:
        ForwardServerMapNode(){}
        ForwardServerMapNode(const ForwardServerMapNode& o)
        {
            forward_server = o.forward_server;
            ref_cnt        = o.ref_cnt;
        }
    public:
        ForwardServer   forward_server;
        int             ref_cnt;      /* number that be referenced by forward_svr of stream_forward_list_node */
};

/* end of stream map definition */

class ForwardServerManager
{
    public:
        ForwardServerManager();
        ~ForwardServerManager();
        int         init();
        //int fm_clean_all();

        ForwardID   fm_compute_id(uint32_t part1, uint16_t part2);
        server_info fm_get_proper_forward_fp_http_api(std::string& streamid, uint16_t asn);
        int         fm_get_interlive_server_stat_http_api(std::string& rsp_str);
        int         fm_get_interlive_server_stat_http_api(json_object* rsp_json);
        int         fm_generate_forward_fp_set(std::vector<server_info>& forward_fp_set, uint16_t asn);
        ForwardID   fm_add_a_forward_v3(const f2t_register_req_v3 *req, uint16_t topo_level, uint32_t group_id);
        ForwardID   fm_add_a_forward_v4(const f2t_register_req_v4 *req, uint16_t topo_level, uint32_t group_id);
        int         fm_add_to_stream_v3(ForwardID fs_id, f2t_update_stream_req_v3 *req, uint16_t region);
        int         fm_del_from_stream_v3(ForwardID fs_id, const f2t_update_stream_req_v3 *req);
        int         fm_get_proper_forward_v3(const f2t_addr_req_v3 *req, f2t_addr_rsp_v3 *rsp);

        int         convert_streamid_int32_to_string(uint32_t int32_id, std::string& string_id);
        int         convert_streamid_array_to_string(const uint8_t* array_id, std::string& string_id);
        int         convert_streamid_int32_to_array(uint32_t int32_id, uint8_t* array_id);

        int         fm_del_a_forward(ForwardID id);

        int         fm_add_alias(ForwardID fid, uint32_t alias_ip, uint16_t alias_asn, uint16_t alias_region);
        int         fm_active_forward(ForwardID fs_id);
        int         fm_update_forward_server_stat(ForwardID fs_id, InterliveServerStat& p);

        int         fm_cur_forward_num();
        int         fm_cur_stream_num();

        void        fm_keep_update_forward_fp_infos();
        st2t_fpinfo_packet& fm_get_forward_fp_infos();

        void        fm_fpinfo_update(st2t_fpinfo_packet &fpinfos);
        int         fm_fp_set_reinit(list_head_t &root);

        server_info fm_get_proper_forward_nfp_http_api(std::string &streamid, uint32_t group_id);
        void        fm_clear_stream_cnt_per_sec();    

    private:
        int         fm_is_active(ForwardServer &forward);

        int         fm_unreference_forward(ForwardID fid);
        int         fm_reference_forward  (ForwardID fid);

        int         fm_add_one_stream(uint32_t stream_id);
        void        fm_del_zombie(StreamMapNode &node);

        int         fm_add_one_stream_v3(std::string& stream_id);
        int         fm_get_proper_forward_nfp_v3(const f2t_addr_req_v3 *req,f2t_addr_rsp_v3 *rsp);
        int         fm_get_proper_forward_fp_v3(const f2t_addr_req_v3 *req, f2t_addr_rsp_v3 *rsp);
        int         fm_get_proper_receiver_v3(const f2t_addr_req_v3 *req,f2t_addr_rsp_v3 *rsp);


        int         _fm_fp_set_gen(std::vector<server_info>& forward_fp_set, uint16_t asn, list_head_t &root);
    private:
        typedef std::map<std::string, StreamMapNode> StreamNodeMapType;
        StreamNodeMapType _stream_map;

        typedef std::map<ForwardID, ForwardServerMapNode> ForwardServerMapType;
        ForwardServerMapType _forward_map;
        std::list<ForwardID> _forward_list;

        ketama_hash *_tel_shards;
        ketama_hash *_cnc_shards;
        ketama_hash *_tel_rtp_shards;
        ketama_hash *_cnc_rtp_shards;
        ketama_hash *_private_shards;

        ForwardID _opt_forward;
        uint32_t _opt_load_score;

        st2t_fpinfo_packet _fpinfos;
};

#endif

