/*
 * author: duanbingnan@youku.com
 * create: 2015.05.05
 *
 */

#include "forward_manager_v3.h"

#include <arpa/inet.h>
#include <assert.h>
#include <cstring>
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

#undef DEBUG
#define DEBUG

ForwardServerManager::ForwardServerManager()
{
}

ForwardServerManager::~ForwardServerManager()
{
    /* clean stream_list_node */
    for (StreamNodeMapType::iterator i = _stream_map_flv.begin();
            i != _stream_map_flv.end();
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
    _stream_map_flv.clear();

    for (StreamNodeMapType::iterator i = _stream_map_rtp.begin();
            i != _stream_map_rtp.end();
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
    _stream_map_rtp.clear();

    /* clean forward_server_list */
    _forward_map.clear();
}

int
ForwardServerManager::init()
{
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

/// TODO: merge tcp and rtp streams here!!!
int
ForwardServerManager::fm_cur_stream_num()
{
    std::set<std::string> stream_ids;
    StreamNodeMapType::iterator stream_map_tcp_iter = _stream_map_flv.begin();
    while (stream_map_tcp_iter != _stream_map_flv.end())
    {
        std::string stream_id = stream_map_tcp_iter->first;
        stream_ids.insert(stream_id);
        ++stream_map_tcp_iter;
    }

    StreamNodeMapType::iterator stream_map_rtp_iter = _stream_map_rtp.begin();
    while (stream_map_rtp_iter != _stream_map_rtp.end())
    {
        std::string stream_id = stream_map_rtp_iter->first;
        stream_ids.insert(stream_id);
        ++stream_map_rtp_iter;
    }

    return stream_ids.size();
}

void
ForwardServerManager::fm_keep_update_forward_fp_infos()
{
    _fpinfos.Clear();
    for (ForwardServerMapType::iterator i = _forward_map.begin();
        i != _forward_map.end(); i++)
    {
        ForwardServerMapNode &node = i->second;
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

// SubTracker: Update FP info
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
        // Here need some fix, since we have more things to pass, e.g. player_port
        fs->stop_ip = fpinfo.port();
        fs->asn = fpinfo.asn();
        fs->topo_level = 1;
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

    _fm_fp_set_gen(forward_fp_set, ASN_PRIVATE, root);
    if (NULL != _private_shards)
    {
        delete _private_shards;
        _private_shards = NULL;
    }
    _private_shards = new ketama_hash(forward_fp_set);
    _private_shards->init(forward_fp_set);
    forward_fp_set.clear();

    _fm_fp_set_gen(forward_fp_set, ASN_CMCC, root);
    if (NULL != _cmcc_shards)
    {
        delete _cmcc_shards;
        _cmcc_shards = NULL;
    }
    _cmcc_shards = new ketama_hash(forward_fp_set);
    _cmcc_shards->init(forward_fp_set);
    forward_fp_set.clear();


    return 0;
}

void ForwardServerManager::clone_stream_map_flv(std::map<std::string, StreamMapNode>& target)
{
    target = _stream_map_flv;
}

void ForwardServerManager::clone_stream_map_rtp(std::map<std::string, StreamMapNode>& target)
{
    target = _stream_map_rtp;
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
ForwardServerManager::fm_compute_id(uint32_t ip, uint16_t port, LCdnStreamTransportType transport)
{
    // Bits Allocation:
    // 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
    // TTTTRRRR RRRRRRRR PPPPPPPP PPPPPPPP IIIIIIII IIIIIIII IIIIIIII IIIIIIII
    // T -- Transport
    // R -- Random
    // P -- Port
    // I -- IP
    return (((uint64_t)transport & 0xF) << 60) + ((time(0) & 0xFFF) << 48) + (((uint64_t)port & 0xFFFF) << 32) + ip;
}

LCdnStreamTransportType fid_transport_type(const ForwardID fid)
{
    return (LCdnStreamTransportType)(fid >> 60);
}

const std::string& transport_type_string(LCdnStreamTransportType type)
{
    static const std::string FLV = "flv";
    static const std::string RTP = "rtp";
    static const std::string UNKNOWN = "unknown";
    switch (type)
    {
        case LCdnStreamTransportFlv:
            return FLV;
        case LCdnStreamTransportRtp:
            return RTP;
    }
    return UNKNOWN;
}
/*
 * generate the forward-fp servers set
 */
int
ForwardServerManager::fm_generate_forward_fp_set(const LCdnStreamTransportType transport_type,
        vector<server_info>& forward_fp_set,
        uint16_t asn)
{
    for (ForwardServerMapType::iterator i = _forward_map.begin();
            i != _forward_map.end(); i++)
    {
        ForwardServerMapNode &node = i->second;
        ForwardID fid = i->first;
        if (fid_transport_type(fid) != transport_type)
        {
            continue;
        }

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
                item.player_port = node.forward_server.player_port;
                item.uploader_port = node.forward_server.uploader_port;
                item.level  = node.forward_server.topo_level;
                item.asn = node.forward_server.asn;
                item.region = node.forward_server.region;
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
                            item.player_port = node.forward_server.player_port;
                            item.uploader_port = node.forward_server.uploader_port;
                            item.level  = node.forward_server.topo_level;
                            item.asn = node.forward_server.asn;
                            item.region = node.forward_server.region;
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
        DBG("forward_fp_set item, ip: %s, backend_port: %u, player_port: %u, uploader_port: %u, level: %u, name: %s, weight: %d, forwardid: %lu",
            util_ip2str((*iter).ip),
            (*iter).port,
            (*iter).player_port,
            (*iter).uploader_port,
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
        uint16_t topo_level)
{
    /* TODO
    if (topo_level > g_config->child_topo_level)
        return -1;
    */

    ForwardID fid = fm_compute_id(req->port, req->ip, LCdnStreamTransportFlv);

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
            INF("remove a forward, fid: %lu, ip: %s, port: %u, asn: %u, region: %u, level: %u",
                tfmi->first, // forward id
                util_ip2str(tfmi->second.forward_server.ip),
                tfmi->second.forward_server.backend_port,
                tfmi->second.forward_server.asn,
                tfmi->second.forward_server.region,
                tfmi->second.forward_server.topo_level);
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

    for (size_t i = 0; i < sizeof(new_forward.forward_server.aliases)/sizeof(alias_t); i++)
    {
        new_forward.forward_server.aliases[i].alias_ip = 0;
        new_forward.forward_server.aliases[i].alias_asn = 0;
        new_forward.forward_server.aliases[i].alias_region = 0;
    }

    new_forward.ref_cnt = 0;
    INF("add a forward. fid: %lu, ip: %s, port: %u, asn: %u, region: %u, level:%u",
            new_forward.forward_server.fid,
            util_ip2str(req->ip),
            req->port,
            req->asn,
            req->region,
            topo_level);

    _forward_map.insert(std::make_pair<ForwardID, ForwardServerMapNode>(fid, new_forward));

#ifdef DEBUG
    ForwardServerMapType::iterator iter = _forward_map.begin();
    for(;iter != _forward_map.end();iter++)
    {
        DBG("_forward_map item, ip:%s, port:%u,fid:%lu, asn:%d, region:%d, topo_level:%d, shutdown:%d",
            util_ip2str(iter->second.forward_server.ip),
            iter->second.forward_server.backend_port,
            iter->second.forward_server.fid,
            iter->second.forward_server.asn,
            iter->second.forward_server.region,
            iter->second.forward_server.topo_level,
            iter->second.forward_server.shutdown);
    }
#endif

    return fid;
}


ForwardID
ForwardServerManager::fm_add_a_forward_v4(const f2t_register_req_v4 *req,
        uint16_t topo_level)
{
    DBG("fm_add_a_forward_v4");
    /* TODO
    if (topo_level > g_config->child_topo_level)
        return -1;
    */
    uint32_t ip = 0;
    uint16_t backend_port = 0;
    uint16_t player_port = 0;
    uint16_t asn = 0;
    uint16_t region = 0;
    uint16_t uploader_port = 0;

    if (req->has_ip())
    {
        ip = req->ip();
    }

    if (req->has_backend_port())
    {
        backend_port = req->backend_port();
    }

    if (req->has_player_port())
    {
        player_port = req->player_port();
    }

    if (req->has_asn())
    {
        asn = req->asn();
    }

    if (req->has_region())
    {
        region = req->region();
    }

    if (req->has_uploader_port())
    {
        uploader_port = req->uploader_port();
    }

    LCdnStreamTransportType transport_type = LCdnStreamTransportFlv;
    if (req->has_transport_type())
    {
        transport_type = req->transport_type();
    }

    ForwardID fid = fm_compute_id(backend_port, ip, transport_type);

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
        if (tfmi->second.forward_server.ip == ip
            && tfmi->second.forward_server.backend_port == backend_port
            && tfmi->second.forward_server.player_port == player_port)
        {
            tfmi->second.forward_server.shutdown = 1;   // shutdown the server
        }

        if (!fm_is_active(tfmi->second.forward_server)
            && tfmi->second.ref_cnt == 0)
        {
            INF("remove a forward, fid: %lu, ip: %s, port: %u, asn: %u, region: %u, level: %u",
                tfmi->first, // forward id
                util_ip2str(tfmi->second.forward_server.ip),
                tfmi->second.forward_server.backend_port,
                tfmi->second.forward_server.asn,
                tfmi->second.forward_server.region,
                tfmi->second.forward_server.topo_level);
            _forward_map.erase(tfmi);
        }
    }

    ForwardServerMapNode new_forward;

    new_forward.forward_server.ip = ip;
    new_forward.forward_server.backend_port = backend_port;
    new_forward.forward_server.player_port  = player_port;
    new_forward.forward_server.fid          = fid;
    new_forward.forward_server.asn          = asn;
    new_forward.forward_server.region       = region;
    new_forward.forward_server.uploader_port = uploader_port;

    new_forward.forward_server.topo_level   = topo_level;
    new_forward.forward_server.shutdown     = 0;
    new_forward.forward_server.last_keep    = time(0);

    for (size_t i = 0; i < sizeof(new_forward.forward_server.aliases)/sizeof(alias_t); i++)
    {
        new_forward.forward_server.aliases[i].alias_ip = 0;
        new_forward.forward_server.aliases[i].alias_asn = 0;
        new_forward.forward_server.aliases[i].alias_region = 0;
    }

    new_forward.ref_cnt = 0;
    INF("add a forward. fid: %lu, ip: %s, backend_port: %u, player_port: %u, asn: %u, region: %u, level:%u",
            new_forward.forward_server.fid,
            util_ip2str(ip),
            backend_port,
            player_port,
            asn,
            region,
            topo_level);

    _forward_map.insert(std::make_pair<ForwardID, ForwardServerMapNode>(fid, new_forward));

#ifdef DEBUG
    ForwardServerMapType::iterator iter = _forward_map.begin();
    for(;iter != _forward_map.end();iter++)
    {
        DBG("_forward_map item, ip:%s, port:%u,fid:%lu, asn:%d, region:%d, topo_level:%d, shutdown:%d",
            util_ip2str(iter->second.forward_server.ip),
            iter->second.forward_server.backend_port,
            iter->second.forward_server.fid,
            iter->second.forward_server.asn,
            iter->second.forward_server.region,
            iter->second.forward_server.topo_level,
            iter->second.forward_server.shutdown);
    }
#endif

    return fid;
}

std::map<std::string, StreamMapNode>& ForwardServerManager::stream_map_by_fid(const ForwardID fid)
{
    LCdnStreamTransportType transport_type = fid_transport_type(fid);
    return transport_type == LCdnStreamTransportFlv ? _stream_map_flv : _stream_map_rtp;
}

std::map<std::string, StreamMapNode>& ForwardServerManager::stream_map_by_transport_type(const LCdnStreamTransportType transport_type)
{
    return transport_type == LCdnStreamTransportFlv ? _stream_map_flv : _stream_map_rtp;
}
/*
 * on receiver, forward-fp, forward-nfp have new streams, report to tracker and update _stream_map
 */
int
ForwardServerManager::fm_add_to_stream_v3(ForwardID fs_id,
        f2t_update_stream_req_v3 *req, uint16_t region)
{
    std::map<std::string, StreamMapNode>& stream_map = stream_map_by_fid(fs_id);

    if (req->level > g_config->child_topo_level)
    {
        WRN("fm_add_to_stream: invalid level: %d", req->level);
        return -1;
    }

    string streamid;
    convert_streamid_array_to_string(req->streamid, streamid);

    DBG("fm_add_to_stream_v3: streamid=%s", streamid.c_str());

    /*find stream in _stream_map*/
    std::pair<StreamNodeMapType::iterator, bool> inserted = stream_map.insert(std::make_pair(streamid, StreamMapNode(streamid, time(0))));
    StreamNodeMapType::iterator mi = inserted.first;

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

    INF("add forward server(ip: %s, port: %u, asn: %u, region: %u, level: %u, transport: %s) to stream list(stream_id: %s)",
            util_ip2str(forward.ip),
            forward.backend_port,
            forward.asn,
            forward.region,
            req->level,
            fid_transport_type(fs_id) == LCdnStreamTransportFlv ? "flv" : "rtp",
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
        mi->second.forward_list.push_back(StreamForwardListNode(fs_id, req->level, region));
        fm_reference_forward(fs_id);
    }

    if (req->level == LEVEL0)
    {
        mi->second.has_level_0 = true;
    }

#ifdef DEBUG
    DBG(">>>>>>>> _stream_map dump begin <<<<<<<<");
    StreamNodeMapType::iterator iter = _stream_map_flv.begin();
    for(;iter != _stream_map_flv.end();iter++)
    {
        DBG("tcp stream:%s, last_active_time:%lu, has_level_0:%d",
            iter->second.stream_id.c_str(),
            iter->second.last_active_time,
            iter->second.has_level_0);

        list<StreamForwardListNode>::iterator li = iter->second.forward_list.begin();
        for (; li != iter->second.forward_list.end(); li++)
        {

            ForwardServerMapType::iterator mi = _forward_map.find(li->fid);

            ForwardServer &forward1 = mi->second.forward_server;

            DBG("forward_list item, fid:%lu, ip:%s, level:%d (runtime level), region:%d",
                (*li).fid, util_ip2str(forward1.ip), (*li).level, (*li).region);
        }
    }

    iter = _stream_map_rtp.begin();
    for(;iter != _stream_map_rtp.end();iter++)
    {
        DBG("rtp: stream:%s, last_active_time:%lu, has_level_0:%d",
            iter->second.stream_id.c_str(),
            iter->second.last_active_time,
            iter->second.has_level_0);

        list<StreamForwardListNode>::iterator li = iter->second.forward_list.begin();
        for (; li != iter->second.forward_list.end(); li++)
        {

            ForwardServerMapType::iterator mi = _forward_map.find(li->fid);

            ForwardServer &forward1 = mi->second.forward_server;

            DBG("forward_list item, fid:%lu, ip:%s, level:%d (runtime level), region:%d",
                (*li).fid, util_ip2str(forward1.ip), (*li).level, (*li).region);
        }
    }

    DBG(">>>>>>>> _stream_map dump end <<<<<<<<");
#endif

    return 0;
}

/*
* on receiver, forward-fp, forward-nfp, streams not exist anymore, report to tracker and update _stream_map
*/
int
ForwardServerManager::fm_del_from_stream_v3(ForwardID fs_id,
        const f2t_update_stream_req_v3 *req)
{
    std::map<std::string, StreamMapNode>& stream_map = stream_map_by_fid(fs_id);

    string streamid;
    convert_streamid_array_to_string(req->streamid, streamid);

    DBG("fm_del_from_stream_v3: streamid=%s", streamid.c_str());

    StreamNodeMapType::iterator i = stream_map.find(streamid);
    if (i != stream_map.end())
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

int
ForwardServerManager::fm_get_proper_forward_v3(ForwardServer& upstream_server,
                                               const ForwardID& fid,
                                               const uint8_t* stream_id_ptr,
                                               const uint32_t ip,
                                               const uint16_t port,
                                               const uint16_t asn,
                                               const uint16_t region,
                                               const uint16_t level)
{
    string stream_id;
    convert_streamid_array_to_string(stream_id_ptr, stream_id);
    return fm_get_proper_forward_v3(upstream_server,
                                    fid_transport_type(fid),
                                    stream_id,
                                    ip,
                                    port,
                                    asn,
                                    region,
                                    level);
}

int
ForwardServerManager::fm_get_proper_forward_v3(ForwardServer& upstream_server,
                                               const LCdnStreamTransportType transport_type,
                                               const std::string& stream_id,
                                               const uint32_t ip,
                                               const uint16_t port,
                                               const uint16_t asn,
                                               const uint16_t region,
                                               const uint16_t level)
{
    /* TODO
     * 1, think about each forward server's load
     */
    INF("looking up forward parent of stream_id:%s, for ip:%s, port:%u, asn: %u, region: %u, level: %u",
            stream_id.c_str(),
            util_ip2str(ip),
            port,
            asn,
            region,
            level);

    bool found = false;
    if (level >= LEVEL2)
    {
        found = fm_get_proper_forward_nfp_v3(upstream_server, transport_type, stream_id, ip, port, asn, region, level);
        if (found)
        {
            INF("found in fm_get_proper_forward_nfp_v3");
        }
        else
        {
            found = fm_get_proper_forward_fp_v3(upstream_server, transport_type, stream_id, ip, port, asn, region, level);

            if (found)
            {
                INF("found in fm_get_proper_forward_fp_v3");
            }
        }
    }
    else
    {
        found = fm_get_proper_receiver_v3(upstream_server, transport_type, stream_id, ip, port, asn, region, level);
    }
    return found ? 0 : 1;
}

/*
 * get proper forward from g_stream_map, loopup forward-nfp for level 3 forward-nfp
 */
bool
ForwardServerManager::fm_get_proper_forward_nfp_v3(
        ForwardServer& upstream_server,
        const LCdnStreamTransportType transport_type,
        const std::string& stream_id,
        const uint32_t ip,
        const uint16_t port,
        const uint16_t asn,
        const uint16_t region,
        const uint16_t level)
{
    std::map<std::string, StreamMapNode>& stream_map = stream_map_by_transport_type(transport_type);
    INF("looking up forward nfp of stream_id:%s, for ip:%s, port:%u, asn: %u, region: %u, level: %u",
            stream_id.c_str(),
            util_ip2str(ip),
            port,
            asn,
            region,
            level);

    bool found = false;
    /* 1, look up stream forward list preferred */
    for (StreamNodeMapType::iterator si = stream_map.begin();
            si != stream_map.end();)
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
            stream_map.erase(tsi);
            continue;
        }

        if(node.stream_id == stream_id)
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

                    if (tli->level == 0 && node.has_level_0 == true)
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
                if ((asn == server_node.forward_server.asn)
                        && (region == server_node.forward_server.region))
                {
                    // skip the requester
                    if (server_node.forward_server.ip == ip && server_node.forward_server.backend_port == port) {
                        continue;
                    }
                    // try to find a record with larger level value
                    if (tli->level > last_level && tli->level < level)
                    {
                        upstream_server = server_node.forward_server;
                        upstream_server.topo_level = tli->level;
                        last_level = tli->level;
                        found = true;
                        INF("found up stream nfp stream: %s, ip: %s, backend_port: %u, player_port: %u, uploader_port: %u, uplevel: %u",
                            stream_id.c_str(),
                            util_ip2str(upstream_server.ip),
                            upstream_server.backend_port,
                            upstream_server.player_port,
                            upstream_server.uploader_port,
                            upstream_server.topo_level);
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
bool
ForwardServerManager::fm_get_proper_forward_fp_v3(ForwardServer& upstream_server,
                                                  const LCdnStreamTransportType transport_type,
                                                  const std::string& stream_id,
                                                  const uint32_t ip,
                                                  const uint16_t port,
                                                  const uint16_t asn,
                                                  const uint16_t region,
                                                  const uint16_t level)
{
    INF("looking up forward fp of stream_id:%s, for ip:%s, port:%u, asn: %u, region: %u, level: %u, transport: %s",
            stream_id.c_str(),
            util_ip2str(ip),
            port,
            asn,
            region,
            level,
            transport_type_string(transport_type).c_str());

    /*consistent hash select a forward-fp server(whose level is 1) for those request that level >= 2 */
    bool found = false;
    server_info  result;
    vector<server_info> forward_fp_set;

    if ((asn == ASN_CNC) ||
        (asn == ASN_TEL) ||
        (asn == ASN_PRIVATE) ||
        (asn == ASN_CMCC) )
    {
        if (0 == fm_generate_forward_fp_set(transport_type, forward_fp_set, asn))
        {
            ketama_hash *shards = new ketama_hash(forward_fp_set);

            if(0 == shards->init(forward_fp_set))
            {
                result = shards->get_server_info(stream_id);

                upstream_server.ip = result.ip;
                upstream_server.backend_port = result.port;
                upstream_server.player_port = result.player_port;
                upstream_server.uploader_port = result.uploader_port;
                upstream_server.asn = result.asn;
                upstream_server.region = result.asn;
                upstream_server.topo_level = result.level;

                INF("found up stream fp stream: %s, ip: %s, backend_port: %u, player_port: %u, uploader_port: %u, level: %u",
                        stream_id.c_str(), util_ip2str(result.ip), result.port, result.player_port, result.uploader_port, result.level);

                found = true;
            }

            if (NULL != shards)
            {
                delete shards;
                shards = NULL;
            }
        }
        else
        {
            WRN("not generate forward fp set correctly!\n");
        }
    }
    else
    {
        WRN("fm_get_proper_forward_fp : not supported asn!\n");
    }

    return found;
}

/* return 0 if not found
 * otherwise if found
 * loopup receiver for forward-fp
 */
bool
ForwardServerManager::fm_get_proper_receiver_v3(ForwardServer& upstream_server,
                                                const LCdnStreamTransportType transport_type,
                                                const std::string& stream_id,
                                                const uint32_t ip,
                                                const uint16_t port,
                                                const uint16_t asn,
                                                const uint16_t region,
                                                const uint16_t level)
{
    std::map<std::string, StreamMapNode>& stream_map = stream_map_by_transport_type(transport_type);
    INF("looking up receiver of stream_id:%s, ip:%s, port:%u, asn: %u, region: %u, level: %u, transport: %s",
            stream_id.c_str(),
            util_ip2str(ip),
            port,
            asn,
            region,
            level,
            transport_type_string(transport_type).c_str()
            );

    bool found = false;

    /* 1, look up stream forward list preferred */
    for (StreamNodeMapType::iterator si = stream_map.begin();
            si != stream_map.end();)
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
            stream_map.erase(tsi);
            continue;
        }

        if(node.stream_id == stream_id)
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
                    if(tli->level == 0 && node.has_level_0 == true)
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
                    if (ip != server_node.forward_server.ip
                            || port != server_node.forward_server.backend_port)
                    {
                        upstream_server = server_node.forward_server;
                        found = true;
                        INF("found up stream receiver stream: %s, ip: %s, backend_port: %u, player_port: %u, uploader_port: %u, level: %u",
                                stream_id.c_str(),
                                util_ip2str(upstream_server.ip),
                                upstream_server.backend_port,
                                upstream_server.player_port,
                                upstream_server.uploader_port,
                                upstream_server.topo_level);

                        // revise upstream_server ip with aliases by matching asn
                        if (asn != server_node.forward_server.asn)
                        {
                            size_t i = 0;
                            for (;i < sizeof(server_node.forward_server.aliases)/sizeof(alias_t); i++)
                            {
                                // This is last alias
                                if (server_node.forward_server.aliases[i].alias_ip == 0)
                                    break;
                                if (asn == server_node.forward_server.aliases[i].alias_asn)
                                {
                                    upstream_server.ip = server_node.forward_server.aliases[i].alias_ip;
                                    assert(tli->level == server_node.forward_server.topo_level);
                                    INF("revised up stream receiver stream: %s, ip: %s, backend_port: %u, player_port: %u, uploader_port: %u, level: %u",
                                            stream_id.c_str(),
                                            util_ip2str(upstream_server.ip),
                                            upstream_server.backend_port,
                                            upstream_server.player_port,
                                            upstream_server.uploader_port,
                                            upstream_server.topo_level);
                                    break;
                                }
                            }
                        }
                        break;
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
            assert (mi != _forward_map.end());
            ForwardServerMapNode &server_node = mi->second;
            if (!fm_is_active(mi->second.forward_server)
                    || mi->second.ref_cnt <= 0)
            {
                INF("delete a zombie streams: ip: %s, port: %u, ref_cnt: %d, stream_id: %s",
                        util_ip2str(server_node.forward_server.ip),
                        server_node.forward_server.backend_port,
                        server_node.ref_cnt,
                        node.stream_id.c_str());
                // TODO need further optimization
                fm_unreference_forward(tli->fid);
                if (tli->level == 0 && node.has_level_0 == true)
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
* http api for merge stream, return proper receiver.
*/
server_info
ForwardServerManager::fm_get_proper_receiver_http_api(string& streamid)
{
    DBG("fm_get_proper_receiver_http_api: streamid = %s", streamid.c_str());

    server_info result;
    int found = 0;

    result.ip = 0;

    /* 1, look up stream forward list preferred */
    for (StreamNodeMapType::iterator si = _stream_map_flv.begin();
            si != _stream_map_flv.end();)
    {
        StreamNodeMapType::iterator tsi = si++;
        StreamMapNode &node = tsi->second;

        /* delete the stream entry if there's no active forward server
         * (or receiver) exist
         */
        if (tsi->second.forward_list.empty()
                && (time(0) - node.last_active_time) > 60)
        {
            INF("fm_get_proper_receiver_http_api: delete an unactive stream, stream_id: %s.", node.stream_id.c_str());
            _stream_map_flv.erase(tsi);
            continue;
        }

        INF("fm_get_proper_receiver_http_api: node_stream_id: %s.", node.stream_id.c_str());

        if(strcasecmp(node.stream_id.c_str(), streamid.c_str()) == 0)
        {
            node.last_active_time = time(0);

            /* 2, look up for proper forward server */
            for (list<StreamForwardListNode>::iterator li = node.forward_list.begin();
                    li != node.forward_list.end();)
            {
                list<StreamForwardListNode>::iterator tli = li++;

                ForwardServerMapType::iterator fmi = _forward_map.find(tli->fid);
                if (fmi == _forward_map.end())
                {
                    WRN("fm_get_proper_receiver_http_api: forward not exist!");
                    continue;
                }

                ForwardServerMapNode &server_node = fmi->second;

                if (!fm_is_active(server_node.forward_server)
                        || server_node.ref_cnt <= 0)
                {
                    INF("fm_get_proper_receiver_http_api: delete a zombie streams: ip: %s, port: %u, ref_cnt: %d, stream_id: %s",
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

                DBG("fm_get_proper_receiver_http_api: the ip of node being compared is: ip: %s; port: %u; asn: %u; region: %u; level: %u",
                        util_ip2str(server_node.forward_server.ip),
                        server_node.forward_server.backend_port,
                        server_node.forward_server.asn,
                        server_node.forward_server.region,
                        tli->level);
                if (!found && tli->level == LEVEL0)
                {
                    result.ip     = server_node.forward_server.ip;
                    result.port   = server_node.forward_server.backend_port;
                    result.level  = tli->level;
                    result.player_port = server_node.forward_server.player_port;
                    result.uploader_port = server_node.forward_server.uploader_port;

                    found = 1;

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

    return result;
}

/*
* http api for lpl_sche, return proper forward-fp.
*/
server_info
ForwardServerManager::fm_get_proper_forward_fp_http_api(string& streamid, uint16_t asn)
{
    INF("fm_get_proper_forward_fp_http_api: stream_id = %s  asn = %d", streamid.c_str(), asn);

    /*consistent hash select a forward-fp (level==1) for those forward-nfp (level >= 2) */
    server_info result;
    vector<server_info> forward_fp_set;

    if (0 == fm_generate_forward_fp_set(LCdnStreamTransportFlv, forward_fp_set, asn))
    {
        ketama_hash *shards = new ketama_hash(forward_fp_set);

        if(0 == shards->init(forward_fp_set))
        {
            result = shards->get_server_info(streamid);

            INF("the response forward_fp_http_api ip:%s, port:%u, level: %u",
                    util_ip2str(result.ip),result.port,result.level);

        }
        else
        {
            result.ip = 0;
            WRN("fm_get_proper_forward_fp_http_api: ketama_hash init failed!\n");
        }

        if (NULL != shards)
        {
            delete shards;
            shards = NULL;
        }
    }
    else
    {
        result.ip = 0;
        WRN("fm_get_proper_forward_fp_http_api: not generate forward fp set correctly!\n");
    }

    return result;
}

int
ForwardServerManager::fm_get_interlive_server_stat_http_api(string& rsp_str)
{
    INF("fm_get_interlive_server_stat_http_api");

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
        }
    }

    return 0;
}

int
ForwardServerManager::fm_get_interlive_server_stat_http_api(json_object* rsp_json, const uint16_t level, const char* ip_filters)
{
    INF("fm_get_interlive_server_stat_http_api JSON");

    json_object* forward_map_json = json_object_new_array();

    for (ForwardServerMapType::iterator i = _forward_map.begin();
            i != _forward_map.end(); i++)
    {
        if (level != (uint16_t)-1 && level != i->second.forward_server.topo_level) {
            continue;
        }
        const char* ipstr = util_ip2str(i->second.forward_server.ip);
        if (ip_filters != NULL && strstr(ip_filters, ipstr) == NULL) {
            continue;
        }

        json_object* json_msg = json_object_new_object();

        if (fm_is_active(i->second.forward_server))
        {
            ForwardServer& fs = i->second.forward_server;
            json_object* j_ip =json_object_new_string(util_ip2str(i->second.forward_server.ip));
            json_object_object_add(json_msg, "transport", json_object_new_string(
                    fid_transport_type(fs.fid) == LCdnStreamTransportFlv ? "flv" : "rtp"));
            json_object_object_add(json_msg, "ip", j_ip);
            json_object_object_add(json_msg, "backend_port", json_object_new_int(i->second.forward_server.backend_port));
            json_object_object_add(json_msg, "player_port", json_object_new_int(i->second.forward_server.player_port));
            json_object_object_add(json_msg, "uploader_port", json_object_new_int(i->second.forward_server.uploader_port));

            ForwardServerStat stat_temp;
            stat_temp = i->second.forward_server.fstat;

            json_object* j_sys_uptime =json_object_new_int64(stat_temp.sys_uptime);
            json_object* j_sys_cpu_cores =json_object_new_int(stat_temp.sys_cpu_cores);
            json_object* j_sys_cpu_idle =json_object_new_int(stat_temp.sys_cpu_idle);
            json_object* j_sys_mem_total =json_object_new_int(stat_temp.sys_mem_total);
            json_object* j_sys_mem_total_free =json_object_new_int(stat_temp.sys_mem_total_free);

            json_object* j_sys_net_rx =json_object_new_int(stat_temp.sys_net_rx);
            json_object* j_sys_net_tx =json_object_new_int(stat_temp.sys_net_tx);
            json_object* j_proc_cpu_use =json_object_new_int(stat_temp.proc_cpu_use);
            json_object* j_proc_sleepavg =json_object_new_int(stat_temp.proc_sleepavg);
            json_object* j_proc_vmpeak =json_object_new_int(stat_temp.proc_vmpeak);

            json_object* j_proc_vmsize =json_object_new_int(stat_temp.proc_vmsize);
            json_object* j_proc_total_uptime =json_object_new_int64(stat_temp.proc_total_uptime);
            json_object* j_proc_process_name =json_object_new_string(stat_temp.proc_process_name.c_str());
            json_object* j_buss_fsv2_stream_count =json_object_new_int(stat_temp.buss_fsv2_stream_count);
            json_object* j_buss_fsv2_total_session =json_object_new_int(stat_temp.buss_fsv2_total_session);

            json_object* j_buss_fsv2_active_session =json_object_new_int(stat_temp.buss_fsv2_active_session);
            json_object* j_buss_fsv2_connection_count =json_object_new_int(stat_temp.buss_fsv2_connection_count);
            json_object* j_buss_fcv2_stream_count =json_object_new_int(stat_temp.buss_fcv2_stream_count);
            json_object* j_buss_uploader_connection_count =json_object_new_int(stat_temp.buss_uploader_connection_count);
            json_object* j_buss_player_connection_count =json_object_new_int(stat_temp.buss_player_connection_count);

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
            json_object_object_add(json_msg, "proc_total_uptime", j_proc_total_uptime);
            json_object_object_add(json_msg, "proc_process_name", j_proc_process_name);
            json_object_object_add(json_msg, "buss_fsv2_stream_count", j_buss_fsv2_stream_count);
            json_object_object_add(json_msg, "buss_fsv2_total_session", j_buss_fsv2_total_session);

            json_object_object_add(json_msg, "buss_fsv2_active_session", j_buss_fsv2_active_session);
            json_object_object_add(json_msg, "buss_fsv2_connection_count", j_buss_fsv2_connection_count);
            json_object_object_add(json_msg, "buss_fcv2_stream_count", j_buss_fcv2_stream_count);
            json_object_object_add(json_msg, "buss_uploader_connection_count", j_buss_uploader_connection_count);
            json_object_object_add(json_msg, "buss_player_connection_count", j_buss_player_connection_count);

            json_object_array_add(forward_map_json, json_msg);
        }
    }

    json_object_object_add(rsp_json, "forward_map", forward_map_json);

    return 0;
}
