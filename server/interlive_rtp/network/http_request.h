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

#include <string>
#include <map>
#include <stdint.h>
#include "ip_access_control.h"

namespace http
{
    class HTTPConnection;

    typedef void(*http_cb)(HTTPConnection*, void*);


    enum HTTPMethod
    {
        HTTP_INVALID = 0,
        HTTP_GET = 1,
        HTTP_PUT = 2,
        HTTP_POST = 3,
        HTTP_HEAD = 4,
    };

    class HTTPHandleCallback
    {
    public:
        HTTPHandleCallback();

    public:
        http_cb first_line_cb;
        void *first_line_cb_arg;

        http_cb cb;
        void *cb_arg;

        http_cb close_cb;
        void *close_cb_arg;
    };

    class HTTPHandle
    {
    public:
        HTTPHandle();
        
    public:
        HTTPHandleCallback callback;

        std::string start_path;

        network::IPAccessControl ip_access_control;
    };

    enum HTTPCode
    {
        HTTP_CODE_OK = 200,
        HTTP_CODE_BAD_REQUEST = 400,
        HTTP_CODE_NOT_FOUND = 404,
        HTTP_CODE_INTERNAL_SERVER_ERROR = 500,
    };

    enum HTTPReadState
    {
        HTTP_ALL_DATA_READ = 1,
        HTTP_MORE_DATA_EXPECTED = 0,
        HTTP_DATA_CORRUPTED = -1,
        HTTP_REQUEST_CANCELED = -2
    };

    enum HTTPWriteState
    {
        HTTP_HEADER_WRITING = 1,
        HTTP_HEADER_WRITEN,
        HTTP_CONTENT_WRITING,
        HTTP_CONTENT_WRITEN,
    };

    class HTTPRequest
    {
        friend class HTTPConnection;
    public:
        HTTPRequest();
        virtual ~HTTPRequest();

        HTTPReadState parse_firstline();
        int parse_request_line(char *line);
        HTTPReadState parse_headers();

        void make_reponse_first_line(HTTPCode code, const char* reason, int reason_len);
        void add_response_header(std::string key, std::string value);
        void done_response_header();
        void generate_response_headers();
        const char* get_code_str(HTTPCode code);
        void add_content(uint8_t* content, int32_t len);

    public:
        HTTPConnection* conn;
        HTTPMethod method;

        int flags;

#define HTTP_REQ_OWN_CONNECTION	0x0001
#define HTTP_PROXY_REQUEST		0x0002

        std::string uri;

        int64_t content_length;

        uint8_t major;			/* HTTP Major number */
        uint8_t minor;			/* HTTP Minor number */

        std::map<std::string, std::string> headers;

        HTTPHandleCallback handle_callback;
    };
}
