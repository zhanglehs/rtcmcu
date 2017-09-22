/* WSG TIME MILSEC LEN URL
 * 4B  4B   4B     4B  + '\0'
 */
/* ffmpeg FLV stream receiver for Youku.com
 *
 * <config>
    <server-expire-second>15</server-expire-second>
    <receiver port='xx' host='127.x.x.x' allow-ip='ip1 ip2' />
    <http port='xx' host='127.x.x.x' seek-to-keyframe='x' />
    <live-buffer-seconds>xxx</live-buffer-seconds>
    <logfile>xxx</logfile>
    <offset streamid='1' video='xxx' audio='xxx' />
 * </config>
 *
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <getopt.h>
#include <event.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <sys/uio.h>

#include <sys/ioctl.h>
#include <signal.h>

#include "xml.h"
#include "buffer.h"

#define MAX(a,b) (a>b?a:b)
#define VERSION "1.0.0"
const int32_t ts_interval = 1000/30;
const int32_t default_bps = 300;

struct live_offset
{
    int streamid;
    int bps;
    int no_audio;
    int audio_offset_ts;
    int video_offset_ts;
};

struct recv_conf
{
    FILE *logfp;

    int recv_port;
    char *recv_host;
    char *recv_allow_ip;

    int http_port;
    char *http_host;

    int verbose_mode;

    int seek_to_keyframe;

    int live_buffer_seconds;

    struct live_offset *offsets;
    int offset_size;

    int expire_ts;

    int admin_port;
};

struct uploader_session;

typedef struct http_client
{
    int fd, state;
    struct event ev;

    struct uploader_session *s;

    struct http_client *next;

    int http_status;

    buffer *r, *w;
    int wpos;

    struct segment *live;
    uint32_t live_start, live_end, live_idx;

    int streamid;
    uint32_t bps;
    int wait;
}http_client;

typedef struct uploader_session
{
    int is_used;

    buffer *onmetadata;
    buffer *avc0;
    buffer *aac0;

    int width, height, framerate;

    uint64_t start_ts;

    struct segment *head, *tail;

    buffer *audio, *video;
    int32_t audio_ts, video_ts;

    struct http_client *clients;

    int streamid;
    uint32_t bps;

    uint32_t audio_offset_ts, video_offset_ts;
    uint32_t delta_ts;
   
    int32_t last_audio_newts, last_video_newts;
    int no_audio;

    time_t expire_ts;
}uploader_session;

typedef struct uploader_list
{
    uploader_session *s;
    int size;
}uploader_list;

typedef enum
{
    net_r_start = 0,
    net_r_flv_header,
    net_r_flv_tag,
    net_r_flv_data,
}read_state_t;

typedef struct net_session
{
    int fd, tagsize;
    struct event ev;
    buffer *r, *w;
    read_state_t r_state;
    int is_shutdown_w;
    uploader_session * uploader;
}net_session;

struct recv_conf conf;

static int receiver_fd = 0, http_fd = 0, udp_fd = 0;

time_t cur_ts;
char cur_ts_str[128];

static struct event ev_receiver, ev_http, ev_receiver_timer, ev_udp;

static struct uploader_list uploaders;

static void ffmpeg_http_handler(const int , const short , void *);
static void enable_http_client_write(struct http_client *);
static void stream_http_client(struct http_client *);
static void http_conn_close(struct http_client *);

static const char resivion[] __attribute__((used)) = { "$Id: receiver.c 509 2011-02-01 06:52:02Z qhy $" };

static void
show_receiver_help(void)
{
    fprintf(stderr, "Youku Flash Live Receiver v%s, Build-Date: " __DATE__ " " __TIME__ "\n"
          "Usage:\n"
          " -h this message\n"
          " -c file, config file, default is /etc/flash/receiver.xml\n"
          " -D don't go to background\n"
          " -M num, maxium ffmpeg receiv slot, default is 1000\n"
          " -v verbose\n\n", VERSION);
}

static int
parse_receiverconf(char *file)
{
    struct xmlnode *mainnode, *config, *p;
    char *q;
    int i, j;

    conf.recv_port = 10000;
    conf.recv_host = NULL;
    conf.recv_allow_ip = NULL;

    conf.http_port = 8888;
    conf.http_host = NULL;

    conf.seek_to_keyframe = 1;
    conf.logfp = stdout;

    conf.live_buffer_seconds = 60;
    conf.expire_ts = 15;
    conf.admin_port = 5000;

    if (file == NULL || file[0] == '\0') return 0;

    mainnode = xmlloadfile(file);
    if (mainnode == NULL) return 1;

    config = xmlgetchild(mainnode, "config", 0);

    if (config) {
        /* <receiver port='81' host='127.x.x.x' allow-ip='ip1 ip2' /> */
        p = xmlgetchild(config, "receiver", 0);
        if (p) {
            q = xmlgetattr(p, "port");
            if (q) {
                conf.recv_port = atoi(q);
                if (conf.recv_port <= 0) conf.recv_port = 81;
            }

            q = xmlgetattr(p, "host");
            if (q) conf.recv_host = strdup(q);

            q = xmlgetattr(p, "allow-ip");
            if (q) conf.recv_allow_ip = strdup(q);
        }

        /* <http port='81' host='127.x.x.x' /> */
        p = xmlgetchild(config, "http", 0);
        if (p) {
            q = xmlgetattr(p, "port");
            if (q) {
                conf.http_port = atoi(q);
                if (conf.http_port <= 0) conf.http_port = 8888;
            }

            q = xmlgetattr(p, "host");
            if (q)
                conf.http_host = strdup(q);

            q = xmlgetattr(p, "seek-to-keyframe");
            if (q) {
                conf.seek_to_keyframe = atoi(q);
                if (conf.seek_to_keyframe < 0) conf.seek_to_keyframe = 0;
            }
        }

        p = xmlgetchild(config, "server-expire-second", 0);
        if (p && p->value) {
            conf.expire_ts = atoi(p->value);
            if (conf.expire_ts <= 0) conf.expire_ts = 15;
        }

        p = xmlgetchild(config, "live-buffer-seconds", 0);
        if (p && p->value) {
            conf.live_buffer_seconds = atoi(p->value);
            if (conf.live_buffer_seconds <= 0) conf.live_buffer_seconds = 600;
        }

        p = xmlgetchild(config, "logfile", 0);
        if (p && p->value) {
            conf.logfp = fopen(p->value, "ab");
            if (conf.logfp == NULL) conf.logfp = stderr;
        }

        p = xmlgetchild(config, "admin-port", 0);
        if (p && p->value) {
            conf.admin_port = atoi(p->value);
            if (conf.admin_port <= 0) conf.admin_port = 5000;
        }

        j = xmlnchild(config, "offset");
        if (j > 0) {
            conf.offsets = (struct live_offset *) calloc(sizeof(struct live_offset), j);
            if (conf.offsets) {
                conf.offset_size = j;
                for (i = 0; i < j; i ++) {
                    /* <offset streamid='1' video='xxx' audio='xxx' /> */
                    p = xmlgetchild(config, "offset", i);

                    q = xmlgetattr(p, "streamid");
                    if (q) conf.offsets[i].streamid = atoi(q);
                    else conf.offsets[i].streamid = 0;

                    q = xmlgetattr(p, "video");
                    if (q) conf.offsets[i].video_offset_ts = atoi(q);
                    else conf.offsets[i].video_offset_ts = 0;

                    q = xmlgetattr(p, "audio");
                    if (q) conf.offsets[i].audio_offset_ts = atoi(q);
                    else conf.offsets[i].audio_offset_ts = 0;

                    q = xmlgetattr(p, "bps");
                    if (q) conf.offsets[i].bps = atoi(q);
                    else conf.offsets[i].bps = 0;

                    q = xmlgetattr(p, "no-audio");
                    if (q) conf.offsets[i].no_audio = atoi(q);
                    else conf.offsets[i].no_audio = 0;
                }
            } else {
                conf.offset_size = 0;
            }

        } else {
            conf.offset_size = 0;
            conf.offsets = NULL;
        }
    }

    freexml(mainnode);

    return 0;
}

static void
net_session_close(net_session * n)
{
    if(NULL == n) return;

    if(n->fd > 0){
        if(conf.verbose_mode)
            fprintf(conf.logfp, "%s: (%s.%d) fd%d uploader net_session closed\n",
                    cur_ts_str, __FILE__, __LINE__, n->fd);
        event_del(&n->ev);
        shutdown(n->fd, SHUT_RDWR);
        close(n->fd);
        n->fd = 0;
    }

    free(n);
}

static void
uploader_reset(uploader_session *u)
{
    struct http_client *c, *cn;

    if (u == NULL) return;

    /* close all http clients attached to this ffmpeg */
    c = u->clients;
    while(c) {
        cn = c->next;
        http_conn_close(c);
        c = cn;
    }

    buffer_free(u->onmetadata);
    buffer_free(u->avc0);
    buffer_free(u->aac0);

    buffer_free(u->audio);
    buffer_free(u->video);

    u->onmetadata = u->avc0 = u->aac0 = u->audio = u->video = NULL;

    if (u->head == u->tail)
        u->tail = NULL;
    free_segment(u->head);
    free_segment(u->tail);

	u->width = u->height = u->framerate = 0;
	u->head = u->tail = NULL;
	u->clients = NULL;
	u->video_offset_ts = u->audio_offset_ts = 0;
	u->audio_ts = u->video_ts = -1;
}

static void
uploader_init(uploader_session * u, int sid)
{
    u->is_used = 0;
    u->head = u->tail = NULL;
    u->onmetadata = u->aac0 = u->avc0 = u->audio = u->video = NULL;
    u->width = u->height = u->framerate = 0;
    u->clients = NULL;
    u->video_offset_ts = u->audio_offset_ts = 0;
    u->audio_ts = u->video_ts = -1;
    u->no_audio = 0;
    u->delta_ts = 0;
    u->last_audio_newts = u->last_video_newts = -1;
    u->bps = default_bps;
    u->streamid = sid;
    u->start_ts = 0;
}

static void
process_clients(uploader_session *u)
{
    struct http_client *c;

    if (u == NULL || u->clients == NULL) return;

    c = u->clients;
    while (c) {
        if (c->wait) {
            enable_http_client_write(c);
            stream_http_client(c);
        }
        c = c->next;
    }
}

static void
generate_uploader_onMetaData(net_session *n)
{
    buffer *b;
    int len = 2;
    FLVTag tag;
    char *dst;

    if (n == NULL || n->uploader == NULL) return;

    uploader_session * u = n->uploader;
    b = buffer_init(512);
    if (b == NULL) return;

    if (u->width > 0) len ++;
    if (u->height > 0) len ++;
    if (u->framerate > 0) len ++;
    if (u->no_audio == 0) len ++;

    memset(&tag, 0, sizeof(tag));
    tag.type = FLV_SCRIPTDATAOBJECT;
    b->used = sizeof(tag);

    append_script_dataobject(b, "onMetaData", len); /* variables */
    append_script_datastr(b, "starttime", strlen("starttime"));
    append_script_var_double(b, (double) u->start_ts);
    append_script_datastr(b, "hasVideo", strlen("hasVideo"));
    append_script_var_bool(b, 1);
    if (u->no_audio == 0) {
        append_script_datastr(b, "hasAudio", strlen("hasAudio"));
        append_script_var_bool(b, 1);
    }

    if (u->width > 0) {
        append_script_datastr(b, "width", strlen("width"));
        append_script_var_double(b, (double) u->width);
    }

    if (u->height > 0) {
        append_script_datastr(b, "height", strlen("height"));
        append_script_var_double(b, (double) u->height);
    }

    if (u->framerate > 0) {
        append_script_datastr(b, "framerate", strlen("framerate"));
        append_script_var_double(b, (double) u->framerate);
    }

    append_script_emca_array_end(b);

    len = b->used - sizeof(tag);
    /* update onmetadata tag structure */
    tag.datasize[0] = (len >> 16) & 0xff;
    tag.datasize[1] = (len >> 8) & 0xff;
    tag.datasize[2] = len & 0xff;

    memcpy(b->ptr, &tag, sizeof(tag));

    /* append 4B of PREV_TAGSIZE */
    dst = b->ptr + b->used;
    dst[0] = (b->used >> 24) & 0xff;
    dst[1] = (b->used >> 16) & 0xff;
    dst[2] = (b->used >> 8) & 0xff;
    dst[3] = b->used & 0xff;
    b->used += 4;

    if (u->onmetadata) buffer_free(u->onmetadata);
    u->onmetadata = b;
}

static void
parse_received_uploader_script_data(net_session *n)
{
    int pos;
    uint64_t width = 0, height = 0, framerate = 0;

    if (n == NULL || n->uploader == NULL || n->r == NULL || n->r->ptr == NULL || n->r->used < n->tagsize) return;

    uploader_session * u = n->uploader;
    pos = memstr(n->r->ptr, "onMetaData", n->r->used, strlen("onMetaData"));
    if (pos < 0) return; /* not onMetaData tag */

    /* to find "height" */
    pos = memstr(n->r->ptr, "height", n->r->used, strlen("height"));
    if (pos >= 0) {
        /* we got it */
        pos += strlen("height");
        switch(n->r->ptr[pos]) {
            case 0: /* double */
                height = (uint32_t) get_double((unsigned char *)(n->r->ptr + pos + 1));
                break;
            default:
                break;
        }
    }

    /* to find "width" */
    pos = memstr(n->r->ptr, "width", n->r->used, strlen("width"));
    if (pos >= 0) {
        /* we got it */
        pos += strlen("width"); /* to the end of 'width' */
        switch(n->r->ptr[pos]) {
            case 0: /* double */
                width = (uint32_t) get_double((unsigned char *)(n->r->ptr + pos + 1));
                break;
            default:
                break;
        }
    }

    /* to find "framerate" */
    pos = memstr(n->r->ptr, "framerate", n->r->used, strlen("framerate"));
    if (pos >= 0) {
        /* we got it */
        pos += strlen("framerate"); /* to the end of 'framerate' */
        switch(n->r->ptr[pos]) {
            case 0: /* double */
                framerate = (uint32_t) get_double((unsigned char *)(n->r->ptr + pos + 1));
                break;
            default:
                break;
        }
    }

    if (height > 0) u->height = height;
    if (width > 0) u->width = width;
    if (framerate > 0) u->framerate = framerate;

    if (conf.verbose_mode)
        fprintf(conf.logfp, "%s: (%s.%d) #%d/#%d ffmpeg stream start_ts = %.0f, width = %d, height = %d, framerate = %d\n",
                       cur_ts_str, __FILE__, __LINE__, u->streamid, u->bps, (double) u->start_ts,
                       u->width, u->height, u->framerate);

    generate_uploader_onMetaData(n);
}

static void
append_video_audio_tag_to_stream(net_session *n, struct buffer *b, uint32_t tagsize,
        int is_video, int is_seekable, uint32_t ts)
{
    uint32_t offset;
    struct segment *seg = NULL;
    struct stream_map *m = NULL;

    if (n == NULL || b == NULL || tagsize == 0 || n->uploader == NULL) return;

    uploader_session *u = n->uploader;
    /* normal audio/video tag, put them into queue */
    if (u->head == NULL || u->tail == NULL) {
        /* first segment */
        seg = u->tail = u->head = init_segment(u->bps, conf.live_buffer_seconds);
    } else {
        if ((is_seekable && ((u->tail->memory->size - u->tail->memory->used) < 256*1024)) ||
            ((u->tail->memory->used + tagsize) > u->tail->memory->size)) {
            /* we want to make sure that new segment has 1 keyframe */
            if (u->head == u->tail) {
                /* only one segment, allocate new one */
                u->head->is_full = 1;
                seg = init_segment(u->bps, conf.live_buffer_seconds);
                if (seg) u->tail = seg;
            } else {
                /* we already has two segments, swap head with tail */
                seg = u->head;
                u->head = u->tail;
                u->tail = seg;

                u->head->is_full = 1;
                /* reset segment data */
                seg->map_used = seg->keymap_used = 0;
                seg->first_ts = seg->last_ts = 0;
                seg->memory->used = 0;
                seg->is_full = 0;
                if (conf.verbose_mode)
                    fprintf(stderr, "%s: (%s.%d) %s SWAP SEG 1# with SEG #2 for #%d/#%d\n",
                            cur_ts_str, __FILE__, __LINE__,
                            is_seekable?"SEEKABLE":"NORMAL", u->streamid, u->bps);
            }
        } else {
            seg = u->tail;
        }
    }

    if (seg == NULL) {
        fprintf(conf.logfp, "%s: (%s.%d) out of memory for new segment\n",
                cur_ts_str, __FILE__, __LINE__);
        return;
    }

    if (seg->map_used >= seg->map_size) {
        char *pp;
#define SEGMENT_STEP 2048
        pp = realloc(seg->map, sizeof(struct stream_map)*(seg->map_size + SEGMENT_STEP));
        if (pp) {
            seg->map = (struct stream_map *)pp;
            seg->map_size += SEGMENT_STEP;
        } else {
            fprintf(conf.logfp, "%s: (%s.%d) out of memory for segment's maps\n",
                    cur_ts_str, __FILE__, __LINE__);
            return;
        }
#undef SEGMENT_STEP
    }

    if (seg->keymap_used >= seg->keymap_size) {
        char *pp;
#define SEGMENT_STEP 32
        pp = realloc(seg->keymap, sizeof(struct stream_map)*(seg->keymap_size + SEGMENT_STEP));
        if (pp) {
            seg->keymap = (struct stream_map *)pp;
            seg->keymap_size += SEGMENT_STEP;
        } else {
            fprintf(conf.logfp, "%s: (%s.%d) out of memory for segment's keymaps\n",
                    cur_ts_str, __FILE__, __LINE__);
            return;
        }
#undef SEGMENT_STEP
    }

    offset = seg->memory->used;
    /* append buffer content*/
    memcpy(seg->memory->ptr + seg->memory->used, b->ptr, tagsize);
    seg->memory->used += tagsize;

    if (is_video) {
        /* we only save segment of video tag
         * to update video segment's maps and keymaps
         */
        m = seg->map + seg->map_used;
        m->ts = ts;
        m->start = offset;
        m->end = seg->memory->used; /* including prev_tagsize */
        ++ seg->map_used;

        /* update first_ts & last_ts */
        if (seg->first_ts == 0) seg->first_ts = ts;
        seg->last_ts = ts;

        /* update keyframe map */
        if (is_seekable) {
            m = seg->keymap + seg->keymap_used;
            m->ts = ts;
            m->start = offset;
            m->end = seg->memory->used; /* including prev_tagsize */
            ++ seg->keymap_used;
        }
        /* to process waiting job queue when VIDEO frame arrived */
        process_clients(u);
    }
}

static void
merge_uploader_video_with_audio(net_session *n)
{
    FLVTag *tag;
    uint32_t tagsize, is_seekable, ts, is_video;
    buffer *b = NULL;
    uint32_t len = sizeof(FLVTag);
    uploader_session * u = n->uploader;

    if (n == NULL || u->audio == NULL || u->video == NULL) return;

    while (u->audio->used > 0 || u->video->used > 0) {
        b = NULL;
        tag = NULL;
        tagsize = is_seekable = ts = is_video = 0;

        /* update s->audio_ts & s->video_ts */
        if (u->audio->used > 0) {
            tag = (FLVTag *) u->audio->ptr;
            u->audio_ts = (tag->timestamp_ex << 24) + (tag->timestamp[0] << 16) +
                (tag->timestamp[1] << 8) + tag->timestamp[2];
        }

        if (u->video->used > 0) {
            tag = (FLVTag *) u->video->ptr;
            u->video_ts = (tag->timestamp_ex << 24) + (tag->timestamp[0] << 16) +
                (tag->timestamp[1] << 8) + tag->timestamp[2];
        }

        if (u->no_audio == 0) {
            /* find the smaller tag's timestamp */
            if (u->video_ts >= 0 && u->video_ts <= u->audio_ts && u->video->used > 0) {
                /* video's timestamp is smaller than audio's timestamp */
                tag = (FLVTag *) u->video->ptr;
                tagsize = (tag->datasize[0] << 16) + (tag->datasize[1] << 8) +
                        tag->datasize[2] + sizeof(FLVTag) + 4;
                ts = u->video_ts;

                if (u->video->used >= tagsize) {
                    b = u->video;
                    is_video = 1;
                    if (tag->type == FLV_VIDEODATA && (((b->ptr[len] & 0xf0) == 0x10) || ((b->ptr[len] & 0xf0) == 0x40)))
                        is_seekable = 1;
                }
            } else if (u->audio_ts >= 0 && u->audio_ts < u->video_ts && u->audio->used > 0) {
                /* audio's timestamp is smaller than video's timestamp */
                tag = (FLVTag *) u->audio->ptr;
                tagsize = (tag->datasize[0] << 16) + (tag->datasize[1] << 8) +
                        tag->datasize[2] + sizeof(FLVTag) + 4;
                ts = u->audio_ts;

                if (u->audio->used >= tagsize) b = u->audio;
            }
        } else {
            /* video only */
            if (u->video_ts >= 0 && u->video->used > 0) {
                /* video's timestamp is smaller than audio's timestamp */
                tag = (FLVTag *) u->video->ptr;
                tagsize = (tag->datasize[0] << 16) + (tag->datasize[1] << 8) +
                        tag->datasize[2] + sizeof(FLVTag) + 4;
                ts = u->video_ts;

                if (u->video->used >= tagsize) {
                    b = u->video;
                    is_video = 1;
                    if (tag->type == FLV_VIDEODATA && (((b->ptr[len] & 0xf0) == 0x10) || ((b->ptr[len] & 0xf0) == 0x40)))
                        is_seekable = 1;
                }
            }
        }

        if (b != NULL && tagsize > 0) {
            /* append video/audio tag to stream */
            append_video_audio_tag_to_stream(n, b, tagsize, is_video, is_seekable, ts);
            if (b->used > tagsize) {
                memmove(b->ptr, b->ptr + tagsize, b->used - tagsize);
                b->used -= tagsize;
            } else {
                b->used = 0;
            }
        } else {
            break; /* exit loop */
        }
    }
}

static int32_t
adjust_tag_ts(net_session * n)
{
    FLVTag * tag = (FLVTag *)n->r->ptr;
    int32_t ts = flv_get_timestamp(tag->timestamp, tag->timestamp_ex);
    int32_t ret_ts = ts;

    struct uploader_session * u = n->uploader;
    if(tag->type == FLV_AUDIODATA){
        //audio
        if(-1 == u->last_audio_newts){
            u->last_audio_newts = ts;
            u->delta_ts = 0;
            return ts;
        }

        if(ts + u->delta_ts < u->last_audio_newts)
            u->delta_ts += u->last_audio_newts - (ts + u->delta_ts) + ts_interval;

        u->last_audio_newts = ts + u->delta_ts;
        ret_ts = u->last_audio_newts;
    }
    else{
        //video
        if(-1 == u->last_video_newts){
            u->last_video_newts = ts;
            u->delta_ts = 0;
            return ts;
        }

        if(ts + u->delta_ts < u->last_video_newts)
            u->delta_ts += u->last_video_newts - (ts + u->delta_ts) + ts_interval;

        u->last_video_newts = ts + u->delta_ts;
        ret_ts = u->last_video_newts;
    }

    flv_set_timestamp(tag->timestamp, &tag->timestamp_ex, ret_ts);
    return ret_ts;
}

static void
append_received_uploader_stream(net_session *n)
{
    FLVTag *tag;
    buffer *b, *b2;
    int len, is_aac = 0, is_avc = 0, is_video = 0, is_seekable = 0, tagsize = 0;
    uint32_t ts = 0;

    if (n == NULL || n->uploader == NULL || n->r == NULL || n->r->ptr == NULL || n->r->used < n->tagsize) return;

    uploader_session * u = n->uploader;
    b = n->r;
    tag = (FLVTag *)b->ptr;

    if (tag->type != FLV_AUDIODATA && tag->type != FLV_VIDEODATA)
        return;

    tagsize = (tag->datasize[0] << 16) + (tag->datasize[1] << 8) + tag->datasize[2] + sizeof(FLVTag) + 4;


    /* check specific audio/video tag */
    len = sizeof(FLVTag);
    if (tag->type == FLV_AUDIODATA && ((b->ptr[len] & 0xf0) == 0xA0) && (b->ptr[len+1] == 0)) {
        is_aac = 1;
    } else if (tag->type == FLV_VIDEODATA && ((b->ptr[len] & 0xf) == 0x7) && (b->ptr[len+1] == 0)) {
        is_avc = 1;
    }

    fprintf(conf.logfp, "%lu:ts = %d, type = %d, is_aac = %d, is_avc = %d\n", 
            get_curr_tick(), 
            flv_get_timestamp(tag->timestamp, tag->timestamp_ex),
            (int)tag->type, is_aac, is_avc);
    ts = adjust_tag_ts(n);
    if (is_aac == 1) {
        /* AAC Audio config data */
        b2 = buffer_init(n->tagsize + 1);
        if (b2) {
            memcpy(b2->ptr, b->ptr, n->tagsize);
            b2->used = n->tagsize;
            if (u->aac0) buffer_free(u->aac0);
            u->aac0 = b2;
            /* reset aac0 tag timestamp */
            b2->ptr[4] = b2->ptr[5] = b2->ptr[6] = b2->ptr[7] = '\0';
        }
    } else if (is_avc == 1) {
        /* AVC Video config data */
        b2 = buffer_init(n->tagsize + 1);
        if (b2) {
            memcpy(b2->ptr, b->ptr, n->tagsize);
            b2->used = n->tagsize;
            if (u->avc0) buffer_free(u->avc0);
            u->avc0 = b2;
            /* reset avc0 tag timestamp */
            b2->ptr[4] = b2->ptr[5] = b2->ptr[6] = b2->ptr[7] = '\0';
        }
    }

    if (tag->type == FLV_VIDEODATA) {
        is_video = 1;
        if (tag->type == FLV_VIDEODATA && (((b->ptr[len] & 0xf0) == 0x10) || ((b->ptr[len] & 0xf0) == 0x40)))
            is_seekable = 1;
    }

    /* append video/audio tag to stream */
    append_video_audio_tag_to_stream(n, b, tagsize, is_video, is_seekable, ts);
}

static void
append_received_uploader_stream_with_offset(net_session *s)
{
    FLVTag *tag;
    buffer *b, *b2;
    int len, is_aac = 0, is_avc = 0;
    uint32_t ts = 0, new_ts = 0;

    if (s == NULL || s->r == NULL || s->r->ptr == NULL || s->r->used < s->tagsize || s->uploader == NULL) return;

    uploader_session * u = s->uploader;
    b = s->r;
    tag = (FLVTag *)b->ptr;

    if (tag->type != FLV_AUDIODATA && tag->type != FLV_VIDEODATA)
        return;

    ts = adjust_tag_ts(s);

    /* check specific audio/video tag */
    len = sizeof(FLVTag);
    if (tag->type == FLV_AUDIODATA && ((b->ptr[len] & 0xf0) == 0xA0) && (b->ptr[len+1] == 0)) {
        is_aac = 1;
    } else if (tag->type == FLV_VIDEODATA && ((b->ptr[len] & 0xf) == 0x7) && (b->ptr[len+1] == 0)) {
        is_avc = 1;
    }

    if (is_aac == 0 && u->audio_offset_ts > 0 && tag->type == FLV_AUDIODATA)
        new_ts = ts + u->audio_offset_ts;

    if (is_avc == 0 && u->video_offset_ts > 0 && tag->type == FLV_VIDEODATA)
        new_ts = ts + u->video_offset_ts;

    if (is_aac == 1) {
        /* AAC Audio config data */
        b2 = buffer_init(s->tagsize + 1);
        if (b2) {
            memcpy(b2->ptr, b->ptr, s->tagsize);
            b2->used = s->tagsize;
            if (u->aac0) buffer_free(u->aac0);
            u->aac0 = b2;
            /* reset aac0 tag timestamp */
            b2->ptr[4] = b2->ptr[5] = b2->ptr[6] = b2->ptr[7] = '\0';
        }
    } else if (is_avc == 1) {
        /* AVC Video config data */
        b2 = buffer_init(s->tagsize + 1);
        if (b2) {
            memcpy(b2->ptr, b->ptr, s->tagsize);
            b2->used = s->tagsize;
            if (u->avc0) buffer_free(u->avc0);
            u->avc0 = b2;
            /* reset avc0 tag timestamp */
            b2->ptr[4] = b2->ptr[5] = b2->ptr[6] = b2->ptr[7] = '\0';
        }
    }

    if (new_ts > 0) {
        tag->timestamp_ex = (new_ts >> 24) & 0xff;
        tag->timestamp[0] = (new_ts >> 16) & 0xff;
        tag->timestamp[1] = (new_ts >> 8) & 0xff;
        tag->timestamp[2] = new_ts & 0xff;
        ts = new_ts;
    }

    if (tag->type == FLV_AUDIODATA) {
        if (u->audio == NULL) {
            u->audio = buffer_init(s->tagsize + 10);
            if (u->audio) {
                memcpy(u->audio->ptr, b->ptr, s->tagsize);
                u->audio->used = s->tagsize;
            } else {
                fprintf(conf.logfp, "%s: (%s.%d) out of memory for audio buffer of streamid/bps #%d/#%d\n",
                        cur_ts_str, __FILE__, __LINE__, u->streamid, u->bps);
            }
        } else {
            /* append to s->audio */
            if ((u->audio->used + s->tagsize) > u->audio->size) {
                buffer_expand(u->audio, u->audio->used + s->tagsize + 10);
            }

            if ((u->audio->used + s->tagsize) <= u->audio->size) {
                memcpy(u->audio->ptr + u->audio->used, b->ptr, s->tagsize);
                u->audio->used += s->tagsize;
            } else {
                fprintf(conf.logfp, "%s: (%s.%d) out of memory for audio buffer of streamid/bps #%d/#%d\n",
                        cur_ts_str, __FILE__, __LINE__, u->streamid, u->bps);
            }
        }
    } else if (tag->type == FLV_VIDEODATA) {
        if (u->video == NULL) {
            /* init s->video */
            u->video = buffer_init(s->tagsize + 10);
            if (u->video) {
                /* copy b->ptr to s->video  */
                memcpy(u->video->ptr, b->ptr, s->tagsize);
                u->video->used = s->tagsize;
            } else {
                fprintf(conf.logfp, "%s: (%s.%d) out of memory for video buffer of streamid/bps #%d/#%d\n",
                        cur_ts_str, __FILE__, __LINE__, u->streamid, u->bps);
            }
        } else {
            /* append to s->video */
            if ((u->video->used + s->tagsize) > u->video->size) {
                buffer_expand(u->video, u->video->used + s->tagsize + 10);
            }

            if ((u->video->used + s->tagsize) <= u->video->size) {
                memcpy(u->video->ptr + u->video->used, b->ptr, s->tagsize);
                u->video->used += s->tagsize;
            } else {
                fprintf(conf.logfp, "%s: (%s.%d) out of memory for video buffer of streamid/bps #%d/#%d\n",
                        cur_ts_str, __FILE__, __LINE__, u->streamid, u->bps);
            }
        }
    }
    merge_uploader_video_with_audio(s);
}

static void
parse_received_uploader_stream(net_session *n)
{
    int to_continue = 1, pos = 0;
    FLVTag *flvtag;

    if (n == NULL||n->uploader == NULL) return;

    uploader_session * u = n->uploader;

    while(to_continue) {
        if (n->r_state == net_r_flv_header) {
            to_continue = 0;
            fprintf(conf.logfp, "used = %d, pos = %d\n", n->r->used, pos);
            if ((n->r->used - pos) >= (sizeof(FLVHeader) + 4)) { /* 4 is UINT32 of previous tag size */
                if (strncmp(n->r->ptr + pos, "FLV", 3) != 0) {
                    fprintf(conf.logfp, "%s:%d \"FLV\" check failed.\n");
                    net_session_close(n);
                    return;
                }
                fprintf(conf.logfp, "%lu:FLV Header\n", get_curr_tick());
                n->r_state = net_r_flv_tag; /* start of next tag */
                pos += sizeof(FLVHeader) + 4;

                if (n->r->used > pos)
                    to_continue = 1;
            }
        }

        if (to_continue && n->r_state == net_r_flv_tag) {
            fprintf(conf.logfp, "%s:%d net_r_flv_tag\n", __FILE__, __LINE__);
            to_continue = 0;
            if ((n->r->used - pos) >= sizeof(FLVTag)) {
                flvtag = (FLVTag *)(n->r->ptr + pos);

                /* 4 is sizeof(previous tag size) */
                n->tagsize = (flvtag->datasize[0] << 16) + (flvtag->datasize[1] << 8) +
                    flvtag->datasize[2] + sizeof(FLVTag) + 4;

                if (n->tagsize > 512*1024) {
                    fprintf(conf.logfp, "%s:%d tagsize = %d\n", __FILE__, __LINE__, n->tagsize);
                    net_session_close(n);
                    return;
                }

                if (n->tagsize > n->r->size)
                    buffer_expand(n->r, n->tagsize);

                n->r_state = net_r_flv_data;
                if (n->r->used > pos)
                    to_continue = 1;
            }
        }

        if (to_continue && n->r_state == net_r_flv_data) {
            fprintf(conf.logfp, "%s:%d net_r_flv_data\n", __FILE__, __LINE__);
            /* FLVTag + Data */
            to_continue = 0;
            if ((n->r->used - pos) >= n->tagsize) {
                memmove(n->r->ptr, n->r->ptr + pos, n->r->used - pos);
                n->r->used -= pos;
                pos = 0;
                /* full of tag data */
                flvtag = (FLVTag *)n->r->ptr;
                if (flvtag->type == FLV_AUDIODATA || flvtag->type == FLV_VIDEODATA ) {
                    if (u->audio_offset_ts > 0 || u->video_offset_ts > 0)
                        append_received_uploader_stream_with_offset(n);
                    else
                        append_received_uploader_stream(n);
                } else if (flvtag->type == FLV_SCRIPTDATAOBJECT ) {
                    parse_received_uploader_script_data(n);
                }

                /* to read next flv tag */
                n->r_state = net_r_flv_tag;
                if (n->r->used > n->tagsize) {
                    memmove(n->r->ptr, n->r->ptr + n->tagsize, n->r->used - n->tagsize);
                    n->r->used -= n->tagsize;
                    if (n->r->used >= sizeof(FLVTag))
                        to_continue = 1;
                } else {
                    fprintf(conf.logfp, "%s:%d \n", __FILE__, __LINE__);
                    n->r->used = 0;
                }
            }
        } else {
            to_continue = 0;
        }
    }

    if (pos > 0) {
            if (pos < n->r->used) {
            memmove(n->r->ptr, n->r->ptr + pos, n->r->used - pos);
            n->r->used -= pos;
        } else {
            n->r->used = 0;
        }
    }
}

static int
check_integer(const char * src)
{
    if(NULL == src || '\0' == *src)
        return 0;

    for(;'\0' != *src ; src++)
    {
        if(*src > '9' || *src < '0')
            return 0;
    }
    return 1;
}

static void uploader_http_handler(const int, const short, void *);

static void
disable_net_write(net_session * n)
{
    if(n)
    {
        event_del(&n->ev);
        event_set(&n->ev, n->fd, EV_READ|EV_PERSIST, uploader_http_handler, (void *)n);
        event_add(&n->ev, 0);
    }
}

static void
enable_net_write(net_session * n)
{
    if(n)
    {
        event_del(&n->ev);
        event_set(&n->ev, n->fd, EV_READ|EV_WRITE|EV_PERSIST, uploader_http_handler, (void *)n);
        event_add(&n->ev, 0);
    }
}

static int
rsp_uploader_404(net_session * n)
{
    if(conf.verbose_mode)
        fprintf(conf.logfp, "rsp 404\n");
    const char *err404 = "HTTP/1.0 404 Not Found\r\nConnection: close\r\n\r\n";
    if(n->w->size - n->w->used <= strlen(err404))
    {
        buffer_expand(n->w, n->w->used + strlen(err404));
    }

    if(snprintf(n->w->ptr, strlen(err404), err404) <= 0)
        return 0;

    n->w->used += strlen(err404);
    n->is_shutdown_w = 1;
    enable_net_write(n);
    return 1;
}

static void
uploader_http_handler(const int fd, const short which, void *arg)
{
    net_session *c = NULL;
    int r, streamid, userid, key;
    streamid = userid = key = -1;
    char method[32];
    char path[64];
    char query_str[1024];

    c = (net_session *)arg;

    if (c == NULL) return;
    if(conf.verbose_mode)
        fprintf(conf.logfp, "uploader_http_handler entered. which = %hd\n", which);
//  c->expire_ts = cur_ts + conf.expire_ts;
    if (which & EV_READ) {
        if(conf.verbose_mode)
        {
            fprintf(conf.logfp, "uploader_http_handler EV_READ\n");
        }
        r = read_buffer(fd, c->r);
        if (r <= 0) {
            if (r == -1) {
                fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                if(NULL != c->uploader) uploader_reset(c->uploader);
                net_session_close(c);
            }
            return;
        }

        if(c->uploader == NULL){
            if(conf.verbose_mode)
                fprintf(conf.logfp, "uploader = NULL\n");
#define MIN_SZ_REQ_LINE "GET /get_state?programid=X&streamid=X&userid=X&key=X& HTTP/1.X\r\n"
            if (c->r->used <= sizeof(MIN_SZ_REQ_LINE))
                return;
#undef MIN_SZ_REQ_LINE

            if(conf.verbose_mode)
                fprintf(conf.logfp, "MIN_SZ_REQ_LINE pass\n");
            if(conf.verbose_mode)
            {
                int i = 0;
                for(;i < c->r->used; i++)
                {
                    if(c->r->ptr[i] == '\r')
                        fprintf(conf.logfp, "\\r");
                    else if(c->r->ptr[i] == '\n')
                        fprintf(conf.logfp, "\\n");
                    else
                        fprintf(conf.logfp, "%c", c->r->ptr[i]);
                }
                fprintf(conf.logfp, "\n");
            }

            int lrcf2_p = memstr(c->r->ptr, "\r\n\r\n", c->r->used, strlen("\r\n\r\n"));
            if(lrcf2_p <= 0)
            {
                if(conf.verbose_mode)
                    fprintf(conf.logfp, "lrcf2_p failed\n");
                return;
            }

            int lrcf_p = memstr(c->r->ptr, "\r\n", c->r->used, strlen("\r\n"));

            if (1 != http_parse_req_line(c->r->ptr, lrcf_p, 
                        method, sizeof(method), 
                        path, sizeof(path),
                        query_str, sizeof(query_str)))
            {
                if(conf.verbose_mode)
                    fprintf(conf.logfp, "parse_req_line failed\n");
                net_session_close(c);
                return;
            }
            

            if(conf.verbose_mode)
                fprintf(conf.logfp, "method = %s, path = %s, query_str = %s\n", 
                        method, path, query_str);

            if (strlen(method) == 3 && 0 == strncmp(method, "GET", 3))
            {
                //GET /get_state?programid=X&streamid=X&userid=X&key=X&localtimestamp=X HTTP/1.X
                if(conf.verbose_mode)
                    fprintf(conf.logfp, "method = GET\n");

                char s_buf[1024];
                char u_buf[1024];
                char k_buf[1024];

#define RSP  "HTTP/1.0 %s %s\r\nDate: %s\r\nServer: Youku Live Receiver\r\nContent-Type: text/html\r\nContent-Length: %d\r\nExpires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n\r\n%s"

                char date[128];
                strftime(date, sizeof("Fri, 01 Jan 1990 00:00:00 GMT")+1,
			"%a, %d %b %Y %H:%M:%S GMT", gmtime(&cur_ts));
                
                int expected = strlen(RSP) + 128;
                if(strlen(path) != strlen("/v1/get_state") || 
                        0 != strncmp("/v1/get_state", path, strlen("/v1/get_state")))
                {
                    if(!rsp_uploader_404(c))
                    {
                        fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                        net_session_close(c);
                        return;
                    }
                    buffer_ignore(c->r, c->r->size);
                    return;
                }
                if(!query_str_get(query_str, strlen(query_str), "streamid", s_buf, sizeof(s_buf))||
                        !query_str_get(query_str, strlen(query_str), "userid", u_buf, sizeof(u_buf))||
                        !query_str_get(query_str, strlen(query_str), "key", k_buf, sizeof(k_buf)))
                {
                    if(!rsp_uploader_404(c))
                    {
                        fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                        net_session_close(c);
                        return;
                    }
                    buffer_ignore(c->r, c->r->size);
                    return;
                }
                if(!check_integer(s_buf) || !check_integer(u_buf) || !check_integer(k_buf))
                {
                    
                    fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                    net_session_close(c);
                    return;
                } 
            
                streamid = strtol(s_buf, (char **) NULL, 10);
                userid = strtol(u_buf, (char **) NULL, 10);
                if(streamid < 0 || streamid >= uploaders.size)
                {
                    if(!rsp_uploader_404(c))
                    {
                        fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                        net_session_close(c);
                        return;
                    }
                    buffer_ignore(c->r, c->r->size);
                    return;
                }
                //TODO check user
                uploader_session * u = uploaders.s + streamid;
                if(c->w->size - c->w->used < expected)
                    buffer_expand(c->w, expected + c->w->used);

                if(!u->is_used||-1 == u->last_audio_newts||-1 == u->last_video_newts)
                {
                    int used = snprintf(c->w->ptr, c->w->size - c->w->used, RSP, "200", "OK", date, 4, "None");
                    c->w->used += used;
                    enable_net_write(c);
                }
                else
                {
                    //rsp last timestamp
                    char buf_ts[1024];
                    memset(buf_ts, 0, sizeof(buf_ts));

                    snprintf(buf_ts, sizeof(buf_ts) - 1, "%d", MAX(u->last_audio_newts, u->last_video_newts));
                    int used = snprintf(c->w->ptr, c->w->size - c->w->used, RSP, "200", "OK", date, strlen(buf_ts), buf_ts);
                    c->w->used += used;
                    enable_net_write(c);
                }

                //ignore all bytes before message body
                if(lrcf2_p + 4 != buffer_ignore(c->r, lrcf2_p + 4))
                {
                    uploader_reset(u);
                    fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                    net_session_close(c);
                    return;
                }
            }
            else if (strlen(method) == 4 && 0 == strncmp(method, "POST", 4))
            {
                //POST
                if(conf.verbose_mode)
                    fprintf(conf.logfp, "method = POST\n");

                char s_buf[1024];
                char u_buf[1024];
                char k_buf[1024];
                if(strlen(path) != strlen("/v1/post_stream") || 
                        0 != strncmp("/v1/post_stream", path, strlen("/v1/post_stream")))
                {
                    fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                    net_session_close(c);
                    return;
                }
                if(!query_str_get(query_str, strlen(query_str), "streamid", s_buf, sizeof(s_buf))||
                        !query_str_get(query_str, strlen(query_str), "userid", u_buf, sizeof(u_buf))||
                        !query_str_get(query_str, strlen(query_str), "key", k_buf, sizeof(k_buf)))
                {
                    fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                    net_session_close(c);
                    return;
                }
                if(!check_integer(s_buf) || !check_integer(u_buf) || !check_integer(k_buf))
                {
                    fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                    net_session_close(c);
                    return;
                } 

                streamid = strtol(s_buf, (char **) NULL, 10);
                userid = strtol(u_buf, (char **) NULL, 10);
                if(streamid < 0 || streamid >= uploaders.size)
                {
                    fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                    net_session_close(c);
                    return;
                }
                //TODO check user
                uploader_session * u = uploaders.s + streamid;
                c->uploader = u;
                u->is_used = 1;
                u->start_ts = cur_ts;
                u->bps = default_bps;
                c->r_state = net_r_flv_header;
                //ignore all bytes before message body
                buffer_ignore(c->r, lrcf2_p + 4);
                fprintf(conf.logfp, "buffer ignored %d bytes\n", lrcf2_p + 4);
            }
            else
            {
                fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                net_session_close(c);
                return;
            }
        }
        if(c->uploader != NULL){
            switch(c->r_state){
                case net_r_flv_header:
                case net_r_flv_tag:
                case net_r_flv_data:
                    parse_received_uploader_stream(c);
                    break;
                default:
                    uploader_reset(c->uploader);
                    fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                    net_session_close(c);
                    break;
            }
        }
    } 
    else if (which & EV_WRITE) {
        if(conf.verbose_mode)
            fprintf(conf.logfp, "uploader_http_handler EV_WRITE c->w->used = %d\n", c->w->used);
        if(c->w->used > 0)
        {
            int i = write(c->fd, c->w->ptr, c->w->used);
            if(i < 0)
            {
                if(errno != EAGAIN && errno != EINTR)
                {
                    fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
                    net_session_close(c);
                }
                i = 0;
            }
            if(conf.verbose_mode)
                fprintf(conf.logfp, "write:%d\n", i);
            buffer_ignore(c->w, i);
        }
        else
        {
            disable_net_write(c);
        }
        if(c->is_shutdown_w)
        {

            fprintf(conf.logfp, "net_session_closing %s:%d\n", __FILE__, __LINE__);
            shutdown(c->fd, SHUT_WR);
        }
    }
}

static void
uploader_http_accept(const int fd, const short which, void *arg)
{
    net_session *c = NULL;
    int newfd;
    struct sockaddr_in s_in;
    socklen_t len = sizeof(struct sockaddr_in);
    char *ip;

    memset(&s_in, 0, len);
    newfd = accept(fd, (struct sockaddr *)&s_in, &len);
    if (newfd < 0) {
        fprintf(conf.logfp, "%s: (%s.%d) fd %d accept() failed\n", cur_ts_str, __FILE__, __LINE__, fd);
        return;
    }

    if (conf.recv_allow_ip && conf.recv_allow_ip[0] != '\0') {
        ip = inet_ntoa(s_in.sin_addr);
        if (strstr(conf.recv_allow_ip, ip) == NULL) {
            fprintf(conf.logfp, "%s: (%s.%d) fd %d -> ip %s isn't in allow ip list\n",
                    cur_ts_str, __FILE__, __LINE__, newfd, ip);
            close(newfd);
            return;
        }
    }

    c = (net_session *)calloc(1, sizeof(net_session));
    if (c == NULL) {
        fprintf(conf.logfp, "%s: (%s.%d) fd %d -> out of memory\n",
                cur_ts_str, __FILE__, __LINE__, newfd);
        close(newfd);
        return;
    }

    c->r = buffer_init(2048);
    c->w = buffer_init(2048);
    if (c->r == NULL || c->w == NULL) {
        fprintf(conf.logfp, "%s: (%s.%d) fd %d -> out of memory\n",
                cur_ts_str, __FILE__, __LINE__, newfd);
        close(newfd);
        free(c);
        return;
    }

    c->fd = newfd;
    set_nonblock(c->fd);

    c->uploader = NULL;
    c->r_state = net_r_start;
    c->is_shutdown_w = 0;
    event_set(&(c->ev), c->fd, EV_READ|EV_WRITE|EV_PERSIST, uploader_http_handler, (void *) c);
    event_add(&(c->ev), 0);
//  c->expire_ts = cur_ts + conf.expire_ts;
}

static void
disable_http_client_write(struct http_client *c)
{
    if (c) {
        event_del(&c->ev);
        event_set(&c->ev, c->fd, EV_READ|EV_PERSIST, ffmpeg_http_handler, (void *)c);
        event_add(&c->ev, 0);
        c->wait = 1;
    }
}

static void
enable_http_client_write(struct http_client *c)
{
    if (c) {
        event_del(&c->ev);
        event_set(&c->ev, c->fd, EV_READ|EV_WRITE|EV_PERSIST, ffmpeg_http_handler, (void *)c);
        event_add(&c->ev, 0);
        c->wait = 0;
    }
}

static void
http_conn_close(struct http_client *c)
{
    struct http_client *cn, *cp;

    if (c == NULL) return;
    if (c->fd > 0) {
        event_del(&(c->ev));
        close(c->fd);
    }

    if (c->s) {
        cn = cp = c->s->clients;
        while (cn) {
            if (cn == c) break;
            cp = cn;
            cn = cn->next;
        }

        if (cn) {
            /* matched */
            if (cn == cp)
                c->s->clients = cn->next;
            else
                cp->next = cn->next;
        }
    }

    buffer_free(c->r);
    buffer_free(c->w);
    free(c);
}

static void
write_ffmpeg_http_error(struct http_client *c)
{
    char errstr[129];
    int r;

    if (c == NULL || c->http_status == 0) return;

    r = snprintf(errstr, 128, "HTTP/1.0 404 Not Found\r\nConnection: close\r\n\r\n");
    write(c->fd, errstr, r);
    http_conn_close(c);
}

static void
parse_ffmpeg_request(struct http_client *c)
{
    char *p, date[65];
    struct uploader_session *s = NULL;
    int i;

    if (c == NULL) return;

    p = c->r->ptr + 4;
    if (strncmp(c->r->ptr, "GET ", 4) != 0 || p[0] != '/') {
        c->http_status = 400; /* Bad Request */
        return;
    }

    /* GET /1/250 HTTP/1.1\r\n
     * GET /streamid/bitrate HTTP/1.1\r\n
     */

    c->streamid = c->bps = 0;

    /* "GET /" */
    p = c->r->ptr + 5;

    while(p[0] && p[0] >= '0' && p[0] <= '9') {
        /* streamid */
        c->streamid = c->streamid * 10 + p[0] - '0';
        p ++;
    }

    if (p[0] != '/') {
        c->http_status = 400;
        return;
    }

    p ++;

    /* bit rate */
    while(p[0] && p[0] >= '0' && p[0] <= '9') {
        c->bps = c->bps * 10 + p[0] - '0';
        p ++;
    }

    uploader_session * u = NULL;
    for (i = 0; i < uploaders.size; i++)
    {
        u = uploaders.s + i;
        if (u->is_used && u->streamid == c->streamid) break;
    }

    if (i == uploaders.size) {
        c->http_status = 400;
    } else {
        s = uploaders.s + i;
        c->s = s;
        c->next = c->s->clients;
        c->s->clients = c;
        c->wpos = 0;
        c->wait = 0;

        /* prepare http response header */
        strftime(date, sizeof("Fri, 01 Jan 1990 00:00:00 GMT")+1,
            "%a, %d %b %Y %H:%M:%S GMT", gmtime(&cur_ts));

        c->w->used = snprintf(c->w->ptr, c->w->size,
                "HTTP/1.0 200 OK\r\nDate: %s\r\nServer: Youku Live Receiver\r\n"
                "Content-Type: video/x-flv\r\n"
                "Expires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n"
                "\r\n",
                date);

#define FLVHEADER "FLV\x1\x5\0\0\0\x9\0\0\0\0" /* 9 plus 4bytes of UINT32 0 */
#define FLVHEADER_SIZE 13
        /* we need to fill standard flv header */
        memcpy(c->w->ptr + c->w->used, FLVHEADER, FLVHEADER_SIZE /* sizeof(FLVHEADER) - 1 */);
        c->w->used += FLVHEADER_SIZE;
#undef FLVHEADER
#undef FLVHEADER_SIZE
        if (s->onmetadata) buffer_append(c->w, s->onmetadata);
        if (s->avc0) buffer_append(c->w, s->avc0);
        if (s->aac0) buffer_append(c->w, s->aac0);

        c->live = NULL;
    }
}

static void
write_ffmpeg_client(struct http_client *c)
{
    int r = 0, cnt = 0;
    struct iovec chunks[2];

    if (c == NULL || c->s == NULL) return;

    if (c->wpos < c->w->used) {
        chunks[0].iov_base = c->w->ptr + c->wpos;
        chunks[0].iov_len = c->w->used - c->wpos;
        cnt = 1;
    }

    if (c->live) {
        if (c->live_end < c->live->memory->used)
            c->live_end = c->live->memory->used;
            if (c->live_start < c->live_end) {
            chunks[cnt].iov_base = c->live->memory->ptr + c->live_start;
            r = c->live_end - c->live_start;
            if (r > 64*1024) r = 64*1024;
            chunks[cnt].iov_len = r;
            cnt ++;
        }
    }

    if (cnt == 0) {
        disable_http_client_write(c);
        return;
    }

    r = writev(c->fd, (struct iovec *)&chunks, cnt);
    if (r < 0) {
            if (errno != EINTR && errno != EAGAIN) {
            http_conn_close(c);
            return;
        }
        r = 0;
    }

    if (c->w->used > 0 && c->wpos <= c->w->used) {
        if (r >= (c->w->used - c->wpos)) {
            r -= (c->w->used - c->wpos);
            c->wpos = c->w->used = 0;
        } else {
            c->wpos += r;
            r = 0;
        }
    }

    c->live_start += r;

    if (c->live_start == c->live_end) {
        disable_http_client_write(c);
    } else {
        if (c->live->memory->used > c->live_end)
            c->live_end = c->live->memory->used;
    }
}

static void
stream_http_client(struct http_client *c)
{
    uploader_session *s;

    if (c == NULL || c->s == NULL || c->fd <= 0) return;

    s = c->s;
    if (s->head == NULL || s->tail == NULL) {
        /* need more data from upstream */
        disable_http_client_write(c);
        return;
    }

    if (c->live == NULL) {
        if ((conf.seek_to_keyframe && s->tail->keymap_used == 0) ||
            (conf.seek_to_keyframe == 0 && s->tail->map_used == 0)) {
            disable_http_client_write(c);
            return;
        }

        /* set to use newest segment by default */
        c->live = s->tail;
        c->live_idx = c->live->map_used - 1;

        /* set c->live_start */
        if (conf.seek_to_keyframe)
            c->live_start = c->live->keymap[c->live->keymap_used-1].start;
        else
            c->live_start = c->live->map[c->live_idx].start;

        /* set c->live_end */
        if (c->live->is_full)
            c->live_end = c->live->memory->used;
        else
            c->live_end = c->live->map[c->live_idx].end;
    } else if (c->live_start == c->live_end) {
        /* previous write operation finished */
        if ((c->live_end < c->live->memory->used) && c->live->map_used > 0) {
            /* current segment has more data to write*/
            c->live_start = c->live_end;
            c->live_idx = c->live->map_used - 1;
            if (c->live->is_full == 0)
                c->live_end = c->live->map[c->live_idx].end;
            else
                c->live_end = c->live->memory->used; /* there may be some audio tag at the end of segment */
        } else {
            /* operation to write current segment completed
             * try next segment now
             */
            int to_rewind = 0;
            if (s->head && s->head != s->tail) {
                /* make sure that we have two segments */
                if (c->live->is_full == 1) {
                    /* so c->live == s->head
                     * but we check it again
                     */
                    if (c->live != s->tail && s->tail->map_used > 0) {
                        c->live = s->tail;
                        to_rewind = 1;
                    }
                } else if (c->live_end >= c->live->memory->used) {
                    /* previous /streamid/bps/offset request finish write c->head
                     * but before that s->tail is to_rewind to c->head
                     */
                    if (c->live == s->head) {
                            if (s->tail->is_full == 0 &&& s->tail->map_used > 0) {
                            /* switch to s->tail */
                            c->live = s->tail;
                            to_rewind = 1;
                        }
                    } else if (c->live == s->tail) {
                        /* client speed is too slow, live stream catchup the client with two rounds */
                            if (s->head->map_used > 0) {
                            c->live = s->head;
                            to_rewind = 1;
                        }
                    }
                }
            }

            if (to_rewind == 1) {
                c->live_start = 0;
                if (c->live->is_full) {
                    c->live_idx = c->live->map_used - 1;
                    c->live_end = c->live->memory->used;
                } else if (c->live->map_used > 0) {
                    c->live_idx = c->live->map_used - 1;
                    c->live_end = c->live->map[c->live_idx].end;
                } else {
                    c->live_end = 0;
                    c->live_idx = 0;
                }
            } else {
                disable_http_client_write(c);
                return;
            }
        }
    }

    write_ffmpeg_client(c);
}

static void
ffmpeg_http_handler(const int fd, const short which, void *arg)
{
    struct http_client *c;
    int r;
    char buf[129];

    c = (struct http_client *)arg;
    if (c == NULL) return;

    if (which & EV_READ) {
        switch(c->state) {
            case 0:
                r = read_buffer(fd, c->r);
                if (r < 0) {
                    http_conn_close(c);
                } else if (r > 0) {
                    /* terminate it with '\0' for following strstr */
                    c->r->ptr[c->r->used] = '\0';

                    if (strstr(c->r->ptr, "\r\n\r\n")) {
                        /* end of request http header */
                        parse_ffmpeg_request(c);

                        if (c->http_status == 0) {
                            c->state = 1;
                            /* write HTTP header and first part of stream to client */
                            r = write(c->fd, c->w->ptr, c->w->used);
                            if (r < 0) {
                                if (errno != EINTR && errno != EAGAIN) {
                                    http_conn_close(c);
                                    return;
                                }
                                r = 0;
                            }

                            c->wpos += r;
                            enable_http_client_write(c);
                            if (c->wpos == c->w->used) {
                                /* finish of writing header */
                                c->wpos = c->w->used = 0;
                                stream_http_client(c);
                            }
                        } else {
                            write_ffmpeg_http_error(c);
                        }
                    }
                }
                break;
            case 1: /* write live flash stream */
                /* just drop incoming unused bytes */
                r = read(c->fd, buf, 128);
                if ((r == -1 && errno != EINTR && errno != EAGAIN) || r == 0)
                    http_conn_close(c);
                break;
        }
    } else if (which & EV_WRITE) {
        if (c->wpos < c->w->used || c->live) {
            write_ffmpeg_client(c);
        } else {
            disable_http_client_write(c);
        }
    }
}

static void
ffmpeg_http_accept(const int fd, const short which, void *arg)
{
    struct http_client *c = NULL;
    int newfd;
    struct sockaddr_in s_in;
    socklen_t len = sizeof(struct sockaddr_in);

    memset(&s_in, 0, len);
    newfd = accept(fd, (struct sockaddr *) &s_in, &len);
    if (newfd < 0) {
        fprintf(conf.logfp, "%s: (%s.%d) fd %d accept() failed\n", cur_ts_str, __FILE__, __LINE__, fd);
        return ;
    }

    c = (struct http_client *)calloc(1, sizeof(*c));
    if (c == NULL) {
        fprintf(conf.logfp, "%s: (%s.%d) out of memory for fd #%d\n",
                cur_ts_str, __FILE__, __LINE__, newfd);
        close(newfd);
        return;
    }

    if (c->r == NULL) c->r = buffer_init(2048);
    if (c->w == NULL) c->w = buffer_init(2048);

    if (c->r == NULL || c->w == NULL) {
        fprintf(conf.logfp, "%s: (%s.%d) out of memory for fd #%d\n",
                cur_ts_str, __FILE__, __LINE__, newfd);
        close(newfd);
        return;
    }

    c->fd = newfd;
    set_nonblock(c->fd);

    c->state = 0;
    c->s = NULL;
    c->next = NULL;
    c->http_status = 0;
    c->wait = 0;
    c->wpos = 0;
    c->live = NULL;
    c->live_start = c->live_end = c->live_idx = 0;

    event_set(&(c->ev), c->fd, EV_READ|EV_PERSIST, ffmpeg_http_handler, (void *) c);
    event_add(&(c->ev), 0);
}

static void
receiver_timer_service(const int fd, short which, void *arg)
{
    struct timeval tv;

    cur_ts = time(NULL);
    strftime(cur_ts_str, 127, "%Y-%m-%d %H:%M:%S", localtime(&cur_ts));

    tv.tv_sec = 1; tv.tv_usec = 0; /* check for every 1 seconds */
    event_add(&ev_receiver_timer, &tv);
/*
    j = uploaders.used;
    for (i = 0; i < j; i ++) {
        s = uploaders.s[i];
        if (s == NULL) continue;
        if (s->fd == 0) {
            fprintf(stderr, "%s: (%s.%d) close terminated ffmpeg stream #%d/#%d\n",
                    cur_ts_str, __FILE__, __LINE__, s->streamid, s->bps);
            uploader_close(s);
        } else if (s->expire_ts <= cur_ts) {
            fprintf(stderr, "%s: (%s.%d) close expired ffmpeg stream #%d/#%d\n",
                    cur_ts_str, __FILE__, __LINE__, s->streamid, s->bps);
            uploader_close(s);
        }
    }
    */
}

#define BUFLEN 4096
static void
ffmpeg_udp_admin(const int fd, const short which, void *arg)
{
    struct sockaddr_in server;
    char buf[BUFLEN+1], buf2[BUFLEN+1];
    int r, i, structlength;
    struct uploader_session *s;

    structlength = sizeof(server);
    r = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *) &server, &structlength);
    if (r == -1) return;

    buf[r] = '\0';
    if (buf[r-1] == '\n') buf[--r] ='\0';

    if (strncasecmp(buf, "stat", 4) == 0) {
        r = snprintf(buf2, BUFLEN, "YOUKU Flash Live System Receiver v%s, total #%d streams\n",
                VERSION, uploaders.size);
        sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
        
        for (i = 0; i < uploaders.size; i ++) {
            if (!uploaders.s[i].is_used) continue;

            s = uploaders.s + i;
            r = snprintf(buf2, BUFLEN, "#%d Receiver: streamid #%d, width %d, height %d, framerate #%d\n",
                    i+1, s->streamid, s->width, s->height, s->framerate);
            sendto(fd, buf2, r, 0, (struct sockaddr *) &server, sizeof(server));
        }
    } else {
        sendto(fd, NULL, 0, 0, (struct sockaddr *) &server, sizeof(server));
    }
}

static void
receiver_service(void)
{
    struct timeval tv;

    event_init();
    event_set(&ev_receiver, receiver_fd, EV_READ|EV_PERSIST, uploader_http_accept, NULL);
    event_add(&ev_receiver, 0);

    if(conf.verbose_mode)
        fprintf(conf.logfp, "uploader_http_accept setup.\n");

    event_set(&ev_http, http_fd, EV_READ|EV_PERSIST, ffmpeg_http_accept, NULL);
    event_add(&ev_http, 0);

    event_set(&ev_udp, udp_fd, EV_READ|EV_PERSIST, ffmpeg_udp_admin, NULL);
    event_add(&ev_udp, 0);

    /* timer */
    evtimer_set(&ev_receiver_timer, receiver_timer_service, NULL);
    tv.tv_sec = 1; tv.tv_usec = 0; /* check for every 1 seconds */
    event_add(&ev_receiver_timer, &tv);

    /* event loop */
    event_dispatch();
}

int
main(int argc, char **argv)
{
    int c, to_daemon = 1, max = 1000, i = 0;
    char *configfile = NULL;
    struct sockaddr_in server, httpaddr, udpaddr;

    while(-1 != (c = getopt(argc, argv, "hDvc:M:"))) {
        switch(c)  {
        case 'D':
            to_daemon = 0;
            break;
        case 'v':
            conf.verbose_mode = 1;
            break;
        case 'c':
            configfile = optarg;
            break;
        case 'M':
            max = atoi(optarg);
            if (max <= 0) max = 1000;
            break;
        case 'h':
        default:
            show_receiver_help();
            return 1;
        }
    }
/*
    if (configfile == NULL || configfile[0] == '\0') {
        fprintf(stderr, "Please provide -c file argument, or use default '/etc/flash/receiver.xml' \n");
        show_receiver_help();
        return 1;
    }
*/
    cur_ts = time(NULL);
    strftime(cur_ts_str, 127, "%Y-%m-%d %H:%M:%S", localtime(&cur_ts));

    if (parse_receiverconf(configfile)) {
        fprintf(stderr, "%s: (%s.%d) parse xml %s failed\n",
                cur_ts_str, __FILE__, __LINE__, configfile);
        return 1;
    }

    uploaders.size = max;
    uploaders.s = (struct uploader_session *)calloc(uploaders.size, sizeof(uploader_session));
    if (uploaders.s == NULL) {
        fprintf(stderr, "%s: (%s.%d) out of memory\n",cur_ts_str, __FILE__, __LINE__);
        return 1;
    }

    for(i = 0; i < uploaders.size; i++)
        uploader_init(uploaders.s + i, i);

    memset((char *) &server, 0, sizeof(server));
    server.sin_family = AF_INET;
    if (conf.recv_host == NULL || conf.recv_host[0] == '\0')
        server.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        server.sin_addr.s_addr = inet_addr(conf.recv_host);

    server.sin_port = htons(conf.recv_port);

    memset(&httpaddr, 0, sizeof(httpaddr));
    httpaddr.sin_family = AF_INET;
    if (conf.http_host == NULL || conf.http_host[0] == '\0')
        httpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        httpaddr.sin_addr.s_addr = inet_addr(conf.http_host);
    httpaddr.sin_port = htons(conf.http_port);

    memset(&udpaddr, 0, sizeof(udpaddr));
    udpaddr.sin_family = AF_INET;
    udpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    udpaddr.sin_port = htons(conf.admin_port);

    if (getuid() == 0) {
        struct rlimit rlim;
        if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
            /* set max file number to 40k */
            rlim.rlim_cur = 40000;
            rlim.rlim_max = 40000;
            if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
                fprintf(stderr, "(%s.%d) can't set 'max filedescriptors': %s\n", __FILE__, __LINE__, strerror(errno));
                return 1;
            }
        }
    }

    /* receiver */
    receiver_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (receiver_fd < 0) {
        fprintf(conf.logfp, "%s: (%s.%d) can't create receiver socket\n", cur_ts_str, __FILE__, __LINE__);
        return 1;
    }

    set_nonblock(receiver_fd);
    if (bind(receiver_fd, (struct sockaddr *) &server, sizeof(struct sockaddr)) || listen(receiver_fd, 512)) {
        fprintf(conf.logfp, "%s: (%s.%d) fd #%d can't bind/listen: %s\n",
                cur_ts_str, __FILE__, __LINE__, receiver_fd, strerror(errno));
        close(receiver_fd);
        return 1;
    }

    /* http */
    http_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (http_fd < 0) {
        fprintf(conf.logfp, "%s: (%s.%d) can't create http socket\n", cur_ts_str, __FILE__, __LINE__);
        return 1;
    }

    set_nonblock(http_fd);
    if (bind(http_fd, (struct sockaddr *) &httpaddr, sizeof(struct sockaddr)) || listen(http_fd, 512)) {
        fprintf(conf.logfp, "%s: (%s.%d) fd #%d can't bind/listen: %s\n",
                cur_ts_str, __FILE__, __LINE__, receiver_fd, strerror(errno));
        close(http_fd);
        close(receiver_fd);
        return 1;
    }

    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) {
        fprintf(stderr, "can't create udp socket\n");
        return 1;
    }

    set_nonblock(udp_fd);
    if (bind(udp_fd, (struct sockaddr *) &udpaddr, sizeof(udpaddr))) {
        if (errno != EINTR)
            fprintf(stderr, "(%s.%d) errno = %d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        close(udp_fd);
        return 1;
    }

    fprintf(conf.logfp, "%s: Youku Live Receiver v%s(" __DATE__ " " __TIME__ "), RECEIVER: TCP %s:%d, HTTPD: TCP %s:%d, UDP: %d\n",
            cur_ts_str, VERSION,
            conf.recv_host?conf.recv_host:"0.0.0.0", conf.recv_port,
            conf.http_host?conf.http_host:"0.0.0.0", conf.http_port,
            conf.admin_port);

    if (to_daemon && daemonize(0, 0) == -1) {
        fprintf(conf.logfp, "%s: (%s.%d) failed to be a daemon\n", cur_ts_str, __FILE__, __LINE__);
        exit(1);
    }

    /* ignore SIGPIPE when write to closing fd
     * otherwise EPIPE error will cause transporter to terminate unexpectedly
     */
    signal(SIGPIPE, SIG_IGN);

    receiver_service();
    return 0;
}
