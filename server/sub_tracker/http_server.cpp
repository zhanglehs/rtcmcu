/**
 * file: http_server.cpp
 * brief:   a simple http server using libevent.
 * author: duanbingnan
 * copyright: Youku
 * email: duanbingnan@youku.com
 * date: 2015/03/02
 */

#include <util/log.h>
#include <util/ketama_hash.h>
#include "tracker_config.h"
#include "http_server.h"

using namespace std;

const static string TRACKER_HTTPD = "tracker httpd";

extern ForwardServerManager *forward_manager;
extern tracker_config_t *g_config;

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

    const char *request_group_id_value = evhttp_find_header(&http_query, "groupid");

    //response http header
    evhttp_add_header(req->output_headers, "Content-Type", "text/html; charset=UTF-8");
    evhttp_add_header(req->output_headers, "Server", TRACKER_HTTPD.c_str());
    evhttp_add_header(req->output_headers, "Connection", "close");
    //write response value into buf
    string   req_streamid;
    uint16_t req_asn = 0;
    uint32_t req_group_id = 0;

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

    if (request_group_id_value != NULL)
    {
        ss.clear();
        ss << request_group_id_value;
        ss >> req_group_id; 
    }
    

    INF("query_handler: request_streamid = %s, request_asn = %d, req_group_id = %d", req_streamid.c_str(), req_asn, req_group_id);

    if (forward_manager)
    {
        if (((request_asn_value != NULL) || (request_group_id_value != NULL))
                && (!req_streamid.empty()))
        {
            if (request_asn_value != NULL)
            {
                if ((req_asn == ASN_CNC) || (req_asn == ASN_TEL) 
                        || (req_asn == ASN_CNC_RTP) || (req_asn == ASN_TEL_RTP))
                {

                    result = forward_manager->fm_get_proper_forward_fp_http_api(req_streamid, req_asn);
                }
                else
                {
                    result.ip = 0;
                    WRN("query_handler: Not supported asn : %d", req_asn);
                }

            }
            else if (request_group_id_value != NULL)
            {
                result = forward_manager->fm_get_proper_forward_nfp_http_api(req_streamid, req_group_id);
                INF("get_proper_forward_nfp, ip:%s, port:%d", util_ip2str(result.ip), result.port); 
            }
            else
            {
                result.ip = 0;
                WRN("query_handler: asn or groupid  empty");
            }
        }
        else
        {
            result.ip = 0;
            WRN("query_handler: stream id empty");
        }
    }

    if (result.ip)
    {
        rsp_str = "{ \"ip\": \"";
        rsp_str += util_ip2str(result.ip);
        rsp_str += "\", \"port\":";

        char sz_port[10] = "";
        memset(sz_port, 0, 10);
        snprintf(sz_port, 10, "%u", result.port);

        rsp_str += sz_port;
        rsp_str += "}";

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
    evhttp_add_header(req->output_headers, "Connection", "close");

    json_object* rsp_json = json_object_new_object();

    if (forward_manager)
    {
        forward_manager->fm_get_interlive_server_stat_http_api(rsp_json);

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

    short          http_port = g_config->http_listen_port;
    char          *http_addr = "0.0.0.0";

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

    /* Set a callback for all other requests. */
    evhttp_set_cb(httpd, "/stat/all", stat_handler, NULL);

    evhttp_set_cb(httpd, "/stat/allstr", stat_str_handler, NULL);

    /* Set a callback for all other requests. */
    evhttp_set_gencb(httpd, generic_handler, NULL);

    /*here we don't call dispatch function, because tracker main.cpp will call*/
    //event_base_dispatch(base);

    /* Not reached in this code as it is now. */
    //evhttp_free(httpd);

    INF("%s","tracker_start_http_server succ");

    return 0;
}

