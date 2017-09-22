/**
* @file
* @brief
* @author   songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/07/24
* @see
*/

#pragma once

#include <event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>

#include "http_request.h"
#include "tcp_connection.h"

#include "utils/buffer.hpp"
#include <json.h>
namespace http
{
    enum HTTPConnectionState
    {
        CONN_DISCONNECTED,	/**< not currently connected not trying either*/
        CONN_CONNECTING,	/**< tries to currently connect */
        CONN_IDLE,		/**< connection is established */
        CONN_READING_FIRSTLINE,/**< reading Request-Line (incoming conn) or < Status-Line (outgoing conn) */
        CONN_READING_HEADERS,	/**< reading request/response headers */
        CONN_READING_BODY,	/**< reading request/response body */
        CONN_READING_TRAILER,	/**< reading request/response chunked trailer */
        CONN_WRITING,		/**< writing request/response headers/body */
        CONN_CLOSING,
        CONN_CLOSED
    };

    class HTTPServer;
    class HTTPConnection : public network::TCPConnection
    {
        friend class HTTPServer;
        friend class HTTPRequest;
    public:
        HTTPConnection(HTTPServer*, event_base*);
        virtual ~HTTPConnection();
        virtual int init(int fd, sockaddr_in& s_in, int read_buffer_max, int write_buffer_max);
        virtual void close();
        uint64_t set_active();
        virtual bool is_alive();
        HTTPConnectionState stop();
        int64_t get_to_read();
		int writeResponse(json_object* rsp);
		int writeResponseString(std::string rsp);

    protected:
        int _bind_request();
        int _read_frist_line();
        int _check_uri();
        int _read_header();
        int _get_body();
        int _read_body();

        void _on_read(const int fd, const short which, void* arg);
        void _on_write(const int fd, const short which, void* arg);
        void _on_timeout(const int fd, const short which, void* arg);

    public:


        int flags;
#define HTTP_CON_INCOMING	0x0001	/* only one request on it ever */
#define HTTP_CON_OUTGOING	0x0002  /* multiple requests possible */
#define HTTP_CON_CLOSEDETECT  0x0004  /* detecting if persistent close */

        //        int timeout;			/* timeout in seconds for events */
        //        int retry_cnt;			/* retry count */
        //        int retry_max;			/* maximum number of retries */

        HTTPConnectionState state;

        /* for server connections, the http server they are connected with */
        HTTPServer *http_server;

        HTTPRequest* request;

        buffer* _write_header_buffer;
        bool _enable_ip_access_control;
    };

}
