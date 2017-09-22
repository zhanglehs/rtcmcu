/*
 * author: duanbingnan@youku.com
 * create: 2015.05.05
 *
 */

#include "forward_manager_v3.h"

#include <arpa/inet.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <util/log.h>
#include <utils/memory.h>
#include <util/util.h>
#include "tracker_config.h"

using std::list;
using std::map;
using namespace std;

static const int UNACTIVE_INTERVAL = 120;   // second
static const uint32_t NET_BANDWIDTH = 1000*1024*1024;
static const uint32_t LOAD_SCORE_UPPER_BOUND = 50; 
static const uint32_t MAX_SAME_STREAM_CNT = 500;

#undef DEBUG
#define DEBUG

void ForwardServer::update_load_score(InterliveServerStat& p, SCORE_TYPE type)
{
    uint32_t time_interval = 0;
    uint32_t net_rx_diff = 0;
    uint32_t net_tx_diff = 0;

    switch (type)
    {
    case NET_SPEED:
    {
        if (p.has_sys_uptime())
        {
            time_interval = p.sys_uptime() - fstat.sys_uptime;

            if (p.has_sys_net_rx() && p.has_sys_net_tx())
            {
                net_rx_diff = (p.sys_net_rx() - fstat.sys_net_rx) * 8;
                net_tx_diff = (p.sys_net_tx() - fstat.sys_net_tx) * 8;

                load_score = (uint32_t)((net_rx_diff + net_tx_diff) \
                        / (time_interval * 1.0)/NET_BANDWIDTH * 100+1);
            }
            else
            {
                load_score = -1;
            }
        }
        else
        {
            load_score = -1;
        }
        break;
    }
    case CPU_RATE:
    {
        if (p.has_proc_cpu_use())
        {
            load_score = p.proc_cpu_use();
        }
        break;
    }
    defalut:
        break;
    }
}


ForwardServerManager::ForwardServerManager()
    :_opt_load_score(-1) 
{
}

ForwardServerManager::~ForwardServerManager()
{
    /* clean stream_list_node */
    for (StreamNodeMapType::iterator i = _stream_map.begin();
            i != _stream_map.end();
            i++)
    {
        for (std::list<StreamForwardListNode>::iterator li = i->second.forward_list.begin();
                li != i->second.forward_list.end();
                li++)
        {
            fm_unreference_forward(li->fid);
        }
        i->second.forward_list.clear();
    }
    _stream_map.clear();

    /* clean forward_server_list */
    _forward_map.clear();

    if (_tel_shards)
    {
        delete _tel_shards;
        _tel_shards = NULL;
    }

    if (_cnc_shards)
    {
        delete _cnc_shards;
        _cnc_shards = NULL;
    }

    if (_tel_rtp_shards)
    {
        delete _tel_rtp_shards;
        _tel_rtp_shards = NULL;
    }

    if (_cnc_rtp_shards)
    {
        delete _cnc_rtp_shards;
        _cnc_rtp_shards = NULL;
    }

    if (_private_shards)
    {
        delete _private_shards;
        _private_shards = NULL;
    }
}

int
ForwardServerManager::init()
{
    vector<server_info> forward_fp_set;

    tc_get_forward_fp_set(forward_fp_set, ASN_TEL);
    _tel_shards = new ketama_hash(forward_fp_set);
    _tel_shards->init(forward_fp_set);
    forward_fp_set.clear();

    tc_get_forward_fp_set(forward_fp_set, ASN_CNC);
    _cnc_shards = new ketama_hash(forward_fp_set);
    _cnc_shards->init(forward_fp_set);
    forward_fp_set.clear();

    tc_get_forward_fp_set(forward_fp_set, ASN_TEL_RTP);
    _tel_rtp_shards = new ketama_hash(forward_fp_set);
    _tel_rtp_shards->init(forward_fp_set);
    forward_fp_set.clear();

    tc_get_forward_fp_set(forward_fp_set, ASN_CNC_RTP);
    _cnc_rtp_shards = new ketama_hash(forward_fp_set);
    _cnc_rtp_shards->init(forward_fp_set);
    forward_fp_set.clear();

    tc_get_forward_fp_set(forward_fp_set, ASN_PRIVATE);
    _private_shards = new ketama_hash(forward_fp_set);
    _private_shards->init(forward_fp_set);

    return 0;
}

int
ForwardServerManager::fm_cur_forward_num()
{
    int num = 0;
    for (ForwardServerMapType::iterator i = _forward_map.begin();
            i != _forward_map.end(); i++)
    {
        if (fm_is_active(i->second.forward_server))
        {
            num++;
        }
    }

    return num;
}

int
ForwardServerManager::fm_cur_stream_num()
{
    return _stream_map.size();
}


void 
ForwardServerManager::fm_keep_update_forward_fp_infos()
{
    _fpinfos.Clear();
    for (ForwardServerMapType::iterator i = _forward_map.begin();
        i != _forward_map.end(); i++)
    {
        ForwardServerMapNode &node = i->second;
        ForwardID fid = i->first;

        if ((node.forward_server.topo_level == LEVEL1)
            && fm_is_active(node.forward_server))
        {
            st2t_fpinfo_packet::forward_info* fpinfo = _fpinfos.add_fi();

            fpinfo->set_ip(node.forward_server.ip);
            fpinfo->set_port(node.forward_server.backend_port);
            fpinfo->set_asn(node.forward_server.asn);

            size_t i = 0;
            for (; i < sizeof(node.forward_server.aliases) / sizeof(alias_t); i++)
            {
                if (node.forward_server.aliases[i].alias_ip == 0)
                    break;

                st2t_fpinfo_packet::forward_info* fpinfo = _fpinfos.add_fi();
                fpinfo->set_ip(node.forward_server.aliases[i].alias_ip);
                fpinfo->set_port(node.forward_server.backend_port);
                fpinfo->set_asn(node.forward_server.aliases[i].alias_asn);
            }
        }
    }
}

st2t_fpinfo_packet& 
ForwardServerManager::fm_get_forward_fp_infos()
{
    return _fpinfos;
}

void ForwardServerManager::fm_fpinfo_update(st2t_fpinfo_packet &fpinfos)
{
    list_head_t root;

    if (fpinfos.fi_size() < 1)
    {
        WRN("ForwardServerManager::fm_fpinfo_update:fpinfos is empty!");
        return;
    }

    INIT_LIST_HEAD(&root);

    for (int i=0; i<fpinfos.fi_size(); i++)
    {
        forward_set_t *fs=(forward_set_t*)mcalloc(1, sizeof(forward_set_t));
        if (!fs)
        {
            ERR("ForwardServerManager::fm_fpinfo_update:forward set struct calloc failed.");
            return;
        }
        INIT_LIST_HEAD(&fs->next);
        list_add_tail(&fs->next, &root);

        const st2t_fpinfo_packet::forward_info &fpinfo = fpinfos.fi(i);
        fs->start_ip = fpinfo.ip();
        fs->stop_ip = fpinfo.port();
        fs->asn = fpinfo.asn();
        fs->topo_level = 1;

        DBG("ForwardServerManager::fm_fpinfo_update:%d: ip:%s, port:%d", i, util_ip2str(fs->start_ip), fs->stop_ip);
    }


    fm_fp_set_reinit(root);

    // release fs
    list_head_t *pos;
    for (pos = root.next; prefetch(pos->next), pos != &root; )
    {
        forward_set_t *fs = list_entry(pos, forward_set_t, next);
        
        pos = pos->next;

        mfree(fs);
    }
}


int ForwardServerManager::fm_fp_set_reinit(list_head_t &root)
{
    vector<server_info> forward_fp_set;

    _fm_fp_set_gen(forward_fp_set, ASN_TEL, root);
    if (NULL != _tel_shards)
    {
        delete _tel_shards;
        _tel_shards = NULL;
    }
    _tel_shards = new ketama_hash(forward_fp_set);
    _tel_shards->init(forward_fp_set);
    forward_fp_set.clear();

    _fm_fp_set_gen(forward_fp_set, ASN_CNC, root);
    if (NULL != _cnc_shards)
    {
        delete _cnc_shards;
        _cnc_shards = NULL;
    }
    _cnc_shards = new ketama_hash(forward_fp_set);
    _cnc_shards->init(forward_fp_set);
    forward_fp_set.clear();

    _fm_fp_set_gen(forward_fp_set, ASN_TEL_RTP, root);
    if (NULL != _tel_rtp_shards)
    {
        delete _tel_rtp_shards;
        _tel_rtp_shards = NULL;
    }
    _tel_rtp_shards = new ketama_hash(forward_fp_set);
    _tel_rtp_shards->init(forward_fp_set);
    forward_fp_set.clear();

    _fm_fp_set_gen(forward_fp_set, ASN_CNC_RTP, root);
    if (NULL != _cnc_rtp_shards)
    {
        delete _cnc_rtp_shards;
        _cnc_rtp_shards = NULL;
    }
    _cnc_rtp_shards = new ketama_hash(forward_fp_set);
    _cnc_rtp_shards->init(forward_fp_set);
    forward_fp_set.clear();

    _fm_fp_set_gen(forward_fp_set, ASN_PRIVATE, root);
    if (NULL != _private_shards)
    {
        delete _private_shards;
        _private_shards = NULL;
    }
    _private_shards = new ketama_hash(forward_fp_set);
    _private_shards->init(forward_fp_set);


    return 0;
}

int ForwardServerManager::_fm_fp_set_gen(std::vector<server_info>& forward_fp_set, uint16_t asn, list_head_t &root)
{
    list_head_t *pos;

    list_for_each(pos, &root)
    {
        forward_set_t * fs = list_entry(pos, forward_set_t, next);

        uint32_t ip = fs->start_ip;
        uint32_t port = fs->stop_ip;
        if (1 == fs->topo_level && asn == fs->asn)
        {
            server_info item;

            item.name = util_ip2str(ip);
            item.weight = 100; //when real server < 10, weight should be 100, means virtual server = real server * 100
            item.ip = ip;
            item.port = port;
            item.level = fs->topo_level;

            forward_fp_set.push_back(item);
        }
    }

    return 0;
}


ForwardID
ForwardServerManager::fm_compute_id(uint32_t part1, uint16_t part2)
{
    uint64_t id = part1 + time(0);
    return (id << 16) + part2 + time(0);
}

/*
 * generate the forward-fp servers set
 */
int
ForwardServerManager::fm_generate_forward_fp_set(vector<server_info>& forward_fp_set,
        uint16_t asn)
{
    for (ForwardServerMapType::iterator i = _forward_map.begin();
            i != _forward_map.end(); i++)
    {
        ForwardServerMapNode &node = i->second;
        ForwardID fid = i->first;

        if ((node.forward_server.topo_level == LEVEL1)
                && fm_is_active(node.forward_server))
        {
            if(asn == node.forward_server.asn)
            {
                server_info item;

                item.name   = "";
                item.weight = 100; //when real server < 10, weight should be 100, means virtual server = real server * 100
                item.ip     = node.forward_server.ip;
                item.port   = node.forward_server.backend_port;
                item.level  = node.forward_server.topo_level;
                item.forwardid = fid;

                forward_fp_set.push_back(item);
            }
            else if (asn != node.forward_server.asn)
            {
                size_t i = 0;
                for (;i < sizeof(node.forward_server.aliases)/sizeof(alias_t); i++)
                {
                    if (node.forward_server.aliases[i].alias_ip == 0)
                        break;
                    if (asn == node.forward_server.aliases[i].alias_asn)
                    {
                        {
                            server_info item;

                            item.name   = "";
                            item.weight = 100;
                            item.ip     = node.forward_server.aliases[i].alias_ip;
                            item.port   = node.forward_server.backend_port;
                            item.level  = node.forward_server.topo_level;
                            item.forwardid = fid;

                            forward_fp_set.push_back(item);
                        }
                    }
                }
            }
        }

    }

    //set name
    stringstream index;

    for (size_t i = 0; i < forward_fp_set.size(); i++)
    {
        index << i;
        forward_fp_set[i].name = "forwardfp" + index.str();
        index.str("");
    }

#ifdef DEBUG
    vector<server_info>::iterator iter;
    for(iter = forward_fp_set.begin();
        iter != forward_fp_set.end();
        iter++)
    {
        DBG("forward_fp_set item,ip:%s, port:%u, level: %u, name: %s, weight: %d, forwardid: %lu",
            util_ip2str((*iter).ip),
            (*iter).port,
            (*iter).level,
            (*iter).name.c_str(),
            (*iter).weight,
            (*iter).forwardid);
    }
#endif

    return 0;
}

/*
 * receiver, forward-fp, forward-nfp register into _forward_map
 */
ForwardID
ForwardServerManager::fm_add_a_forward_v3(const f2t_register_req_v3 *req,
        uint16_t topo_level, uint32_t group_id)
{
    /* TODO
    if (topo_level > g_config->child_topo_level)
        return -1;
    */

    ForwardID fid = fm_compute_id(req->port, req->ip);

    /* _forward_map permit the same fid exists more than one
     * TODO: why?
     * cause of delay deletion ?
     * here this for loop propose to delete all the non-keepalived
     * forwards in forward map.
     */
    for (ForwardServerMapType::iterator fmi = _forward_map.begin();
            fmi != _forward_map.end();)
    {
        ForwardServerMapType::iterator tfmi = fmi++;
        if (tfmi->second.forward_server.ip == req->ip
            && tfmi->second.forward_server.backend_port == req->port)
        {
            tfmi->second.forward_server.shutdown = 1;   // shutdown the server
        }

        if (!fm_is_active(tfmi->second.forward_server)
            && tfmi->second.ref_cnt == 0)
        {
            INF("remove a forward, fid: %lu, ip: %s, port: %u, asn: %u, region: %u, level: %u, group_id: %u",
                tfmi->first, // forward id
                util_ip2str(tfmi->second.forward_server.ip),
                tfmi->second.forward_server.backend_port,
                tfmi->second.forward_server.asn,
                tfmi->second.forward_server.region,
                tfmi->second.forward_server.topo_level,
                tfmi->second.forward_server.group_id);
            _forward_list.remove(tfmi->first);
            _forward_map.erase(tfmi);
        }
    }

    ForwardServerMapNode new_forward;
    new_forward.forward_server.ip           = req->ip;
    new_forward.forward_server.backend_port = req->port;
    new_forward.forward_server.fid          = fid;
    new_forward.forward_server.asn          = req->asn;
    new_forward.forward_server.region       = req->region;
    new_forward.forward_server.topo_level   = topo_level;
    new_forward.forward_server.shutdown     = 0;
    new_forward.forward_server.last_keep    = time(0);
    new_forward.forward_server.group_id    = group_id;

    for (size_t i = 0; i < sizeof(new_forward.forward_server.aliases)/sizeof(alias_t); i++)
    {
        new_forward.forward_server.aliases[i].alias_ip = 0;
        new_forward.forward_server.aliases[i].alias_asn = 0;
        new_forward.forward_server.aliases[i].alias_region = 0;
    }

    new_forward.ref_cnt = 0;
    INF("add a forward. fid: %lu, ip: %s, port: %u, asn: %u, region: %u, level:%u, group_id:%u",
            new_forward.forward_server.fid,
            util_ip2str(req->ip),
            req->port,
            req->asn,
            req->region,
            topo_level,
            group_id);

    _forward_map.insert(std::make_pair<ForwardID, ForwardServerMapNode>(fid, new_forward));
    _forward_list.push_back(fid);

#ifdef DEBUG
    ForwardServerMapType::iterator iter = _forward_map.begin();
    for(;iter != _forward_map.end();iter++)
    {
        DBG("_forward_map item, ip:%s, port:%u,fid:%lu, asn:%d, region:%d, topo_level:%d, shutdown:%d, group_id:%u",
            util_ip2str(iter->second.forward_server.ip),
            iter->second.forward_server.backend_port,
            iter->second.forward_server.fid,
            iter->second.forward_server.asn,
            iter->second.forward_server.region,
            iter->second.forward_server.topo_level,
            iter->second.forward_server.shutdown,
            iter->second.forward_server.group_id);
    }
#endif

    return fid;
}

ForwardID
ForwardServerManager::fm_add_a_forward_v4(const f2t_register_req_v4 *req,
        uint16_t topo_level, uint32_t group_id)
{
    DBG("fm_add_a_forward_v4");
    /* TODO
    if (topo_level > g_config->child_topo_level)
        return -1;
    */

    ForwardID fid = fm_compute_id(req->backend_port(), req->ip());

    /* _forward_map permit the same fid exists more than one
     * TODO: why?
     * cause of delay deletion ?
     * here this for loop propose to delete all the non-keepalived
     * forwards in forward map.
     */
    for (ForwardServerMapType::iterator fmi = _forward_map.begin();
            fmi != _forward_map.end();)
    {
        ForwardServerMapType::iterator tfmi = fmi++;
        if (tfmi->second.forward_server.ip == req->ip()
            && tfmi->second.forward_server.backend_port == req->backend_port()
            && tfmi->second.forward_server.player_port == req->player_port())
        {
            tfmi->second.forward_server.shutdown = 1;   // shutdown the server
        }

        if (!fm_is_active(tfmi->second.forward_server)
            && tfmi->second.ref_cnt == 0)
        {
            INF("remove a forward, fid: %lu, ip: %s, port: %u, asn: %u, region: %u, level: %u, group_id: %u",
                tfmi->first, // forward id
                util_ip2str(tfmi->second.forward_server.ip),
                tfmi->second.forward_server.backend_port,
                tfmi->second.forward_server.asn,
                tfmi->second.forward_server.region,
                tfmi->second.forward_server.topo_level,
                tfmi->second.forward_server.group_id);
            _forward_list.remove(tfmi->first);
            _forward_map.erase(tfmi);
        }
    }

    ForwardServerMapNode new_forward;
    new_forward.forward_server.ip           = req->ip();
    new_forward.forward_server.backend_port = req->backend_port();
    new_forward.forward_server.player_port  = req->player_port();
    new_forward.forward_server.fid          = fid;
    new_forward.forward_server.asn          = req->asn();
    new_forward.forward_server.region       = req->region();
    new_forward.forward_server.topo_level   = topo_level;
    new_forward.forward_server.shutdown     = 0;
    new_forward.forward_server.last_keep    = time(0);
    new_forward.forward_server.group_id    = group_id;

    for (size_t i = 0; i < sizeof(new_forward.forward_server.aliases)/sizeof(alias_t); i++)
    {
        new_forward.forward_server.aliases[i].alias_ip = 0;
        new_forward.forward_server.aliases[i].alias_asn = 0;
        new_forward.forward_server.aliases[i].alias_region = 0;
    }

    new_forward.ref_cnt = 0;
    INF("add a forward. fid: %lu, ip: %s, backend_port: %u, player_port: %u, asn: %u, region: %u, level:%u, group_id:%u",
            new_forward.forward_server.fid,
            util_ip2str(req->ip()),
            req->backend_port(),
            req->player_port(),
            req->asn(),
            req->region(),
            topo_level,
            group_id);

    _forward_map.insert(std::make_pair<ForwardID, ForwardServerMapNode>(fid, new_forward));
    _forward_list.push_back(fid);

#ifdef DEBUG
    ForwardServerMapType::iterator iter = _forward_map.begin();
    for(;iter != _forward_map.end();iter++)
    {
        DBG("_forward_map item, ip:%s, port:%u,fid:%lu, asn:%d, region:%d, topo_level:%d, shutdown:%d, group_id:%u",
            util_ip2str(iter->second.forward_server.ip),
            iter->second.forward_server.backend_port,
            iter->second.forward_server.fid,
            iter->second.forward_server.asn,
            iter->second.forward_server.region,
            iter->second.forward_server.topo_level,
            iter->second.forward_server.shutdown,
            iter->second.forward_server.group_id);
    }
#endif

    return fid;
}



/*
 * on receiver, forward-fp, forward-nfp have new streams, report to tracker and update _stream_map
 */
int
ForwardServerManager::fm_add_to_stream_v3(ForwardID fs_id,
        f2t_update_stream_req_v3 *req, uint16_t region)
{
    if (req->level > g_config->child_topo_level)
    {
        WRN("fm_add_to_stream: invalid level: %d", req->level);
        return -1;
    }

    string streamid;
    convert_streamid_array_to_string(req->streamid, streamid);

    DBG("fm_add_to_stream_v3: streamid=%s", streamid.c_str());

    /*find stream in _stream_map*/
    StreamNodeMapType::iterator mi = _stream_map.find(streamid);
    if (mi == _stream_map.end())
    {
        if (fm_add_one_stream_v3(streamid) != 0)
        {
            WRN("add new stream error. stream_id: %s", streamid.c_str());
            return -1;
        }
        mi = _stream_map.find(streamid);
        assert(mi != _stream_map.end());
    }

    if (mi->second.has_level_0 == true && req->level == LEVEL0)
    {
        WRN("do not permit 2 different forwards of level 0 have the same stream. REJECT");
        // TODO
        //return -1;
        return 0;
    }

    /*find forward_id in _forward_map*/
    ForwardServerMapType::iterator fmi = _forward_map.find(fs_id);
    if (fmi == _forward_map.end())
    {
        WRN("the forward being added to stream list IS NOT an valid one. REJECT");
        // TODO
        return -1;
    }
    ForwardServer &forward = fmi->second.forward_server;

    /*when child_topo_level > 2, make sure for one stream, only contain one LEVEL2 forward-nfp in one region*/
    if (LEVEL2 == req->level)
    {
        list<StreamForwardListNode>::iterator li = mi->second.forward_list.begin();
        for (; li != mi->second.forward_list.end(); li++)
        {
            if ((li->level == req->level) && (li->region == region) && (li->fid != fs_id))
            {
                WRN("already have a LEVEL2 forward for this region. level+1");
                if (req->level < forward.topo_level)
                {
                    //req->level += 1;
                    req->level = LEVEL3;
                }
            }
        }
    }

    INF("add forward server(ip: %s, port: %u, asn: %u, region: %u, level: %u) to stream list(stream_id: %s)",
            util_ip2str(forward.ip),
            forward.backend_port,
            forward.asn,
            forward.region,
            req->level,
            streamid.c_str());

    //update the forward server info
    //fmi->second.forward_server = forward;

    /* update the existed one */
    list<StreamForwardListNode>::iterator li = mi->second.forward_list.begin();
    for (; li != mi->second.forward_list.end(); li++)
    {
        if (li->fid == fs_id)
        {
            li->level = req->level;
            li->region = region;
            break;
        }
    }

    /* or, create a new one */
    if (li == mi->second.forward_list.end())
    {
        mi->second.forward_list.push_back(StreamForwardListNode(fs_id, req->level, region, 1));
        fm_reference_forward(fs_id);
    }

    if (req->level == LEVEL0)
    {
        mi->second.has_level_0 = true;
    }

#ifdef DEBUG
    StreamNodeMapType::iterator iter = _stream_map.begin();
    DBG("_stream_map: count: %d", _stream_map.size());
    for(;iter != _stream_map.end();iter++)
    {
        DBG("_stream_map item, first:%s, stream_id:%s, last_active_time:%lu,has_level_0:%d",
            iter->first.c_str(),
            iter->second.stream_id.c_str(),
            iter->second.last_active_time,
            iter->second.has_level_0);

        list<StreamForwardListNode>::iterator li = iter->second.forward_list.begin();
        for (; li != iter->second.forward_list.end(); li++)
        {

            ForwardServerMapType::iterator mi = _forward_map.find(li->fid);

            ForwardServer &forward1 = mi->second.forward_server;

            DBG("forward_list item, fid:%lu, ip:%s, level:%d (runtime level), region:%d, \
                    stream_cnt:%d", (*li).fid, util_ip2str(forward1.ip), (*li).level, \
                    (*li).region, (*li).stream_cnt);
        }
    }
#endif

    return 0;
}

int ForwardServerManager::fm_add_one_stream_v3(std::string& stream_id)
{
    if (_stream_map.find(stream_id) == _stream_map.end())
    {
        _stream_map.insert(std::make_pair<std::string, StreamMapNode>(stream_id, StreamMapNode(stream_id, time(0))));
    }

    return 0;
}

/*
* on receiver, forward-fp, forward-nfp, streams not exist anymore, report to tracker and update _stream_map
*/
int
ForwardServerManager::fm_del_from_stream_v3(ForwardID fs_id,
        const f2t_update_stream_req_v3 *req)
{
    string streamid;
    convert_streamid_array_to_string(req->streamid, streamid);

    DBG("fm_del_from_stream_v3: streamid=%s", streamid.c_str());

    StreamNodeMapType::iterator i = _stream_map.find(streamid);
    if (i != _stream_map.end())
    {
        list<StreamForwardListNode> &forward_list = i->second.forward_list;
        list<StreamForwardListNode>::iterator li = forward_list.begin();
        for (; li != forward_list.end(); li++)
        {
            if(li->fid == fs_id)
            {
                break;
            }
        }
        if (li != forward_list.end())
        {
            ForwardServerMapType::iterator mi = _forward_map.find(li->fid);
            assert (mi != _forward_map.end());
            ForwardServer &forward = mi->second.forward_server;
            INF("del forward server(ip: %s,port: %u,asn: %u,region: %u,level: %u) from list of stream_id:%s",
                    util_ip2str(forward.ip),
                    forward.backend_port,
                    forward.asn,
                    forward.region,
                    forward.topo_level,
                    streamid.c_str());
            fm_unreference_forward(li->fid);
            if(i->second.has_level_0 == true)
            {
                i->second.has_level_0 = false;
            }

            INF("del forward server, li->level:%d", li->level);

            forward_list.erase(li);
        }
    }
    return 0;
}

/* return 0 if succ
 * otherwise if failed
 */
int
ForwardServerManager::fm_get_proper_forward_v3(const f2t_addr_req_v3 *req,
        f2t_addr_rsp_v3 *rsp)
{
    /* TODO
     * 1, think about each forward server's load
     */
    string streamid;
    convert_streamid_array_to_string(req->streamid, streamid);

    INF("looking up forward parent of stream_id:%s, for ip:%s, port:%u, asn: %u, region: %u, level: %u",
            streamid.c_str(),
            util_ip2str(req->ip),
            req->port,
            req->asn,
            req->region,
            req->level);

    int found = 0;
    if (req->level >= LEVEL2)
    {
        found = fm_get_proper_forward_nfp_v3(req, rsp);//lookup forward-nfp for level 3 forward-nfp

        if(found)
        {
            INF("found in fm_get_proper_forward_nfp_v3");
        }

        if (!found)
        {
            
            found = fm_get_proper_forward_fp_v3(req, rsp);//lookup forward-fp for level 2 forward-nfp

            if(found)
            {
                INF("found in fm_get_proper_forward_fp_v3");
            }

        }
    }
    else
    {
        found = fm_get_proper_receiver_v3(req, rsp);//lookup receiver for forward-fp
    }
    return found ? 0 : 1;
}

/*
 * get proper forward from g_stream_map, loopup forward-nfp for level 3 forward-nfp
 */
int
ForwardServerManager::fm_get_proper_forward_nfp_v3(const f2t_addr_req_v3 *req,
        f2t_addr_rsp_v3 *rsp)
{
    string streamid;

    uint32_t group_id = 0;

    int flag = tc_get_group_id(req->ip, &group_id);
    if (flag == -1)
    {
        ERR("ForwardServerManager::fm_get_proper_forward_nfp_v3:%s hasn't register to subtracker!", util_ip2str(req->ip));
        return 0;
    }

    convert_streamid_array_to_string(req->streamid, streamid);

    INF("looking up forward nfp of stream_id:%s, for ip:%s, port:%u, asn: %u, region: %u, level: %u",
            streamid.c_str(),
            util_ip2str(req->ip),
            req->port,
            req->asn,
            req->region,
            req->level);

    int found = 0;
    /* 1, look up stream forward list preferred */
    for (StreamNodeMapType::iterator si = _stream_map.begin();
            si != _stream_map.end();)
    {
        StreamNodeMapType::iterator tsi = si++;
        StreamMapNode &node = tsi->second;

        /* delete the stream entry if there's no active forward server
         * (or receiver) exist
         */
        if (node.forward_list.empty()
                && (time(0) - node.last_active_time) > 60)
        {
            INF("delete an unactive stream, stream_id: %s.", node.stream_id.c_str());
            _stream_map.erase(tsi);
            continue;
        }

        if(node.stream_id == streamid)
        {
            node.last_active_time = time(0);

            /* 2, look up for proper forward server */
            uint16_t last_level = LEVEL0;
            for (list<StreamForwardListNode>::iterator li = node.forward_list.begin();
                    li != node.forward_list.end();)// li++)
            {
                list<StreamForwardListNode>::iterator tli = li++;
                ForwardServerMapType::iterator fi = _forward_map.find(tli->fid);
                assert (fi != _forward_map.end());

                ForwardServerMapNode &server_node = fi->second;

                if (!fm_is_active(server_node.forward_server)
                        || server_node.ref_cnt <= 0)
                {
                    INF("delete a zombie streams: ip: %s, port: %u, ref_cnt: %d, stream_id: %s",
                        util_ip2str(server_node.forward_server.ip),
                        server_node.forward_server.backend_port,
                        server_node.ref_cnt,
                        node.stream_id.c_str());
                    /* do not return forward server that's unactive */
                    fm_unreference_forward(tli->fid);
                    if(node.has_level_0 == true)
                    {
                        node.has_level_0 = false;
                    }
                    node.forward_list.erase(tli);
                    continue;
                }

                TRC("the ip of node being compared is: ip: %s; port: %u; asn: %u; region: %u; level: %u",
                        util_ip2str(server_node.forward_server.ip),
                        server_node.forward_server.backend_port,
                        server_node.forward_server.asn,
                        server_node.forward_server.region,
                        tli->level);
                if ((group_id == server_node.forward_server.group_id)
                        && (req->asn == server_node.forward_server.asn)
                        && (req->region == server_node.forward_server.region))
                {
                    if (req->ip != server_node.forward_server.ip
                            || req->port != server_node.forward_server.backend_port)
                    {
                        if (tli->level > last_level
                                && tli->level < req->level)
                        {
                            rsp->ip     = server_node.forward_server.ip;
                            rsp->port   = server_node.forward_server.backend_port;
                            rsp->level  = tli->level;

                            last_level = rsp->level;

                            found = 1;

                            INF("the response forward_gen ip:%s, port:%u,level: %u",
                                    util_ip2str(rsp->ip),rsp->port,rsp->level);
                            break;
                        }
                    }
                }
            }
            break;
        }
        else
        {
            fm_del_zombie(node);
        }
    }

    return found;
}

/* return 0 if not found
 * otherwise if found
 * loopup forward-fp for level 2 forward-nfp
 */
int
ForwardServerManager::fm_get_proper_forward_fp_v3(const f2t_addr_req_v3 *req,
        f2t_addr_rsp_v3 *rsp)
{   
    string streamid;
    convert_streamid_array_to_string(req->streamid, streamid);

    INF("looking up forward fp of stream_id:%s, for ip:%s, port:%u, asn: %u, region: %u, level: %u",
            streamid.c_str(),
            util_ip2str(req->ip),
            req->port,
            req->asn,
            req->region,
            req->level);

    /*consistent hash select a forward-fp server(whose level is 1) for those request that level >= 2 */
    int found = 0;
    server_info  result;

    if (g_config->network_type == 1)
    {
        result = _private_shards->get_server_info(streamid);
    }
    else
    {
        if (req->asn == ASN_TEL)
        {
            result = _tel_shards->get_server_info(streamid);
        }
        else if (req->asn == ASN_CNC)
        {
            result = _cnc_shards->get_server_info(streamid);
        }
        else if (req->asn == ASN_TEL_RTP)
        {
            result = _tel_rtp_shards->get_server_info(streamid);
        }
        else if (req->asn == ASN_CNC_RTP)
        {
            result = _cnc_rtp_shards->get_server_info(streamid);
        }
        else
        {
            WRN("fm_get_proper_forward_fp : not supported asn!\n");
            return found;
        }
    }

    rsp->ip = result.ip;
    rsp->port = result.port;
    rsp->level = result.level;

    INF("the response forward fp ip:%s, port:%u, level: %u",
        util_ip2str(rsp->ip), rsp->port, rsp->level);

    found = 1;

    return found;
}

/* return 0 if not found
 * otherwise if found
 * loopup receiver for forward-fp
 */
int
ForwardServerManager::fm_get_proper_receiver_v3(const f2t_addr_req_v3 *req,
        f2t_addr_rsp_v3 *rsp)
{

    string streamid;
    convert_streamid_array_to_string(req->streamid, streamid);

    INF("looking up receiver of stream_id:%s, for ip:%s, port:%u, asn: %u, region: %u, level: %u",
            streamid.c_str(),
            util_ip2str(req->ip),
            req->port,
            req->asn,
            req->region,
            req->level);

    int found = 0;

    /* 1, look up stream forward list preferred */
    for (StreamNodeMapType::iterator si = _stream_map.begin();
            si != _stream_map.end();)
    {
        StreamNodeMapType::iterator tsi = si++;
        StreamMapNode &node = tsi->second;

        /* delete the stream entry if there's no active forward server
         * (or receiver) exist
         */
        if (tsi->second.forward_list.empty()
                && (time(0) - node.last_active_time) > 60)
        {
            INF("delete an unactive stream, stream_id: %s.", node.stream_id.c_str());
            _stream_map.erase(tsi);
            continue;
        }

        if(node.stream_id == streamid)
        {
            node.last_active_time = time(0);

            /* 2, look up for proper forward server */
            for (list<StreamForwardListNode>::iterator li = node.forward_list.begin();
                    li != node.forward_list.end();)
            {
                list<StreamForwardListNode>::iterator tli = li++;

                ForwardServerMapType::iterator fmi = _forward_map.find(tli->fid);
                assert (fmi != _forward_map.end());

                ForwardServerMapNode &server_node = fmi->second;

                if (!fm_is_active(server_node.forward_server)
                        || server_node.ref_cnt <= 0)
                {
                    INF("delete a zombie streams: ip: %s, port: %u, ref_cnt: %d, stream_id: %s",
                        util_ip2str(server_node.forward_server.ip),
                        server_node.forward_server.backend_port,
                        server_node.ref_cnt,
                        node.stream_id.c_str());
                    /* do not return forward server that's unactive */
                    // TODO: need further optimization
                    fm_unreference_forward(tli->fid);
                    if(node.has_level_0 == true)
                    {
                        node.has_level_0 = false;
                    }
                    node.forward_list.erase(tli);
                    continue;
                }

                DBG("the ip of node being compared is: ip: %s; port: %u; asn: %u; region: %u; level: %u",
                        util_ip2str(server_node.forward_server.ip),
                        server_node.forward_server.backend_port,
                        server_node.forward_server.asn,
                        server_node.forward_server.region,
                        tli->level);
                if (!found && tli->level == LEVEL0)
                {
                    if (req->ip != server_node.forward_server.ip
                            || req->port != server_node.forward_server.backend_port)
                    {
                        rsp->ip     = server_node.forward_server.ip;
                        rsp->port   = server_node.forward_server.backend_port;
                        rsp->level  = tli->level;

                        found = 1;
                    }
                    if (req->asn != server_node.forward_server.asn)
                    {
                        size_t i = 0;
                        for (;i < sizeof(server_node.forward_server.aliases)/sizeof(alias_t); i++)
                        {
                            if (server_node.forward_server.aliases[i].alias_ip == 0)
                                break;
                            if (req->asn == server_node.forward_server.aliases[i].alias_asn)
                            {
                                if (req->ip != server_node.forward_server.aliases[i].alias_ip
                                        || req->port != server_node.forward_server.backend_port)
                                {
                                    rsp->ip = server_node.forward_server.aliases[i].alias_ip;
                                    rsp->port = server_node.forward_server.backend_port;
                                    rsp->level = tli->level;

                                    assert(tli->level == server_node.forward_server.topo_level);
                                    found = 1;
                                    INF("the response receiver ip:%s, port:%u,level: %u",
                                            util_ip2str(rsp->ip),rsp->port,rsp->level);
                                }
                            }
                        }
                    }
                    break;
                }
            }
            break;
        }
        else
        {
            fm_del_zombie(node);
        }

    }

    return found;// ? 0 : 1;
}

/*
* convert uint32_t streamid into string streamid
*/
int
ForwardServerManager::convert_streamid_int32_to_string(uint32_t int32_id, std::string& string_id)
{
    char str[40];
    memset(str, 0x00, sizeof(str));
    sprintf(str, "%032X", int32_id);
    string_id.assign(str, 32);
    return 0;
}

/*
* convert char array[16] streamid into string streamid
*/
int
ForwardServerManager::convert_streamid_array_to_string(const uint8_t* array_id, std::string& string_id)
{
    char str[40];
    memset(str, 0x00, sizeof(str));
    for (int i = 0; i < 16; i++)
    {
        sprintf((char*)str + i*2, "%02X", array_id[15-i]);
    }
    string_id.assign(str, 32);
    return 0;
}

/*
* convert uint32_t streamid into char array[16] streamid for v1,v2, to v3 transition
*/
int
ForwardServerManager::convert_streamid_int32_to_array(uint32_t int32_id, uint8_t* array_id)
{
    char str[40];
    memset(str, 0x00, sizeof(str));
    sprintf(str, "%032X", int32_id);

    //big endian
    for (int i = 0; i < 16; i++)
    {
        int ii;
        sscanf(str + 2 * i, "%2X", &ii);
        array_id[15 - i] = ii;
    }
    return 0;
}

/*
 * set forward shutdown flag to true
 */
int
ForwardServerManager::fm_del_a_forward(ForwardID id)
{
    INF("fm_del_a_forward, id: %lu", id);
    ForwardServerMapType::iterator i = _forward_map.find(id);
    if (i != _forward_map.end()
            && fm_is_active(i->second.forward_server))
    {
        i->second.forward_server.shutdown = 1;
    }

    return 0;
}

/*
* only receiver restart could call this function, why?
*/
void
ForwardServerManager::fm_del_zombie(StreamMapNode &node)
{
    if (node.last_active_time < time(0)
            && (time(0) - node.last_active_time) > 60)
    {
        // free zombie streams
        for (list<StreamForwardListNode>::iterator li = node.forward_list.begin();
                li != node.forward_list.end();)
        {
            list<StreamForwardListNode>::iterator tli = li++;
            ForwardServerMapType::iterator mi = _forward_map.find(tli->fid);
            if (mi == _forward_map.end())
            {
                INF("ForwardServerManager::fm_del_zombie: fid is invalid forward!");
                if (node.has_level_0 == true)
                {
                    node.has_level_0 = false;
                }
                node.forward_list.erase(tli);
                continue;
            }

            if (!fm_is_active(mi->second.forward_server)
                    || mi->second.ref_cnt <= 0)
            {
                INF("ForwardServerManager::fm_del_zombie:delete a zombie streams: \
                        ip: %s, port: %u, ref_cnt: %d, stream_id: %s", \
                        util_ip2str(mi->second.forward_server.ip), \
                        mi->second.forward_server.backend_port, \
                        mi->second.ref_cnt, \
                        node.stream_id.c_str());
                // TODO need further optimization
                fm_unreference_forward(tli->fid);
                if (node.has_level_0 == true)
                {
                    node.has_level_0 = false;
                }
                node.forward_list.erase(tli);
            }
        }

    }
}

int
ForwardServerManager::fm_reference_forward(ForwardID fid)
{
    ForwardServerMapType::iterator i = _forward_map.find(fid);
    if (i == _forward_map.end())
    {
        return -1;
    }

    i->second.ref_cnt += 1;
    return 0;
}

int
ForwardServerManager::fm_unreference_forward(ForwardID fid)
{
    ForwardServerMapType::iterator i = _forward_map.find(fid);
    if (i != _forward_map.end())
    {
        i->second.ref_cnt--;
        if (i->second.ref_cnt < 0)
        {
            i->second.ref_cnt = 0;
        }
        return 0;
    }
    return -1;
}

/*
 * keep alive
 */
int
ForwardServerManager::fm_active_forward(ForwardID fid)
{
    /* TODO */
    /* is relative to is_active of ForwardServerMapNode ? */
    ForwardServerMapType::iterator i = _forward_map.find(fid);
    if (i != _forward_map.end())
    {
        i->second.forward_server.last_keep = time(0);
        return 0;
    }
    return -1;
}

/*
 * update forward server stat
 */
int
ForwardServerManager::fm_update_forward_server_stat(ForwardID fid, InterliveServerStat& p)
{

    ForwardServerMapType::iterator i = _forward_map.find(fid);
    if (i != _forward_map.end())
    {
        // update forward_server's load_score
        i->second.forward_server.update_load_score(p, CPU_RATE);
        uint32_t fs_load_score = i->second.forward_server.load_score;

        
        if (_opt_load_score < 0)
        {
            _opt_load_score = fs_load_score;
            _opt_forward = fid;
        }
        else
        {
            if (fs_load_score < _opt_load_score)
            {
                _opt_load_score = fs_load_score;
                _opt_forward = fid;
            }
        }

        if (p.has_sys_uptime())
        {
            i->second.forward_server.fstat.sys_uptime = p.sys_uptime();
        }
        if (p.has_sys_cpu_cores())
        {
            i->second.forward_server.fstat.sys_cpu_cores = p.sys_cpu_cores();
        }
        if (p.has_sys_cpu_idle())
        {
            i->second.forward_server.fstat.sys_cpu_idle = p.sys_cpu_idle();
        }
        if (p.has_sys_mem_total())
        {
            i->second.forward_server.fstat.sys_mem_total = p.sys_mem_total();
        }
        if (p.has_sys_mem_total_free())
        {
            i->second.forward_server.fstat.sys_mem_total_free = p.sys_mem_total_free();
        }
        if (p.has_sys_net_rx())
        {
            i->second.forward_server.fstat.sys_net_rx = p.sys_net_rx();
        }
        if (p.has_sys_net_tx())
        {
            i->second.forward_server.fstat.sys_net_tx = p.sys_net_tx();
        }
        if (p.has_proc_cpu_use())
        {
            i->second.forward_server.fstat.proc_cpu_use = p.proc_cpu_use();
        }
        if (p.has_proc_sleepavg())
        {
            i->second.forward_server.fstat.proc_sleepavg = p.proc_sleepavg();
        }
        if (p.has_proc_vmpeak())
        {
            i->second.forward_server.fstat.proc_vmpeak = p.proc_vmpeak();
        }
        if (p.has_proc_vmsize())
        {
            i->second.forward_server.fstat.proc_vmsize = p.proc_vmsize();
        }

        if (p.has_proc_vmrss())
        {
            i->second.forward_server.fstat.proc_vmrss = p.proc_vmrss();
        }

        if (p.has_proc_total_uptime())
        {
            i->second.forward_server.fstat.proc_total_uptime = p.proc_total_uptime();
        }
        if (p.has_proc_process_name())
        {
            i->second.forward_server.fstat.proc_process_name = p.proc_process_name();
        }
        if (p.has_buss_fsv2_stream_count())
        {
            i->second.forward_server.fstat.buss_fsv2_stream_count = p.buss_fsv2_stream_count();
        }
        if (p.has_buss_fsv2_total_session())
        {
            i->second.forward_server.fstat.buss_fsv2_total_session = p.buss_fsv2_total_session();
        }
        if (p.has_buss_fsv2_active_session())
        {
            i->second.forward_server.fstat.buss_fsv2_active_session = p.buss_fsv2_active_session();
        }
        if (p.has_buss_fsv2_connection_count())
        {
            i->second.forward_server.fstat.buss_fsv2_connection_count = p.buss_fsv2_connection_count();
        }
        if (p.has_buss_fcv2_stream_count())
        {
            i->second.forward_server.fstat.buss_fcv2_stream_count = p.buss_fcv2_stream_count();
        }
        if (p.has_buss_uploader_connection_count())
        {
            i->second.forward_server.fstat.buss_uploader_connection_count = p.buss_uploader_connection_count();
        }
        if (p.has_buss_player_connection_count())
        {
            i->second.forward_server.fstat.buss_player_connection_count = p.buss_player_connection_count();
        }
        if (p.has_buss_player_online_cnt())
        {
            i->second.forward_server.fstat.buss_player_online_cnt = \
                p.buss_player_online_cnt();
        }

        ForwardServer fs = i->second.forward_server;
        DBG("ForwardServerManager::fm_update_forward_server_stat: ip:%s, proc_cpu_use:%d, proc_vmrss:%d, load_score:%d, \
                fcv2 stream cnt:%d, buss_player_online_cnt:%d", util_ip2str(fs.ip), fs.fstat.proc_cpu_use, fs.fstat.proc_vmrss,\
                fs.load_score, fs.fstat.buss_fcv2_stream_count, \
                fs.fstat.buss_player_online_cnt);

        return 0;
    }

    DBG("fm_update_forward_server_stat, do not find forward = %lu in _forward_map", fid);
    return -1;
}

/* return 0 means unactive;
 * otherwise active
 */
int
ForwardServerManager::fm_is_active(ForwardServer &forward)
{
    time_t now = time(0);
    if (forward.last_keep > now)
        forward.last_keep = now;

    if ((now - forward.last_keep) > UNACTIVE_INTERVAL)
    {
        return 0;
    }
    if (forward.shutdown)
    {
        return 0;
    }
    return 1;
}

int
ForwardServerManager::fm_add_alias(ForwardID fid, uint32_t alias_ip,
        uint16_t alias_asn, uint16_t alias_region)
{
    ForwardServerMapType::iterator mi = _forward_map.find(fid);
    if (mi == _forward_map.end())
    {
        return -1;
    }
    ForwardServer &server = mi->second.forward_server;

    for (size_t i = 0; i < sizeof(server.aliases)/sizeof(alias_t); i++)
    {
        if (server.aliases[i].alias_ip == 0)
        {
            server.aliases[i].alias_ip = alias_ip;
            server.aliases[i].alias_asn = alias_asn;
            server.aliases[i].alias_region = alias_region;
            return 0;
        }
    }

    return -1;
}

/*
* http api for lpl_sche, return proper forward-fp.
*/
server_info
ForwardServerManager::fm_get_proper_forward_fp_http_api(string& streamid, uint16_t asn)
{
    INF("fm_get_proper_forward_fp_http_api: stream_id = %s  asn = %d", streamid.c_str(), asn);

    /*consistent hash select a forward-fp server(whose level is 1) for those request that level >= 2 */
    server_info  result;

    if (asn == ASN_TEL)
    {
        result = _tel_shards->get_server_info(streamid);
    }
    else if (asn == ASN_CNC)
    {
        result = _cnc_shards->get_server_info(streamid);
    }
    else if (asn == ASN_TEL_RTP)
    {
        result = _tel_rtp_shards->get_server_info(streamid);
    }
    else if (asn == ASN_CNC_RTP)
    {
        result = _cnc_rtp_shards->get_server_info(streamid);
    }
    else if (asn == ASN_PRIVATE)
    {
        result = _private_shards->get_server_info(streamid);
    }
    else
    {
        result.ip = 0;
        WRN("fm_get_proper_forward_fp_http_api : not supported asn!\n");
    }

    INF("fm_get_proper_forward_fp_http_api : the response forward fp ip:%s, port:%u, level: %u",
        util_ip2str(result.ip), result.port, result.level);

    return result;
}


/*
 * get proper forward nfp, for lps
 */
server_info ForwardServerManager::fm_get_proper_forward_nfp_http_api(std::string &streamid, uint32_t group_id)
{
    INF("fm_get_proper_forward_nfp_http_api:stream_id = %s group_id= %u", streamid.c_str(), group_id);

    server_info result;
    int found = 0;
    int last_load_score = LOAD_SCORE_UPPER_BOUND;


    TRC("stream_dispatch: streamid: %s", streamid.c_str());

    /* 1, look up stream forward list preferred */
    for (StreamNodeMapType::iterator si = _stream_map.begin();
            si != _stream_map.end();)
    {
        StreamNodeMapType::iterator tsi = si++;
        StreamMapNode &node = tsi->second;

        /* delete the stream entry if there's no active forward server
         * (or receiver) exist
         */
        if (node.forward_list.empty()
                && (time(0) - node.last_active_time) > 60)
        {
            INF("delete an unactive stream, stream_id: %s.", node.stream_id.c_str());
            _stream_map.erase(tsi);
            continue;
        }


        if(node.stream_id == streamid)
        {
            TRC("stream_dispatch: streamid: %s, node found", streamid.c_str());
            node.last_active_time = time(0);

            /* 2, look up for proper forward server */
            uint16_t last_level = LEVEL0;
            int32_t node_forward_list_size = node.forward_list.size();

            while ((--node_forward_list_size >= 0) && (found == 0))
            {
                list<StreamForwardListNode>::iterator tli = node.forward_list.begin();

                if (tli == node.forward_list.end())
                {
                    TRC("stream_dispatch: streamid: %s, tli invalid, break", streamid.c_str());
                    break;
                }

                ForwardServerMapType::iterator fi = _forward_map.find(tli->fid);
                if (fi == _forward_map.end())
                {
                    TRC("stream_dispatch: streamid: %s, forwardserver not found, break", streamid.c_str());
                    node.forward_list.erase(tli);
                    break;
                }

                StreamForwardListNode stream_forward_list_node = *tli;
                if (tli->stream_cnt > MAX_SAME_STREAM_CNT)
                {
                    TRC("stream_dispatch: streamid: %s, stream_cnt too big, continue", streamid.c_str());
                    node.forward_list.erase(tli);
                    node.forward_list.push_back(stream_forward_list_node);
                    continue;
                }

                ForwardServerMapNode &server_node = fi->second;

                if (!fm_is_active(server_node.forward_server)
                        || server_node.ref_cnt <= 0)
                {
                    /* do not return forward server that's unactive */
                    TRC("stream_dispatch: streamid: %s, fm isnot active, continue", streamid.c_str());

                    fm_unreference_forward(tli->fid);
                    if(node.has_level_0 == true)
                    {
                        node.has_level_0 = false;
                    }
                    node.forward_list.erase(tli);
                    continue;
                }

                if (group_id == server_node.forward_server.group_id)
                {
                    uint32_t fs_load_score = server_node.forward_server.load_score;
                    if ((fs_load_score < last_load_score) && (fs_load_score >= 0))
                    {
                        result.name   = "";
                        result.weight = 100; 
                        result.ip     = server_node.forward_server.ip;
                        result.port   = server_node.forward_server.player_port;
                        result.level  = tli->level;
                        result.forwardid = tli->fid;

                        last_load_score = fs_load_score;

                        tli->stream_cnt++;

                        TRC("stream_dispatch: streamid: %s, forwardserver found, ip:%s, port:%u",\
                                streamid.c_str(), util_ip2str(result.ip), result.port);

                        found = 1;
                        break;
                    }
                }

                TRC("stream_dispatch: streamid: %s, continue", streamid.c_str());

                node.forward_list.erase(tli);
                node.forward_list.push_back(stream_forward_list_node);
            }
            break;
        }
        /*else
        {
            DBG("stream_dispatch: streamid: %s, zombie node", streamid.c_str());
            fm_del_zombie(node);
        }
	*/
    }

    if (found == 0)
    {
        int32_t forward_list_size = _forward_list.size();

        TRC("stream_dispatch: streamid: %s, not found from streamlist, %d", streamid.c_str(), forward_list_size);
        while ((--forward_list_size >= 0) && (found == 0))
        {
	    TRC("stream_dispatch: streamid: %s, forward_list_size: %d", streamid.c_str(), forward_list_size);
            list<ForwardID>::iterator fid_iter = _forward_list.begin();
            if (fid_iter == _forward_list.end())
            {
                TRC("stream_dispatch: streamid: %s, _forward_list invalid", streamid.c_str());
                break;
            }

            ForwardID fid = *fid_iter;
            _forward_list.pop_front();
            _forward_list.push_back(fid);

            ForwardServerMapType::iterator fi = _forward_map.find(fid);
            if (fi == _forward_map.end())
            {
                TRC("stream_dispatch: streamid: %s, not found from _forward_map", streamid.c_str());
                break;
            }

            ForwardServerMapNode &server_node = fi->second;
            if (!fm_is_active(server_node.forward_server))
            {
                TRC("stream_dispatch: streamid: %s, not active,continue ", streamid.c_str());
                continue;
            }

	    TRC("stream_dispatch: streamid: %s, %d, %d", streamid.c_str(), server_node.forward_server.group_id, group_id);
            if (server_node.forward_server.group_id == group_id)
            {
                uint32_t fs_load_score = server_node.forward_server.load_score;

		TRC("stream_dispatch: streamid: %s, %u, %u", streamid.c_str(), fs_load_score, last_load_score);
                if ((fs_load_score < last_load_score) && (fs_load_score >= 0))
                {
                    result.name   = "";
                    result.weight = 100; 
                    result.ip     = server_node.forward_server.ip;
                    result.port   = server_node.forward_server.player_port;
                    result.level  = LEVEL3;
                    result.forwardid =fi->first;

                    last_load_score = fs_load_score;

                    found = 1;

                    TRC("stream_dispatch: streamid: %s, found, ip:%s,port:%u ",\
                           streamid.c_str(), util_ip2str(result.ip), result.port);
                    break;
                }
            }

        }
    }

    return result;
}

int
ForwardServerManager::fm_get_interlive_server_stat_http_api(string& rsp_str)
{
    TRC("fm_get_interlive_server_stat_http_api");

    rsp_str = "forward total number:";

    stringstream gss;
    string gs;
    gss << fm_cur_forward_num();
    gss >> gs;
    rsp_str += gs;
    rsp_str += "\r\n";

    for (ForwardServerMapType::iterator i = _forward_map.begin();
            i != _forward_map.end(); i++)
    {
        if (fm_is_active(i->second.forward_server))
        {
            rsp_str += "ip:";
            rsp_str += util_ip2str(i->second.forward_server.ip);
            rsp_str += " ";

            ForwardServerStat stat_temp;
            stat_temp = i->second.forward_server.fstat;

            stringstream ss;
            string s;

            rsp_str += "sys_uptime:";
            ss << stat_temp.sys_uptime;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "sys_cpu_cores:";
            ss << stat_temp.sys_cpu_cores;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "sys_cpu_idle:";
            ss << stat_temp.sys_cpu_idle;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "sys_mem_total:";
            ss << stat_temp.sys_mem_total;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "sys_mem_total_free:";
            ss << stat_temp.sys_mem_total_free;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "sys_net_rx:";
            ss << stat_temp.sys_net_rx;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "sys_net_tx:";
            ss << stat_temp.sys_net_rx;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "proc_cpu_use:";
            ss << stat_temp.proc_cpu_use;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "proc_sleepavg:";
            ss << stat_temp.proc_sleepavg;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "proc_vmpeak:";
            ss << stat_temp.proc_vmpeak;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "proc_vmsize:";
            ss << stat_temp.proc_vmsize;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "proc_vmrss:";
            ss << stat_temp.proc_vmrss;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "proc_total_uptime:";
            ss << stat_temp.proc_total_uptime;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "proc_process_name:";
            rsp_str += stat_temp.proc_process_name;
            rsp_str += " ";

            rsp_str += "buss_fsv2_stream_count:";
            ss << stat_temp.buss_fsv2_stream_count;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "buss_fsv2_total_session:";
            ss << stat_temp.buss_fsv2_total_session;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "buss_fsv2_active_session:";
            ss << stat_temp.buss_fsv2_active_session;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "buss_fsv2_connection_count:";
            ss << stat_temp.buss_fsv2_connection_count;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "buss_fcv2_stream_count:";
            ss << stat_temp.buss_fcv2_stream_count;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "buss_uploader_connection_count:";
            ss << stat_temp.buss_uploader_connection_count;
            ss >> s;
            rsp_str += s;
            rsp_str += " ";
            ss.clear();

            rsp_str += "buss_player_connection_count:";
            ss << stat_temp.buss_player_connection_count;
            ss >> s;
            rsp_str += s;
            rsp_str += "     \r\n";
            ss.clear();
            
            rsp_str += "buss_player_online_cnt:";
            ss << stat_temp.buss_player_online_cnt;
            ss >> s;
            rsp_str += s;
            rsp_str += "     \r\n";
            ss.clear();

        }
    }

    return 0;
}

int
ForwardServerManager::fm_get_interlive_server_stat_http_api(json_object* rsp_json)
{
    INF("fm_get_interlive_server_stat_http_api JSON");

    json_object* forward_map_json = json_object_new_array();

    for (ForwardServerMapType::iterator i = _forward_map.begin();
            i != _forward_map.end(); i++)
    {
        json_object* json_msg = json_object_new_object();

        if (fm_is_active(i->second.forward_server))
        {
            json_object* j_ip =json_object_new_string(util_ip2str(i->second.forward_server.ip));
            json_object_object_add(json_msg, "ip", j_ip);

            ForwardServerStat stat_temp;
            stat_temp = i->second.forward_server.fstat;

            json_object* j_sys_uptime =json_object_new_int64(stat_temp.sys_uptime);
            json_object* j_sys_cpu_cores =json_object_new_int(stat_temp.sys_cpu_cores);
            json_object* j_sys_cpu_idle =json_object_new_int(stat_temp.sys_cpu_idle);
            json_object* j_sys_mem_total =json_object_new_int(stat_temp.sys_mem_total);
            json_object* j_sys_mem_total_free =json_object_new_int(stat_temp.sys_mem_total_free);

            json_object* j_sys_net_rx =json_object_new_int64(stat_temp.sys_net_rx);
            json_object* j_sys_net_tx =json_object_new_int64(stat_temp.sys_net_tx);
            json_object* j_proc_cpu_use =json_object_new_int(stat_temp.proc_cpu_use);
            json_object* j_proc_sleepavg =json_object_new_int(stat_temp.proc_sleepavg);
            json_object* j_proc_vmpeak =json_object_new_int(stat_temp.proc_vmpeak);

            json_object* j_proc_vmsize =json_object_new_int(stat_temp.proc_vmsize);
            json_object* j_proc_vmrss =json_object_new_int(stat_temp.proc_vmrss);
            json_object* j_proc_total_uptime =json_object_new_int64(stat_temp.proc_total_uptime);
            json_object* j_proc_process_name =json_object_new_string(stat_temp.proc_process_name.c_str());
            json_object* j_buss_fsv2_stream_count =json_object_new_int(stat_temp.buss_fsv2_stream_count);
            json_object* j_buss_fsv2_total_session =json_object_new_int(stat_temp.buss_fsv2_total_session);

            json_object* j_buss_fsv2_active_session =json_object_new_int(stat_temp.buss_fsv2_active_session);
            json_object* j_buss_fsv2_connection_count =json_object_new_int(stat_temp.buss_fsv2_connection_count);
            json_object* j_buss_fcv2_stream_count =json_object_new_int(stat_temp.buss_fcv2_stream_count);
            json_object* j_buss_uploader_connection_count =json_object_new_int(stat_temp.buss_uploader_connection_count);
            json_object* j_buss_player_connection_count =json_object_new_int(stat_temp.buss_player_connection_count);
            json_object* j_buss_player_online_cnt =json_object_new_int(stat_temp.buss_player_online_cnt);

            json_object_object_add(json_msg, "sys_uptime", j_sys_uptime);
            json_object_object_add(json_msg, "sys_cpu_cores", j_sys_cpu_cores);
            json_object_object_add(json_msg, "sys_cpu_idle", j_sys_cpu_idle);
            json_object_object_add(json_msg, "sys_mem_total", j_sys_mem_total);
            json_object_object_add(json_msg, "sys_mem_total_free", j_sys_mem_total_free);

            json_object_object_add(json_msg, "sys_net_rx", j_sys_net_rx);
            json_object_object_add(json_msg, "sys_net_tx", j_sys_net_tx);
            json_object_object_add(json_msg, "proc_cpu_use", j_proc_cpu_use);
            json_object_object_add(json_msg, "proc_sleepavg", j_proc_sleepavg);
            json_object_object_add(json_msg, "proc_vmpeak", j_proc_vmpeak);

            json_object_object_add(json_msg, "proc_vmsize", j_proc_vmsize);
            json_object_object_add(json_msg, "proc_vmrss", j_proc_vmrss);
            json_object_object_add(json_msg, "proc_total_uptime", j_proc_total_uptime);
            json_object_object_add(json_msg, "proc_process_name", j_proc_process_name);
            json_object_object_add(json_msg, "buss_fsv2_stream_count", j_buss_fsv2_stream_count);
            json_object_object_add(json_msg, "buss_fsv2_total_session", j_buss_fsv2_total_session);

            json_object_object_add(json_msg, "buss_fsv2_active_session", j_buss_fsv2_active_session);
            json_object_object_add(json_msg, "buss_fsv2_connection_count", j_buss_fsv2_connection_count);
            json_object_object_add(json_msg, "buss_fcv2_stream_count", j_buss_fcv2_stream_count);
            json_object_object_add(json_msg, "buss_uploader_connection_count", j_buss_uploader_connection_count);
            json_object_object_add(json_msg, "buss_player_connection_count", j_buss_player_connection_count);
            json_object_object_add(json_msg, "buss_player_online_cnt", j_buss_player_online_cnt);

        }

        json_object_array_add(forward_map_json, json_msg);
    }

    json_object_object_add(rsp_json, "forward_map", forward_map_json);

    return 0;
}



void ForwardServerManager::fm_clear_stream_cnt_per_sec()    
{
    StreamNodeMapType::iterator iter = _stream_map.begin();
    for(;iter != _stream_map.end();iter++)
    {
        list<StreamForwardListNode>::iterator li = iter->second.forward_list.begin();
        for (; li != iter->second.forward_list.end(); li++)
        {
            // DBG("ForwardServerManager::fm_clear_stream_cnt_per_sec:stream:%s, forwardid:%lu, streamcnt:%d", \
                    iter->first.c_str(), li->fid, li->stream_cnt);
            li->stream_cnt = 0;
        }
    }

}



