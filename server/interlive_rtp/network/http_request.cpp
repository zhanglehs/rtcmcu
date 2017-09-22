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

#include "http_request.h"
#include "http_connection.h"
#include "utils/buffer.hpp"
#include "util/log.h"
#include <string>
#include <stdlib.h>
#include <assert.h>

using namespace std;

namespace http
{
    HTTPHandleCallback::HTTPHandleCallback()
        :first_line_cb(NULL),
        first_line_cb_arg(NULL),
        cb(NULL),
        cb_arg(NULL),
        close_cb(NULL),
        close_cb_arg(NULL)
    {

    }

    HTTPHandle::HTTPHandle()
    {

    }
    
    
    HTTPRequest::HTTPRequest()
    {
        conn = 0;
        method = http::HTTP_INVALID;
        flags = 0;
        content_length = 0;
        major = 0;
        minor = 0;
    }

    HTTPRequest::~HTTPRequest()
    {

    }

    void HTTPRequest::make_reponse_first_line(HTTPCode code, const char* reason, int reason_len)
    {
        const char* code_str = get_code_str(code);

        if (code_str == NULL && (reason == NULL || reason_len == 0))
        {
            // TODO: error
            return;
        }

        //char* http_1_0 = "HTTP/1.0";
        char temp1[1024];
        sprintf(temp1, "HTTP/1.0 %d %s\r\n", code, code_str);
        buffer_append_ptr(conn->_write_buffer, temp1, strlen(temp1));
    }


#ifndef HAVE_STRSEP
    /* strsep replacement for platforms that lack it.  Only works if
    * del is one character long. */
    static char *strsep(char **s, const char *del)
    {
        char *d, *tok;
        assert(strlen(del) == 1);
        if (!s || !*s)
            return NULL;
        tok = *s;
        d = strstr(tok, del);
        if (d) {
            *d = '\0';
            *s = d + 1;
        }
        else
            *s = NULL;
        return tok;
    }
#endif


    int HTTPRequest::parse_request_line(char *line)
    {
        char *method_char;
        //char *uri_char;
        char *version_char;

        /* Parse the request line */
        method_char = strsep(&line, " ");
        if (line == NULL)
            return (-1);
        uri = strsep(&line, " ");
        if (line == NULL)
            return (-1);
        version_char = strsep(&line, " ");
        if (line != NULL)
            return (-1);

        /* First line */
        if (strcmp(method_char, "GET") == 0) {
            method = HTTP_GET;
        }
        else if (strcmp(method_char, "PUT") == 0) {
            method = HTTP_PUT;
        }
        else if (strcmp(method_char, "POST") == 0) {
            method = HTTP_POST;
        }
        else
        {
            WRN("bad method %s on request from %s",
                method_char, conn->_remote_ip);
            //TODO: ACCESS
            return (-1);
        }

        if (strcmp(version_char, "HTTP/1.0") == 0) {
            major = 1;
            minor = 0;
        }
        else if (strcmp(version_char, "HTTP/1.1") == 0) {
            major = 1;
            minor = 1;
        }
        else
        {
            WRN("bad version %s on request from %s",
                version_char, conn->_remote_ip);
            //TODO: ACCESS
            return (-1);
        }

        if (uri == "")
        {
            WRN("req->uri is empty");
            return (-1);
        }

        /* determine if it's a proxy request */
        if (uri.length() > 0 && uri[0] != '/')
        {
            flags |= HTTP_PROXY_REQUEST;
        }

        return (0);
    }

    static char* buffer_readline(buffer* buf)
    {
        const char* data = buffer_data_ptr(buf);
        int len = buffer_data_len(buf);
        char* line;
        int i;

        for (i = 0; i < len; i++)
        {
            if (data[i] == '\r' || data[i] == '\n')
                break;
        }

        if (i == len)
        {
            return (NULL);
        }

        line = (char*)malloc(i + 1);

        if (line == NULL)
        {
            ERR("out of memory");
            return (NULL);
        }

        memcpy(line, data, i);
        line[i] = '\0';

        /*
        * Some protocols terminate a line with '\r\n', so check for
        * that, too.
        */
        if (i < len - 1)
        {
            char fch = data[i], sch = data[i + 1];

            /* Drain one more character if needed */
            if ((sch == '\r' || sch == '\n') && sch != fch)
                i += 1;
        }

        buffer_eat(buf, i + 1);

        return line;
    }


    HTTPReadState HTTPRequest::parse_headers()
    {
        char* line;
        HTTPReadState state = http::HTTP_MORE_DATA_EXPECTED;
        buffer* buf = conn->_read_buffer;

        while (true)
        {
            line = buffer_readline(buf);
            if (line == NULL)
            {
                break;
            }

            if (*line == '\0')
            { /* Last header - Done */
                state = http::HTTP_ALL_DATA_READ;
                free(line);
                break;
            }

            /* Check if this is a continuation line */
            // TODO
            /*if (*line == ' ' || *line == '\t') {
            if (evhttp_append_to_last_header(headers, line) == -1)
            goto error;
            free(line);
            continue;
            }*/

            char *skey, *svalue;
            svalue = line;
            skey = strsep(&svalue, ":");
            if (svalue == NULL)
            {
                free(line);
                return HTTP_DATA_CORRUPTED;
            }

            svalue += strspn(svalue, " ");

            string key(skey);
            string value(svalue);

            headers[key] = value;

            free(line);
        }

        return state;
    }

    HTTPReadState HTTPRequest::parse_firstline()
    {
        buffer* buf = conn->_read_buffer;
        HTTPReadState state = HTTP_ALL_DATA_READ;

        char* line = buffer_readline(buf);

        if (line == NULL)
        {
            return http::HTTP_MORE_DATA_EXPECTED;
        }

        int ret = 0;

        ret = parse_request_line(line);
        if (ret == -1)
        {
            state = HTTP_DATA_CORRUPTED;
        }

        free(line);
        return state;
    }

    void HTTPRequest::add_response_header(std::string key, std::string value)
    {
        string line = key + ": " + value + "\r\n";
        buffer_append_ptr(conn->_write_buffer, line.c_str(), line.length());
    }

    void HTTPRequest::done_response_header()
    {
        string line = "\r\n";
        buffer_append_ptr(conn->_write_buffer, line.c_str(), line.length());
    }

    void HTTPRequest::generate_response_headers()
    {
        add_response_header("Server", "Youku Live Server");
    }

    void HTTPRequest::add_content(uint8_t* content, int32_t len)
    {
        char temp[128];
        sprintf(temp, "%d", len);
        add_response_header("Content-Length", string(temp));
        done_response_header();
        buffer_append_ptr(conn->_write_buffer, content, len);

        conn->enable_write();
        conn->state = CONN_WRITING;
    }

    const char* HTTPRequest::get_code_str(HTTPCode code)
    {
        switch (code)
        {
        case http::HTTP_CODE_OK: // 200
            return "OK";
            break;
        case http::HTTP_CODE_BAD_REQUEST: // 400
            return "Bad Request";
            break;
        case http::HTTP_CODE_NOT_FOUND: // 404
            return "Not Found";
            break;
        case http::HTTP_CODE_INTERNAL_SERVER_ERROR: // 500
            return "Internal Server Error";
            break;
        default:
            break;
        }

        // TODO: warning
        return NULL;
    }

}
