#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "buffer.h"

buffer *
buffer_init(int size)
{
    buffer *b;

    b = calloc(1, sizeof(*b));
    if (b) {
        b->used = 0;
        b->size = size;
#define BUFFER_PIECE_SIZE 64
        b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);
#undef BUFFER_PIECE_SIZE
        b->ptr = calloc(1, b->size);
        if (b->ptr == NULL) {
            free(b);
            b = NULL;
        }
    }
    return b;
}

void
buffer_free(buffer *b)
{
    if (b) {
        free(b->ptr);
        free(b);
    }
}

void
buffer_expand(buffer *b, int size)
{
    char *p;

    if (b && b->size < size) {
#define BUFFER_PIECE_SIZE 64
        size += BUFFER_PIECE_SIZE - (size % BUFFER_PIECE_SIZE);
#undef BUFFER_PIECE_SIZE
        p = realloc(b->ptr, size);
        if (p) {
            b->ptr = p;
            b->size = size;
        }
    }
}

void
buffer_append(buffer *dst, buffer *src)
{
    if(dst == NULL || dst->ptr == NULL || src == NULL || src->ptr == NULL || src->used == 0) return;

    if ((dst->used + src->used) > dst->size)
        buffer_expand(dst, dst->used + src->used + 1);

    if ((dst->used + src->used) > dst->size) /* realloc failed */
        return;
    memcpy(dst->ptr + dst->used, src->ptr, src->used);
    dst->used += src->used;
}

void
buffer_copy(buffer *dst, buffer *src)
{
    if(dst == NULL || dst->ptr == NULL || src == NULL || src->ptr == NULL || src->used == 0) return;

    if (src->used > dst->size) buffer_expand(dst, src->used + 1);

    if (src->used > dst->size) /* realloc failed */
        return;
    memcpy(dst->ptr, src->ptr, src->used);
    dst->used = src->used;
}

int
buffer_ignore_safe(buffer * buf, int bytes)
{
    if(NULL == buf || bytes <= 0 || bytes > buf->used) return -1;

    memmove(buf->ptr, buf->ptr + bytes, buf->used - bytes);
    buf->used -= bytes;
    return bytes;
}

int
buffer_ignore(buffer * buf, int bytes)
{
    if(NULL == buf || bytes <= 0) return 0;

    if(bytes >= buf->used)
    {
        int ret = buf->used;
        buf->used = 0;
        return ret;
    }

    memmove(buf->ptr, buf->ptr + bytes, buf->used - bytes);
    buf->used -= bytes;
    return bytes;
}

uint32_t
FLV_UI16(unsigned char* buf)
{
    return (buf[0]<<8) + buf[1];
}

uint32_t
FLV_UI24(unsigned char* buf)
{
    return (((buf[0]<<8) + buf[1])<<8) + buf[2];
}

uint32_t
FLV_UI32(unsigned char* buf)
{
    return (((((buf[0]<<8) + buf[1])<<8) + buf[2])<<8) + buf[3];
}

int32_t
flv_get_timestamp(unsigned char * ts, unsigned char ts_ex)
{
    return (ts_ex << 24) + (ts[0] << 16) + (ts[1] << 8) + ts[2];
}

void
flv_set_timestamp(unsigned char * ts, unsigned char * ts_ex, int32_t value)
{
    *ts_ex = (value >> 24) & 0xff;
    ts[0] = (value >> 16) & 0xff;
    ts[1] = (value >> 8) & 0xff;
    ts[2] = value & 0xff;
}

double
get_double(unsigned char* flv)
{
    union { unsigned char dc[8]; double dd;} d;
    d.dc[0] = flv[7];
    d.dc[1] = flv[6];
    d.dc[2] = flv[5];
    d.dc[3] = flv[4];
    d.dc[4] = flv[3];
    d.dc[5] = flv[2];
    d.dc[6] = flv[1];
    d.dc[7] = flv[0];
    return d.dd;
}

int
memstr(char *s, char *find, int srclen, int findlen)
{
    char *bp, *sp;
    int len = 0, success = 0;

    if (findlen == 0 || srclen < findlen) return -1;
    for (len = 0; len <= (srclen-findlen); len ++) {
        if (s[len] == find[0]) {
            bp = s + len;
            sp = find;
            do {
                if (!*sp) {
                    success = 1;
                    break;
                }
            } while (*bp++ == *sp++);
            if (success) break;
        }
    }

    if (success) return len;
    else return -1;
}

void
append_script_datastr(buffer *b, char *s, int len)
{
    if (b == NULL || b->ptr == NULL || s == NULL || len == 0) return;

    if ((b->used + len) > b->size)
        buffer_expand(b, b->used + len + 1);

    b->ptr[b->used ++] = len >> 16;
    b->ptr[b->used ++] = len & 0xff;

    memcpy(b->ptr + b->used, s, len);
    b->used += len;
}

void
append_script_var_str(buffer *b, char *s, int len)
{
    if (b == NULL || b->ptr == NULL || s == NULL || len == 0) return;

    if ((b->used + len + 1) > b->size)
        buffer_expand(b, b->used + len + 2);

    b->ptr[b->used ++] = 2; /* string */
    b->ptr[b->used ++] = len >> 16;
    b->ptr[b->used ++] = len & 0xff;

    memcpy(b->ptr + b->used, s, len);
    b->used += len;
}

void
append_script_var_double(buffer *b, double src)
{
    union { unsigned char dc[8]; double dd;} d;
    char *dst;

    if (b == NULL || b->ptr == NULL) return;

    if ((b->used + 9) > b->size)
        buffer_expand(b, b->used + 10);

    b->ptr[b->used ++] = '\0'; /* double value */
    dst = b->ptr + b->used;
    d.dd = src;

    dst[0] = d.dc[7];
    dst[1] = d.dc[6];
    dst[2] = d.dc[5];
    dst[3] = d.dc[4];
    dst[4] = d.dc[3];
    dst[5] = d.dc[2];
    dst[6] = d.dc[1];
    dst[7] = d.dc[0];

    b->used += 8;
}

void
append_script_var_bool(buffer *b, int value)
{
    if (b == NULL || b->ptr == NULL) return;

    if ((b->used + 2) > b->size)
        buffer_expand(b, b->used + 3);

    b->ptr[b->used ++] = '\1'; /* bool value */
    b->ptr[b->used ++] = value & 0x1;
}

void
append_script_var_emca_array(buffer *b, char *name, int variable_count)
{
    int len;
    char *dst;

    if (b == NULL || b->ptr == NULL || name == NULL || name[0] == '\0') return;

    len = strlen(name);
    if ((b->used + len + 6) > b->size)
        buffer_expand(b, b->used + len + 7);

    append_script_datastr(b, name,len);
    b->ptr[b->used ++] = 0x8; /* ECMA Array */
    dst = b->ptr + b->used;

    dst[0] = (variable_count >> 24) & 0xff;
    dst[1] = (variable_count >> 16) & 0xff;
    dst[2] = (variable_count >> 8) & 0xff;
    dst[3] = variable_count  & 0xff;

    b->used += 4;
}

void
append_script_dataobject(buffer *b, char *name, int variable_count)
{
    int len;

    if (b == NULL || b->ptr == NULL || name == NULL || name[0] == '\0') return;

    len = strlen(name);
    if ((b->used + len + 6) > b->size)
        buffer_expand(b, b->used + len + 7);

    b->ptr[b->used ++] = 0x2; /* Script Data Object */

    append_script_var_emca_array(b, name, variable_count);
}

void
append_script_emca_array_end(buffer *b)
{
    char *dst;
    if (b == NULL || b->ptr == NULL) return;

    if ((b->used + 3) > b->size)
        buffer_expand(b, b->used + 4);

    dst = b->ptr + b->used;
    dst[0] = dst[1] = 0x0;
    dst[2] = 0x9;

    b->used += 3;
}

/* return -1 if failed
 * return 0 if there has no data to read at this time
 * return > 0 if success
 */
int
read_buffer(int fd, buffer *b)
{
    int toread = 0, r;

    if (fd <= 0 || b == NULL) return -1;

    if (ioctl(fd, FIONREAD, &toread)) {
        if (errno != EAGAIN && errno != EINTR) return -1;
        else return 0;
    }

    /* try to detect whether it's eof */
    if (toread == 0) toread = 1;

    if (toread >= (b->size - b->used))
        buffer_expand(b, toread + b->used); /* expand receive buffer */

    if (toread > (b->size - b->used))
        toread = b->size - b->used;

    r = read(fd, b->ptr + b->used, toread);
    if ((r == -1 && (errno != EINTR && errno != EAGAIN)) || r == 0)
        /* connection closed or reset */
        return -1;

    if (r > 0) {
        b->used += r;
        return r;
    } else {
        return 0;
    }
}

void
set_nonblock(int fd)
{
    int flags = 1;
    struct linger ling = {0, 0};

    if (fd > 0) {
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
        setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)|O_NONBLOCK);
    }
}

void
free_segment(struct segment *seg)
{
    if (seg == NULL) return;
    buffer_free(seg->memory);
    free(seg->map);
    free(seg->keymap);
    free(seg);
}

struct segment *
init_segment(int bitrate, int live_buffer_time)
{
    struct segment *s;

    s = (struct segment *)calloc(sizeof(struct segment), 1);

    if (bitrate < 50) bitrate = 50;

    if (s) {
        s->memory = buffer_init(live_buffer_time * bitrate * 1024 / 8);
        if (bitrate < 64) {
            s->map_size = live_buffer_time * 2 + 64; /* 2 fps */
            s->keymap_size = live_buffer_time / 3 + 16; /* 1 keyframe per 3 seconds */
        } else {
            s->map_size = live_buffer_time * 15 + 1024; /* 15 fps */
            s->keymap_size = live_buffer_time / 6 + 128; /* 1 keyframe per 6 seconds */
        }

        s->map = (struct stream_map *)calloc(sizeof(struct stream_map), s->map_size);
        s->keymap = (struct stream_map *)calloc(sizeof(struct stream_map), s->keymap_size);

        if (s->memory == NULL || s->map == NULL || s->keymap == NULL) {
            buffer_free(s->memory);
            free(s->map);
            free(s->keymap);
            free(s);
            s = NULL;
        } else {
            s->map_used = s->keymap_used = 0;
            s->first_ts = s->last_ts = 0;
        }
    }

    return s;
}

/* return -1 if failed
 * return 0 if there has no data to read at this time
 * return > 0 if success
 */
int
read_dvb_buffer(int fd, buffer *b)
{
    int toread = 0, r;

    if (fd <= 0 || b == NULL) return -1;

    if (ioctl(fd, FIONREAD, &toread)) {
        if (errno != EAGAIN && errno != EINTR) return -1;
        else return 0;
    }

    /* try to detect whether it's eof */
    if (toread == 0) toread = 1;

    if (toread > (b->size - b->used))
        toread = b->size - b->used;

    r = read(fd, b->ptr + b->used, toread);
    if ((r == -1 && (errno != EINTR && errno != EAGAIN)) || r == 0)
        /* connection closed or reset */
        return -1;

    if (r > 0) {
        b->used += r;
        return r;
    } else {
        return 0;
    }
}

int
daemonize(int nochdir, int noclose)
{
    int fd;
    
    switch (fork()) {
    case -1:
        return (-1);
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }
    
    if (setsid() == -1)
        return (-1);
    
    if (nochdir == 0) {
        if (chdir("/") != 0) {
            perror("chdir");
            return (-1);
        }
    }
    
    if (noclose == 0 && (fd = open("/dev/null", O_RDWR, 0)) != -1) {
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2 stdin");
            return (-1);
        }
        
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 stdout");
            return (-1);
        }
        
        if (dup2(fd, STDERR_FILENO) < 0) {
            perror("dup2 stderr");
            return (-1);
        }
        
        if (fd > STDERR_FILENO) {
            if (close(fd) < 0) {
                perror("close");
                return (-1);
            }
        }
    }
    
    return (0);
}


/*
 * programid=XXX&streamid=XXX&userid=XXX[&xxx=XXX]
 *
 */
char *
query_str_get(char * src, const int src_len, char * key, 
        char * value_buf, const int buf_len)
{
    if(NULL == src || NULL == key || NULL == value_buf || 0 == src_len ||
            src_len <= strlen(key) + 1 || buf_len <= 1)
        return NULL;
    
    const int key_len = strlen(key);
    char key_expand[key_len + 2];
    memcpy(key_expand, key, key_len);
    *(key_expand + key_len) = '=';
    *(key_expand + key_len + 1) = '\0';
    int key_expand_len = key_len + 1;

    int m_start_pos = memstr(src, key_expand, src_len, key_len + 1);
    int v_start_pos = m_start_pos + key_expand_len;
    if(-1 == m_start_pos || v_start_pos >= src_len || '&' == src[v_start_pos])
        return NULL;

    int v_end_pos = memstr(src + v_start_pos, "&", src_len - v_start_pos, 1);
    if(-1 == v_end_pos)
        v_end_pos = src_len - v_start_pos;

    if(v_end_pos >= buf_len -1)
        return NULL;

    memcpy(value_buf, src + v_start_pos, v_end_pos);
    value_buf[v_end_pos] = '\0';
    return value_buf;
}

/*
 * GET/POST PATH?QUERY_STR HTTP/1.X
 *
 * src_len not include "\r\n"
 *
 * return -1: error
 * return 0: contain method, path
 * return 1: contain method, path, query_str
 */

int
http_parse_req_line(char * src, const int src_len, 
        char * method, const int method_len,
        char * path, const int path_len,
        char * query_str, const int query_str_len)
{
    if(NULL == src || NULL == method || NULL == path || NULL == query_str)
    {
        return -1;
    }
    int sp0 = memstr(src, " ", src_len, 1);
    if(sp0 <= 0 || sp0 >= method_len)
    {
        return -1;
    }

    int sp1 = memstr(src + sp0 + 1, " ", src_len - sp0 - 1, strlen(" "));
    if(sp1 <= 0)
    {
        return -1;
    }

    sp1 += sp0 + 1;

    memcpy(method, src, sp0);
    method[sp0] = '\0';

    int qu = memstr(src + sp0 + 1, "?", sp1 - sp0 - 1, strlen("?"));
    if(qu < 0)
    {
        if(sp1 - sp0 - 1 >= path_len)
        {
            return -1;
        }
        memcpy(path, src + sp0 + 1, sp1 - sp0 - 1);
        path[sp1 - sp0 - 1] = '\0';
        return 0;
    }
    else
    {
        if(0 == qu || qu >= path_len || (sp1 - sp0 -1) - qu - 1 >= query_str_len ||
                (sp1 - sp0 -1) -qu-1 <= 0)
        {
            return -1;
        }
        memcpy(path, src + sp0 + 1, qu);
        path[qu] = '\0';
        memcpy(query_str, src + sp0 + 1 + qu + 1, (sp1 - sp0 -1) - qu -1);
        query_str[(sp1 - sp0 -1) - qu -1] = '\0';
        return 1;
    }
}

uint64_t
get_curr_tick()
{
    struct timeval v;
    gettimeofday(&v, NULL);
    return v.tv_sec * 1000l + v.tv_usec / 1000l;
}
