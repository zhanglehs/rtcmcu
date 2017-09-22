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

#include "http_connection.h"
#include "base_http_server.h"
#include "util/util.h"
#include "util/log.h"
#include <string>

using namespace std;
using namespace network;

namespace http
{
    HTTPConnection::HTTPConnection(HTTPServer* server, event_base *base)
        :TCPConnection(server, base)
    {
        state = http::CONN_DISCONNECTED;
        http_server = server;
        request = NULL;
        _write_header_buffer = NULL;
        _enable_ip_access_control = false;
    }

    HTTPConnection::~HTTPConnection()
    {
        close();
    }

    int HTTPConnection::init(int fd, sockaddr_in& s_in, int read_buffer_max, int write_buffer_max)
    {
        int ret = TCPConnection::init(fd, s_in, read_buffer_max, write_buffer_max);

        _write_header_buffer = buffer_create_max(4096, read_buffer_max);
        if (NULL == _write_header_buffer)
        {
            ERR("buffer_create_max failed.");
            _fd = -1;
            return -1;
        }

        state = CONN_READING_FIRSTLINE;

        return ret;
    }

    void HTTPConnection::close()
    {
        if (state == CONN_CLOSED)
        {
            return;
        }

        if (request != NULL)
        {
            if (request->handle_callback.close_cb != NULL)
            {
                request->handle_callback.close_cb(this, request->handle_callback.close_cb_arg);
            }
        }

        http_server = NULL;

        if (request != NULL)
        {
            delete request;
            request = NULL;
        }

        if (_write_header_buffer != NULL)
        {
            buffer_free(_write_header_buffer);
            _write_header_buffer = NULL;
        }

        TCPConnection::close();

        state = CONN_CLOSED;
    }

    uint64_t HTTPConnection::set_active()
    {
        return 0;
    }

    int HTTPConnection::_bind_request()
    {
        if (this->request != NULL)
        {

            return 0;
        }

        HTTPRequest* request = new HTTPRequest();
        if (request == NULL)
        {
            ERR("create request failed, #%d(%s:%d)",
                this->_fd, this->_remote_ip, this->_remote_port);
            return -1;
        }

        request->conn = this;
        this->request = request;

        return 0;
    }

    int HTTPConnection::_read_frist_line()
    {
        HTTPReadState res;
        HTTPRequest *req = request;
        res = req->parse_firstline();

        if (res == HTTP_DATA_CORRUPTED)
        {
            WRN("read first line failed, #%d(%s:%d)",
                _fd, _remote_ip, _remote_port);
            stop();
            return -1;
        }

        if (res == http::HTTP_MORE_DATA_EXPECTED)
        {
            return -1;
        }

        INF("parse uri, %s", req->uri.c_str());

        HTTPHandle* handle = http_server->_find_handle(this);

        if (handle == NULL)
        {
            util_http_rsp_error_code(_fd, HTTP_404);
            INF("http %s:%6hu 404, uri: %s", _remote_ip, _remote_port,
                req->uri.c_str());
            ACCESS("http %s:%6hu 404, uri: %s", _remote_ip, _remote_port,
                req->uri.c_str());
            stop();
            return -1;
        }

        if (_enable_ip_access_control)
        {
            if (!handle->ip_access_control.access_valid(this->_remote_ip))
            {
                util_http_rsp_error_code(_fd, HTTP_403);
                INF("http %s:%6hu 403, uri: %s", _remote_ip, _remote_port,
                    req->uri.c_str());
                ACCESS("http %s:%6hu 403, uri: %s", _remote_ip, _remote_port,
                    req->uri.c_str());
                stop();
                return -1;
            }
        }

        req->handle_callback = handle->callback;
        _check_uri();

        if (state != CONN_READING_FIRSTLINE)
        {
            return -1;
        }

        state = CONN_READING_HEADERS;
        _read_header();
        return 0;
    }

    bool HTTPConnection::is_alive()
    {
        if (state == CONN_CLOSING || state == CONN_CLOSED)
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    int HTTPConnection::_check_uri()
    {
        HTTPRequest *req = request;
        HTTPHandleCallback& handle_callback = req->handle_callback;

        if (handle_callback.first_line_cb != NULL)
        {
            handle_callback.first_line_cb(this, handle_callback.first_line_cb_arg);
        }

        return 0;
    }

    int HTTPConnection::_read_header()
    {
        if (state != CONN_READING_HEADERS)
        {
            return -1;
        }

        HTTPReadState res;
        HTTPRequest *req = request;

        res = req->parse_headers();
        if (res == HTTP_DATA_CORRUPTED)
        {
            WRN("read headers failed, #%d(%s:%d)",
                _fd, _remote_ip, _remote_port);
            stop();
            return -1;
        }
        else if (res == http::HTTP_MORE_DATA_EXPECTED)
        {
            return -1;
        }

        if (req->method == http::HTTP_GET)
        {
            if (req->handle_callback.cb != NULL)
            {
                req->handle_callback.cb(this, req->handle_callback.cb_arg);
            }
            return 0;
        }

        _get_body();
        return 0;
    }

    int HTTPConnection::_get_body()
    {
        HTTPRequest* req = request;

        // TODO: chunked
        if (req->headers.find("Content-Length") == req->headers.end())
        {
            WRN("can not find Content-Length in http header, #%d(%s:%d)",
                _fd, _remote_ip, _remote_port);
            stop();
            return -1;
        }
        else
        {
            string tmp = req->headers["Content-Length"];
            req->content_length = strtoll(tmp.c_str(), NULL, 10);

            DBG("Content-Length: %d, #%d(%s:%d)", (int)req->content_length,
                _fd, _remote_ip, _remote_port);
        }

        state = CONN_READING_BODY;
        _read_body();
        return 0;
    }

    int64_t HTTPConnection::get_to_read()
    {
        return _to_read;
    }

	int HTTPConnection::writeResponse(json_object* rsp) {
		//int ret;
		char header[1024];
		const char* content = json_object_to_json_string_ext(rsp, JSON_C_TO_STRING_PRETTY);
		size_t content_length = strlen(content);
		int header_len = snprintf(header, sizeof(header),
			"HTTP/1.0 200 OK\r\nServer: Youku Live Transformer\r\n"
			"Content-Type: text/plain\r\nContent-Length: %lu\r\n"
			"Expires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n"
			"\r\n", content_length);
		write(get_fd(),header,header_len);
		write(get_fd(),content,content_length);
    return 0;
	}

	int HTTPConnection::writeResponseString(std::string rsp) {
		write(get_fd(),rsp.c_str(),rsp.length());
		enable_write();
		state = CONN_WRITING;
    return 0;
	}

    int HTTPConnection::_read_body()
    {
        if (state != CONN_READING_BODY)
        {
            return -1;
        }
        HTTPRequest* req = request;

        if ((int)buffer_data_len(_read_buffer) >= req->content_length)
        {
            _to_read = 0;
        }
        else
        {
            _to_read = req->content_length - buffer_data_len(_read_buffer);
        }

        if ((int)buffer_data_len(_read_buffer) >= req->content_length)
        {
            if (req->handle_callback.cb != NULL)
            {
                req->handle_callback.cb(this, req->handle_callback.cb_arg);
            }
        }

        return 0;
    }

    void HTTPConnection::_on_read(const int fd, const short which, void* arg)
    {
        int read_bytes = 0;
        HTTPConnection *conn = (HTTPConnection *)arg;

        //int to_read = MAX_HTTP_REQ_LINE;

        //if (conn->_to_read > 0)
        //{
        //    to_read = conn->_to_read;
        //}

        read_bytes = buffer_read_fd_max(conn->_read_buffer, conn->_fd, MAX_HTTP_REQ_LINE);

        if (read_bytes == -1 || read_bytes == 0)
        {
            //if (errno != EINTR && errno != EAGAIN)
            //{
                INF("buffer_read_fd_max error, #%d(%s:%d)",
                    conn->_fd, conn->_remote_ip, conn->_remote_port);
                conn->stop();
                return;
            //}
        }
        //else if (read_bytes == 0)

        _bind_request();

        //int ret;
        switch (conn->state)
        {
        case CONN_READING_FIRSTLINE:
            _read_frist_line();
            break;

        case CONN_READING_HEADERS:
            _read_header();
            break;

        case CONN_READING_BODY:
            _read_body();
            break;

        case CONN_CLOSING:
        case CONN_READING_TRAILER:
        case CONN_DISCONNECTED:
        case CONN_CONNECTING:
        case CONN_IDLE:
        case CONN_WRITING:
        default:
            WRN("illegal connection state, #%d(%s:%d)",
                conn->_fd, conn->_remote_ip, conn->_remote_port);
            break;
        }
    }

    void HTTPConnection::_on_write(const int fd, const short which, void* arg)
    {
        //int read_bytes = 0;
        HTTPConnection *conn = (HTTPConnection *)arg;
        HTTPRequest *req = conn->request;

        int ret = buffer_write_fd(conn->_write_buffer, conn->_fd);
        if (ret < 0)
        {
            ERR("write error, #%d(%s:%d)",
                conn->_fd, conn->_remote_ip, conn->_remote_port);
            conn->stop();
        }
        else
        {
            if (req->handle_callback.cb != NULL)
            {
                req->handle_callback.cb(this, req->handle_callback.cb_arg);
            }
        }

        if (conn->state == http::CONN_CLOSING)
        {
            conn->stop();
        }
    }

    void HTTPConnection::_on_timeout(const int fd, const short which, void* arg)
    {

    }

    HTTPConnectionState HTTPConnection::stop()
    {
        state = http::CONN_CLOSING;
        return state;
    }
}

