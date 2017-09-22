/************************************************************************
* author: zhangcan
* email: zhangcan@youku.com
* date: 2014-12-30
* copyright:youku.com
************************************************************************/

#include "client.h"
#include "util/util.h"
#include "util/log.h"
#include "util/access.h"
#include "fragment/fragment.h"
#include "connection.h"
#include "target.h"
#include "target_player.h"

#include <string>

using namespace fragment;
using namespace avformat;
using namespace std;

#define WRITE_MAX (64 * 1024)
#define NOCACHE_HEADER "Expires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n"
#define HTTP_CONNECTION_HEADER "Connection: Close\r\n"

void SDPClient::request_data()
{
    switch (get_play_request().get_play_request_type())
    {
    case PlayRequest::RequestSDP: 
        request_sdp_data();
        break;
    default:
        break;
    }
}

void SDPClient::request_sdp_data()
{
    connection *c = get_connection();
    PlayRequest &pr = get_play_request();

    string sdp_str;
    get_sdp_cb(pr.get_sid(), sdp_str);

    if (sdp_str.length() > 0) {
        // response header
        char rsp[1024];
        int used = 0;

        used = snprintf(rsp, sizeof(rsp),
            "HTTP/1.0 200 OK\r\nServer: Youku Live Forward\r\n"
            "Content-Type: application/sdp\r\n"
            "Host: %s:%d\r\n"
            HTTP_CONNECTION_HEADER
            NOCACHE_HEADER "Content-Length: %zu\r\n\r\n",
            c->local_ip, c->local_port, sdp_str.length());

        if (buffer_append_ptr(c->w, rsp, used) < 0)
        {
            WRN("player-sdp %s:%6hu %s %s?%s 404",
                c->remote_ip, c->remote_port, pr.get_method().c_str(), pr.get_path().c_str(), pr.get_query().c_str());
            set_stream_state(SDPClient::SDP_STOP);
            return;
        }

        buffer_append_ptr(c->w, sdp_str.c_str(), sdp_str.length());

        DBG("player-sdp %s:%6hu %s %s?%s 200",
            c->remote_ip, c->remote_port, pr.get_method().c_str(), pr.get_path().c_str(), pr.get_query().c_str());
        set_stream_state(SDPClient::SDP_STOP);
    }
    else
    {
        WRN("player-sdp %s:%6hu %s %s?%s 404",
            c->remote_ip, c->remote_port, pr.get_method().c_str(), pr.get_path().c_str(), pr.get_query().c_str());
        util_http_rsp_error_code(c->fd, HTTP_404);
        set_stream_state(SDPClient::SDP_STOP);
    }
}
