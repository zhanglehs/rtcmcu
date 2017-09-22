/**
 * file: http_server.cpp
 * brief:   a simple http server using libevent.
 * author: duanbingnan
 * copyright: Youku
 * email: duanbingnan@youku.com
 * date: 2015/03/02
 */
#include <algorithm>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <util/log.h>
#include <util/ketama_hash.h>
#include <vector>
#include "http_server.h"
#include "tracker_config.h"

using namespace std;

const static string TRACKER_HTTPD = "tracker httpd";

extern ForwardServerManager *forward_manager;
extern tracker_config_t          *g_config;

void
root_handler(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;

    buf = evbuffer_new();
    if (buf == NULL)
    {
        ERR("failed to create response buffer");
        return;
    }
    evbuffer_add_printf(buf, "Tracker simple httpd!");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
    evbuffer_free(buf);
}

void
query_handler(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;

    buf = evbuffer_new();
    if (buf == NULL)
    {
        ERR("failed to create response buffer");
        return;
    }

    //parse request
    char *decode_uri = strdup((char*)evhttp_request_uri(req));
    //evbuffer_add_printf(buf, "decode_uri = %s\r\n", decode_uri);

    struct evkeyvalq http_query;
    evhttp_parse_query(decode_uri, &http_query);
    free(decode_uri);

    //get parameter from http request.
    const char *request_streamid_value = evhttp_find_header(&http_query, "streamid");
    //evbuffer_add_printf(buf, "request_streamid_value = %s\n", request_streamid_value);

    const char *request_asn_value = evhttp_find_header(&http_query, "asn");
    //evbuffer_add_printf(buf, "request_asn_value = %s\n", request_asn_value);

    //response http header
    evhttp_add_header(req->output_headers, "Content-Type", "text/html; charset=UTF-8");
    evhttp_add_header(req->output_headers, "Server", TRACKER_HTTPD.c_str());

    //write response value into buf
    string   req_streamid;
    uint16_t req_asn;

    stringstream ss;
    string rsp_str;
    server_info result;

    if (request_streamid_value != NULL)
    {
        req_streamid = request_streamid_value;
    }

    if (request_asn_value != NULL)
    {
        ss << request_asn_value;
        ss >> req_asn;
    }

    INF("query_handler: request_streamid = %s, request_asn = %d", req_streamid.c_str(), req_asn);

    if (forward_manager)
    {
        if (((req_asn == ASN_CNC) ||
            (req_asn == ASN_TEL) ||
            (req_asn == ASN_PRIVATE) ||
            (req_asn == ASN_CMCC)) &&
            (!req_streamid.empty()))
        {
            result = forward_manager->fm_get_proper_forward_fp_http_api(req_streamid, req_asn);
        }
        else
        {
            result.ip = 0;
            WRN("query_handler: Not supported asn : %d or stream id empty", req_asn);
        }
    }

    if (result.ip)
    {
        rsp_str = util_ip2str(result.ip);
        rsp_str += "\r\n";

        evbuffer_add_printf(buf, "%s", rsp_str.c_str());
        INF("query_handler: rsp_str = %s", rsp_str.c_str());

        //send response message
        evhttp_send_reply(req, HTTP_OK, "OK", buf);
    }
    else
    {
        //send response message
        evhttp_send_reply(req, HTTP_NOTFOUND, "Not Found", buf);
        INF("query_handler: NOT Found");
    }

    //free memory
    evhttp_clear_headers(&http_query);
    evbuffer_free(buf);

}


void
query_receiver_handler(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;

    buf = evbuffer_new();
    if (buf == NULL)
    {
        ERR("failed to create response buffer");
        return;
    }

    //parse request
    char *decode_uri = strdup((char*)evhttp_request_uri(req));
    //evbuffer_add_printf(buf, "decode_uri = %s\r\n", decode_uri);

    struct evkeyvalq http_query;
    evhttp_parse_query(decode_uri, &http_query);
    free(decode_uri);

    //get parameter from http request.
    const char *request_streamid_value = evhttp_find_header(&http_query, "streamid");
    //evbuffer_add_printf(buf, "request_streamid_value = %s\n", request_streamid_value);


    //response http header
    evhttp_add_header(req->output_headers, "Content-Type", "text/html; charset=UTF-8");
    evhttp_add_header(req->output_headers, "Server", TRACKER_HTTPD.c_str());

    //write response value into buf
    string   req_streamid;

    stringstream ss;
    string rsp_str;
    server_info result;

    if (request_streamid_value != NULL)
    {
        req_streamid = request_streamid_value;
    }

    INF("query_receiver_handler: request_streamid = %s", req_streamid.c_str());

    if (forward_manager)
    {
        result = forward_manager->fm_get_proper_receiver_http_api(req_streamid);
    }

    if (result.ip)
    {
        rsp_str = "{ \"ip\": \"";
        rsp_str += util_ip2str(result.ip);

        char sz_port[10] = "";

        rsp_str += "\", \"backend_port\":";
        memset(sz_port, 0, 10);
        snprintf(sz_port, 10, "%u", result.port);
        rsp_str += sz_port;

        rsp_str += ", \"player_port\":";
        memset(sz_port, 0, 10);
        snprintf(sz_port, 10, "%u", result.player_port);
        rsp_str += sz_port;

        rsp_str += ", \"uploader_port\":";
        memset(sz_port, 0, 10);
        snprintf(sz_port, 10, "%u", result.uploader_port);
        rsp_str += sz_port;

        rsp_str += "}";

        evbuffer_add_printf(buf, "%s", rsp_str.c_str());
        INF("query_receiver_handler: rsp_str = %s", rsp_str.c_str());

        //send response message
        evhttp_send_reply(req, HTTP_OK, "OK", buf);
    }
    else
    {
        //send response message
        evhttp_send_reply(req, HTTP_NOTFOUND, "Not Found", buf);
        INF("query_receiver_handler: NOT Found");
    }

    //free memory
    evhttp_clear_headers(&http_query);
    evbuffer_free(buf);

}

void
stat_handler(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;

    buf = evbuffer_new();
    if (buf == NULL)
    {
        ERR("failed to create response buffer");
        return;
    }

    //parse request
    char *decode_uri = strdup((char*)evhttp_request_uri(req));
    //evbuffer_add_printf(buf, "decode_uri = %s\r\n", decode_uri);

    struct evkeyvalq http_query;
    evhttp_parse_query(decode_uri, &http_query);
    free(decode_uri);

    //get parameter from http request.
    //const char *request_type = evhttp_find_header(&http_query, "receiver");
    //evbuffer_add_printf(buf, "request_type = %s\n", request_type);

    //response http header
    evhttp_add_header(req->output_headers, "Content-Type", "text/html; charset=UTF-8");
    evhttp_add_header(req->output_headers, "Server", TRACKER_HTTPD.c_str());

    json_object* rsp_json = json_object_new_object();

    if (forward_manager)
    {
        forward_manager->fm_get_interlive_server_stat_http_api(rsp_json);

        const char* content = json_object_to_json_string_ext(rsp_json, JSON_C_TO_STRING_PRETTY);

        evbuffer_add_printf(buf, "%s", content);

        evhttp_send_reply(req, HTTP_OK, "OK", buf);
    }

    json_object_put(rsp_json);

    //free memory
    evhttp_clear_headers(&http_query);
    evbuffer_free(buf);

}


void
stat_str_handler(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;

    buf = evbuffer_new();
    if (buf == NULL)
    {
        ERR("failed to create response buffer");
        return;
    }

    //parse request
    char *decode_uri = strdup((char*)evhttp_request_uri(req));
    //evbuffer_add_printf(buf, "decode_uri = %s\r\n", decode_uri);

    struct evkeyvalq http_query;
    evhttp_parse_query(decode_uri, &http_query);
    free(decode_uri);

    //get parameter from http request.
    //const char *request_type = evhttp_find_header(&http_query, "receiver");
    //evbuffer_add_printf(buf, "request_type = %s\n", request_type);

    //response http header
    evhttp_add_header(req->output_headers, "Content-Type", "text/html; charset=UTF-8");
    evhttp_add_header(req->output_headers, "Server", TRACKER_HTTPD.c_str());

    string rsp_str;

    if (forward_manager)
    {
        forward_manager->fm_get_interlive_server_stat_http_api(rsp_str);

        evbuffer_add_printf(buf, "%s", rsp_str.c_str());

        evhttp_send_reply(req, HTTP_OK, "OK", buf);
    }

    //free memory
    evhttp_clear_headers(&http_query);
    evbuffer_free(buf);

}

void
stat_receiver_handler(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;

    buf = evbuffer_new();
    if (buf == NULL)
    {
        ERR("failed to create response buffer");
        return;
    }

    //parse request
    char *decode_uri = strdup((char*)evhttp_request_uri(req));
    //evbuffer_add_printf(buf, "decode_uri = %s\r\n", decode_uri);

    struct evkeyvalq http_query;
    evhttp_parse_query(decode_uri, &http_query);
    free(decode_uri);

    //get query args
    const char *ip_filter = evhttp_find_header(&http_query, "ip");

    //response http header
    evhttp_add_header(req->output_headers, "Content-Type", "application/json; charset=UTF-8");
    evhttp_add_header(req->output_headers, "Server", TRACKER_HTTPD.c_str());
    evhttp_add_header(req->output_headers, "Connection", "close");

    json_object* rsp_json = json_object_new_object();

    if (forward_manager)
    {
        forward_manager->fm_get_interlive_server_stat_http_api(rsp_json, LEVEL0, ip_filter);

        const char* content = json_object_to_json_string_ext(rsp_json, JSON_C_TO_STRING_PLAIN);

        evbuffer_add_printf(buf, "%s", content);

        evhttp_send_reply(req, HTTP_OK, "OK", buf);
    }

    json_object_put(rsp_json);

    //free memory
    evhttp_clear_headers(&http_query);
    evbuffer_free(buf);

}

void
get_streams_upstreams_handler(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf = evbuffer_new();
    if (buf == NULL)
    {
        ERR("failed to create response buffer");
        return;
    }

    bool success = true;
    int code = 200;
    std::string status = "OK";
    std::string message("success");
    std::vector<ForwardServer> upstream_servers;

    const char* path = evhttp_request_uri(req);
    if (strlen(path) < strlen("/streams/00000000000000000000000000000000/upstreams"))
    {
        // todo: say something
        return;
    }

    std::string stream_id(path + strlen("/streams/"), 32);
    std::transform(stream_id.begin(), stream_id.end(), stream_id.begin(), ::toupper);

    struct evkeyvalq query_args;
    evhttp_parse_query(path, &query_args);

    //get query args
    const char* client_ip_str = evhttp_find_header(&query_args, "ip");
    const char* client_port_str = evhttp_find_header(&query_args, "port");
    const char* client_asn_str = evhttp_find_header(&query_args, "asn");
    const char* client_region_str = evhttp_find_header(&query_args, "region");
    const char* client_level_str = evhttp_find_header(&query_args, "level");
    const char* client_transport_str = evhttp_find_header(&query_args, "transport");

    uint32_t client_ip = 0;
    uint16_t client_port = 0;
    uint16_t client_asn = client_asn_str ? strtol(client_asn_str, NULL, 10) : -1;
    uint16_t client_region = client_region_str ? strtol(client_region_str, NULL, 10) : -1;
    uint16_t client_level = client_level_str ? strtol(client_level_str, NULL, 10) : -1;
    LCdnStreamTransportType client_transport = LCdnStreamTransportFlv;
    //check required args
    if (success)
    {
        struct in_addr ip;
        if (!client_ip_str || strlen(client_ip_str) == 0 || !inet_aton(client_ip_str, &ip))
        {
            success = false;
            code = 400;
            status = "Bad Request";
            message = "invalid ip in query string";
        }
        // to satisfy our convention(e.g. log formatting), we save 32bit IP in host order
        client_ip = ntohl(ip.s_addr);
    }
    if (success)
    {
        if (!client_port_str || strlen(client_port_str) == 0)
        {
            success = false;
            code = 400;
            status = "Bad Request";
            message = "invalid port in query string";
        }
        client_port = strtol(client_port_str, NULL, 10);
    }

    if (success)
    {
        if (!client_asn_str || strlen(client_asn_str) == 0)
        {
            success = false;
            code = 400;
            status = "Bad Request";
            message = "invalid asn in query string";
        }
        client_asn = strtol(client_asn_str, NULL, 10);
    }

    if (success)
    {
        if (!client_region_str || strlen(client_region_str) == 0)
        {
            success = false;
            code = 400;
            status = "Bad Request";
            message = "invalid region in query string";
        }
        client_region = strtol(client_region_str, NULL, 10);
    }

    if (success)
    {
        if (!client_level_str || strlen(client_level_str) == 0)
        {
            success = false;
            code = 400;
            status = "Bad Request";
            message = "invalid level in query string";
        }
        client_level = strtol(client_level_str, NULL, 10);
    }

    if (success)
    {
        if (!client_transport_str || strlen(client_transport_str) == 0)
        {
            success = false;
            code = 400;
            status = "Bad Request";
            message = "invalid transport in query string";
        }

        if (strcasecmp(client_transport_str, "flv") == 0)
        {
            client_transport = LCdnStreamTransportFlv;
        }
        else if (strcasecmp(client_transport_str, "rtp") == 0)
        {
            client_transport = LCdnStreamTransportRtp;
        }
        else
        {
            success = false;
            code = 400;
            status = "Bad Request";
            message = "invalid transport in query string";
        }
    }

    //response http header
    evhttp_add_header(req->output_headers, "Content-Type", "application/json; charset=UTF-8");
    evhttp_add_header(req->output_headers, "Server", TRACKER_HTTPD.c_str());

    if (success)
    {
        ForwardServer upstream_server;
        int error = forward_manager->fm_get_proper_forward_v3(upstream_server,
                                                              client_transport,
                                                              stream_id,
                                                              client_ip,
                                                              client_port,
                                                              client_asn,
                                                              client_region,
                                                              client_level);
        if (error)
        {
            success = false;
            code = 404;
            status = "Not Found";
            message = "not found";
        }
        else
        {
            upstream_servers.push_back(upstream_server);
        }
    }


    json_object* rsp_json = json_object_new_object();
    json_object_object_add(rsp_json, "success", json_object_new_boolean(success));
    json_object_object_add(rsp_json, "code", json_object_new_int(code));
    json_object_object_add(rsp_json, "description", json_object_new_string(message.c_str()));
    json_object* forwards_json = json_object_new_array();
    for (size_t i = 0; i < upstream_servers.size(); ++i)
    {
        ForwardServer& forward = upstream_servers[i];
        json_object* forward_json = json_object_new_object();
        struct in_addr ip;
        // This might be a bug, we are storing Little-Endian(aka. Host Order) 32bit IP address
        ip.s_addr = htonl(forward.ip);
        json_object_object_add(forward_json, "ip", json_object_new_string(inet_ntoa(ip)));
        json_object_object_add(forward_json, "backend_port", json_object_new_int(forward.backend_port));
        json_object_object_add(forward_json, "player_port", json_object_new_int(forward.player_port));
        json_object_object_add(forward_json, "uploader_port", json_object_new_int(forward.uploader_port));
        json_object_object_add(forward_json, "asn", json_object_new_int(forward.asn));
        json_object_object_add(forward_json, "region", json_object_new_int(forward.region));
        json_object_object_add(forward_json, "level", json_object_new_int(forward.topo_level));
        json_object_object_add(forward_json, "transport", json_object_new_string(client_transport_str));
        json_object_array_add(forwards_json, forward_json);
    }
    json_object_object_add(rsp_json, "forwards", forwards_json);
    evbuffer_add_printf(buf, "%s", json_object_to_json_string(rsp_json));
    evhttp_send_reply(req, code, status.c_str(), buf);
    json_object_put(rsp_json);
    evhttp_clear_headers(&query_args);
    evbuffer_free(buf);
}

void streams_handler(struct evhttp_request *req, void *arg)
{
    if (req->type == EVHTTP_REQ_GET) {
        if (strncmp(req->uri + strlen("/streams/00000000000000000000000000000000"), "/upstreams", strlen("/upstreams")) == 0)
        {
            get_streams_upstreams_handler(req, arg);
            return;
        }
    }
    struct evbuffer *buf = evbuffer_new();
    evbuffer_add_printf(buf, "uri %s not registered", req->uri);
    evhttp_send_reply(req, 404, "Not Found", buf);
    evbuffer_free(buf);
}

// We only have fid here, we need more data to present
class network_link {
public:
    uint64_t source_fid;
    uint64_t target_fid;

    bool operator<(const network_link& other) const
    {
        return source_fid == other.source_fid ? target_fid < other.target_fid : source_fid < other.source_fid;
    }
};

void
network_json_handler(struct evhttp_request *req, void* arg)
{
    std::set<uint64_t> nodes;
    std::set<network_link> links;

    std::map<std::string, StreamMapNode> clone;
    forward_manager->clone_stream_map_flv(clone);

    for (std::map<std::string, StreamMapNode>::iterator i = clone.begin(); i != clone.end(); i++)
    {
        uint64_t upstream_fid = 0;
        uint64_t last_node_fid = 0;
        uint16_t last_node_level = -1;

        for (std::list<StreamForwardListNode>::iterator li = i->second.forward_list.begin();
                li != i->second.forward_list.end();
                li++)
        {
            StreamForwardListNode& forward_node = *li;
            nodes.insert(forward_node.fid);

            if (!upstream_fid)
            {
                // this is first node
                upstream_fid = forward_node.fid;
            }
            else
            {
                if (last_node_level != forward_node.level)
                {
                    upstream_fid = last_node_fid;
                }

                network_link link;
                link.source_fid = upstream_fid;
                link.target_fid = forward_node.fid;
                links.insert(link);
            }

            last_node_level = forward_node.level;
            last_node_fid = forward_node.fid;
        }
    }

       //response http header
    evhttp_add_header(req->output_headers, "Content-Type", "application/vnd.tracker.v1+json");
    evhttp_add_header(req->output_headers, "Server", TRACKER_HTTPD.c_str());
    evhttp_add_header(req->output_headers, "Connection", "close");

    json_object* nodes_json = json_object_new_array();
    json_object* links_json = json_object_new_array();

    std::map<uint64_t, uint32_t> node_index;
    uint32_t index = 0;
    for (std::set<uint64_t>::iterator node_it = nodes.begin(); node_it != nodes.end(); node_it++)
    {
        json_object* node = json_object_new_object();
        json_object_object_add(node, "name", json_object_new_int64(*node_it));
        json_object_array_add(nodes_json, node);

        node_index.insert(std::make_pair(*node_it, index++));
    }
    for (std::set<network_link>::iterator link_it = links.begin(); link_it != links.end(); link_it++)
    {
        const network_link& l = *link_it;
        json_object* link = json_object_new_object();
        json_object_object_add(link, "source", json_object_new_int(node_index[l.source_fid]));
        json_object_object_add(link, "target", json_object_new_int(node_index[l.target_fid]));
        json_object_object_add(link, "value", json_object_new_int(1));
        json_object_array_add(links_json, link);
    }
    json_object* rsp_json = json_object_new_object();
    json_object_object_add(rsp_json, "nodes", nodes_json);
    json_object_object_add(rsp_json, "links", links_json);

    const char* content = json_object_to_json_string_ext(rsp_json, JSON_C_TO_STRING_PLAIN);
    struct evbuffer* buf = evbuffer_new();
    evbuffer_add_printf(buf, "%s", content);
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
    evbuffer_free(buf);
    json_object_put(rsp_json);

    return;
}

void
generic_handler(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;

    buf = evbuffer_new();
    if (buf == NULL)
    {
        ERR("failed to create response buffer");
        return;
    }

    evbuffer_add_printf(buf, "Requested: %s/n", evhttp_request_uri(req));
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
    evbuffer_free(buf);
}

int
tracker_start_http_server(struct event_base *base)
{
    struct evhttp *httpd;

    short          http_port = 8888;
    char          *http_addr = "0.0.0.0";

    if (NULL != g_config)
    {
        http_port = g_config->http_listen_port;
    }

    httpd = evhttp_new(base);
    if (httpd == NULL)
    {
        ERR("evhttp_new error");
        return -1;
    }

    if (evhttp_bind_socket(httpd,http_addr,http_port) == -1)
    {
        ERR("evhttp_bind_socket error");
        free(httpd);
        return -1;
    }

    /* Set a callback for requests to "/" */
    //evhttp_set_cb(httpd, "/", root_handler, NULL);

    /* Set a callback for requests to "/query" */
    evhttp_set_cb(httpd, "/query", query_handler, NULL);

    /* Set a callback for requests to "/query_receiver" */
    evhttp_set_cb(httpd, "/query_receiver", query_receiver_handler, NULL);

    /* Set a callback for all other requests. */
    evhttp_set_cb(httpd, "/stat/all", stat_handler, NULL);

    evhttp_set_cb(httpd, "/stat/allstr", stat_str_handler, NULL);

    /* uploading scheduler query this to choose between receiver */
    evhttp_set_cb(httpd, "/stat/receiver", stat_receiver_handler, NULL);

    //evhttp_set_cb(httpd, "/network", network_html_handler, NULL);
    //evhttp_set_cb(httpd, "/network.htm", network_html_handler, NULL);
    //evhttp_set_cb(httpd, "/network.html", network_html_handler, NULL);
    evhttp_set_cb(httpd, "/network.json", network_json_handler, NULL);

    evhttp_set_cb(httpd, "/streams/", streams_handler, NULL);

    /* Set a callback for all other requests. */
    evhttp_set_gencb(httpd, generic_handler, NULL);

    /*here we don't call dispatch function, because tracker main.cpp will call*/
    //event_base_dispatch(base);

    /* Not reached in this code as it is now. */
    //evhttp_free(httpd);

    INF("%s","tracker_start_http_server succ");

    return 0;
}

