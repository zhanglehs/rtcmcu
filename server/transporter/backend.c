/* source code to parse flv stream data
 * uses [TAG_DATA + PREV_TAGSIZE] as analysis unit
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <stdint.h>
#include <event.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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

#include <sys/ioctl.h>

#include "transporter.h"

/* client.c */
extern void process_waitqueue(struct stream *);
extern void close_all_clients(void);
extern void close_stream_clients(struct stream *);

/* transporter.c */
extern struct config conf;
extern struct event_base *main_base;
extern int verbose_mode;
extern char cur_ts_str[128];

extern int connect_live_stream_server(struct stream *);

/* ipad.c */
extern void generate_ts_stream(struct stream *, time_t, time_t);

static const char resivion[] __attribute__((used)) = { "$Id: backend.c 508 2011-01-15 16:21:14Z qhy $" };

static void
free_ts_chunkqueue(struct ts_chunkqueue *cq)
{
    struct ts_chunkqueue *t, *n;

    t = cq;
    while (t) {
        n = t->next;
        buffer_free(t->mem);
        free(t);
        t = n;
    }
}

void
reset_stream_ts_data(struct stream *s)
{
    if (s == NULL) return;

    if (s->ts_squence) s->ts_squence += s->cq_number + 1; /* update ts squence number */
    if (s->ipad_mem) s->ipad_mem->used = 0;

    free_ts_chunkqueue(s->ipad_head);
    s->ipad_head = s->ipad_tail = NULL;
    s->cq_number = 0;
    s->max_ts_duration = 0;
    s->ts_reset_ticker = 0;
    s->ipad_start_ts = 0;
}

void
close_live_stream(struct stream *s)
{
    if (s == NULL) return;
    if (s->fd > 0) {
        event_del(&s->ev);
        close(s->fd);
        s->fd = 0;
        if (verbose_mode)
            ERROR_LOG(conf.logfp, "stream #%d/#%d: close backend's stream connection\n", s->si.streamid, s->si.bps);
    }
    s->connected = 0;
    if (s->r) s->r->used = 0;
    if (s->w) s->w->used = 0;

    if (s->g_param.sps) free(s->g_param.sps);
    if (s->g_param.pps) free(s->g_param.pps);
    memset(&(s->g_param), 0, sizeof(s->g_param));
    if (s->ipad_mem) s->ipad_mem->used = 0;
}

static void
generate_onMetaData(struct stream *s)
{
    buffer *b;
    int len = 3;
    FLVTag tag;
    char *dst;

    if (s == NULL) return;

    b = buffer_init(512);
    if (b == NULL) return;

    if (s->width > 0) len ++;
    if (s->height > 0) len ++;
    if (s->framerate > 0) len ++;

    memset(&tag, 0, sizeof(tag));
    tag.type = FLV_SCRIPTDATAOBJECT;
    b->used = sizeof(tag);

    append_script_dataobject(b, "onMetaData", len); /* variables */
    append_script_datastr(b, "starttime", strlen("starttime"));
    append_script_var_double(b, (double) s->start_ts);
    append_script_datastr(b, "hasVideo", strlen("hasVideo"));
    append_script_var_bool(b, 1);
    append_script_datastr(b, "hasAudio", strlen("hasAudio"));
    append_script_var_bool(b, 1);
    if (s->width > 0) {
        append_script_datastr(b, "width", strlen("width"));
        append_script_var_double(b, (double) s->width);
    }
    if (s->height > 0) {
        append_script_datastr(b, "height", strlen("height"));
        append_script_var_double(b, (double) s->height);
    }
    if (s->framerate > 0) {
        append_script_datastr(b, "framerate", strlen("framerate"));
        append_script_var_double(b, (double) s->framerate);
    }

    if (s->audiosamplerate > 0) {
        append_script_datastr(b, "audiosamplerate", strlen("audiosamplerate"));
        append_script_var_double(b, (double) s->audiosamplerate);
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

    if (s->onmetadata) buffer_free(s->onmetadata);
    s->onmetadata = b;
}

static void
parse_flv_script_data(struct stream *s)
{
    int pos, to_reset = 0;
    uint64_t ts = 0, width = 0, height = 0, framerate = 0, audiosamplerate = 0;
    uint32_t ts_2 = 0;
    struct timeval t;

    if (s == NULL || s->r == NULL || s->r->ptr == NULL || s->r->used < s->tagsize) return;

    pos = memstr(s->r->ptr, "onMetaData", s->r->used, strlen("onMetaData"));
    if (pos < 0) return; /* not onMetaData tag */

    /* to find "height" */
    pos = memstr(s->r->ptr, "height", s->r->used, strlen("height"));
    if (pos >= 0) {
        /* we got it */
        pos += strlen("height");
        switch(s->r->ptr[pos]) {
            case 0: /* double */
                height = (uint32_t) get_double((unsigned char *)(s->r->ptr + pos + 1));
                break;
            default:
                break;
        }
    }

    /* to find "width" */
    pos = memstr(s->r->ptr, "width", s->r->used, strlen("width"));
    if (pos >= 0) {
        /* we got it */
        pos += strlen("width"); /* to the end of 'width' */
        switch(s->r->ptr[pos]) {
            case 0: /* double */
                width = (uint32_t) get_double((unsigned char *)(s->r->ptr + pos + 1));
                break;
            default:
                break;
        }
    }

    /* to find "framerate" */
    pos = memstr(s->r->ptr, "framerate", s->r->used, strlen("framerate"));
    if (pos >= 0) {
        /* we got it */
        pos += strlen("framerate"); /* to the end of 'framerate' */
        switch(s->r->ptr[pos]) {
            case 0: /* double */
                framerate = (uint32_t) get_double((unsigned char *)(s->r->ptr + pos + 1));
                break;
            default:
                break;
        }
    }

    /* to find "audiosamplerate" */
    pos = memstr(s->r->ptr, "audiosamplerate", s->r->used, strlen("audiosamplerate"));
    if (pos >= 0) {
        /* we got it */
        pos += strlen("audiosamplerate"); /* to the end of 'audiosamplerate' */
        switch(s->r->ptr[pos]) {
            case 0: /* double */
                audiosamplerate = (uint32_t) get_double((unsigned char *)(s->r->ptr + pos + 1));
                break;
            default:
                break;
        }
    }

    if (s->height > 0 && s->max_ts_count > 0)
        s->ts_reset_ticker = 13; /* TS DISCONUNITY when onMetaData arrived*/

    if (height > 0) s->height = height;
    if (width > 0) s->width = width;

    if (framerate > 0) {
        s->framerate = framerate;
        s->video_interval = 1000.0 / framerate;
    }

    if (audiosamplerate > 0) s->audiosamplerate = audiosamplerate;
    else s->audiosamplerate = 22050; /* 22K */

    s->audio_interval = 576. * 1000 / s->audiosamplerate; /* default to use mp3 audio */

    /* to find "starttime" */
    pos = memstr(s->r->ptr, "starttime", s->r->used, strlen("starttime"));
    if (pos >= 0) {
        /* we got it */
        pos += strlen("starttime"); /* to the end of 'starttime' */
        switch(s->r->ptr[pos]) {
            case 11: /* date */
            case 0: /* double */
                ts = (uint64_t) get_double((unsigned char *)(s->r->ptr + pos + 1));
                break;
            default:
                break;
        }
    }

    if (ts > 0) {
        s->backend_start_ts = ts;
        if (verbose_mode)
            ERROR_LOG(conf.logfp, "stream #%d/#%d: backend's live stream start_ts = %.0f\n",
                       s->si.streamid, s->si.bps, (double) s->backend_start_ts);
    } else {
        if (gettimeofday(&t, NULL))
            s->backend_start_ts = t.tv_sec * 1000 + t.tv_usec / 1000;
    }

    if (s->start_ts == 0) {
        s->start_ts = s->backend_start_ts;
        s->ts_diff = 0;
        generate_onMetaData(s);
    } else {
        s->ts_diff = s->backend_start_ts - s->start_ts;
        if (s->backend_start_ts < s->start_ts) {
            /* to_reset = 1; */
            /* live stream server's time changed */
            if (verbose_mode)
                ERROR_LOG(conf.logfp, "stream #%d/#%d: live stream server's time changed to sometime before current\n",
                        s->si.streamid, s->si.bps);
        } else {
            if (s->last_stream_ts && s->ts_diff > s->last_stream_ts) {
                ts_2 = s->ts_diff - s->last_stream_ts;
                if (ts_2 > (s->live_buffer_time * 1000)) {
                    to_reset = 1;
                    if (verbose_mode)
                        ERROR_LOG(conf.logfp, "stream #%d/#%d: stream last data timestamp is %u ms before now, s->ts_diff = %u, s->last_stream_ts = %u\n",
                                s->si.streamid, s->si.bps, ts_2, s->ts_diff, s->last_stream_ts);
                }
            } else if (s->ts_diff > 2 * 86400 * 1000) { /* 2 days */
                to_reset = 1;
                if (verbose_mode)
                    ERROR_LOG(conf.logfp, "stream #%d/#%d: new live stream server's time is %u ms after current stream start time\n",
                            s->si.streamid, s->si.bps, s->ts_diff);
            }
        }

        if (to_reset) {
            if (verbose_mode)
                ERROR_LOG(conf.logfp, "stream #%d/#%d: RESETTING\n", s->si.streamid, s->si.bps);

            close_stream_clients(s); /* close connected clients for this stream */

            /* reset stream data */
            s->start_ts = s->backend_start_ts;
            s->ts_diff = 0;
            s->last_stream_ts = 0;
            generate_onMetaData(s);
            buffer_free(s->avc0);
            buffer_free(s->aac0);
            s->avc0 = s->aac0 = NULL;

            if (s->head == s->tail) s->head = NULL;

            free_segment(s->tail);
            free_segment(s->head);

            s->head = s->tail = NULL;

            reset_stream_ts_data(s);
        }
    }
}

static void
parse_flv_aac0(struct stream *s)
{
    unsigned char *data;

    if (s == NULL || s->aac0 == NULL || s->aac0->used < 8) return;

    data = s->aac0->ptr + sizeof(FLVTag);

    s->g_param.objecttype = (data[2] >> 3) - 1;
    s->g_param.sample_rate_index = ((data[2] & 7) << 1) + (data[3] >> 7);
    s->g_param.channel_conf = (data[3] >> 3) & 0xf;

    if (s->g_param.channel_conf) {
        /* with aac0 audio conf */
        if (7 == s->g_param.sample_rate_index)
            s->audio_interval = 1024. * 1000 / 22050;
        else if (4 == s->g_param.sample_rate_index)
            s->audio_interval = 1024. * 1000 / 44100;
        else
            s->audio_interval = 1024. * 1000 / s->audiosamplerate;
    } else {
        /* mp3 audio conf */
        /* use default audio settings */
        s->audiosamplerate = 22050; /* FIXME */
        s->audio_interval = 576. * 1000 / s->audiosamplerate;
    }
}

static void
parse_flv_avc0(struct stream *s)
{
    int datasize;
    unsigned char* data;

    if (s == NULL || s->avc0 == NULL || s->avc0->used < 13) return;

    data = s->avc0->ptr + sizeof(FLVTag);
    datasize = s->avc0->used - 4;

    s->g_param.sps_len = FLV_UI16(data + 11);
    if (datasize < (13 + s->g_param.sps_len + 3))
        return;

    s->g_param.sps = realloc(s->g_param.sps, s->g_param.sps_len);
    if (s->g_param.sps)
        memcpy(s->g_param.sps, data + 13, s->g_param.sps_len);
    else {
        s->g_param.sps_len = 0;
        return;
    }

    s->g_param.pps_len = FLV_UI16(data + 14 + s->g_param.sps_len);
    if (datasize < (16 + s->g_param.sps_len + s->g_param.pps_len)) {
        free(s->g_param.sps);
        s->g_param.sps = NULL;
        s->g_param.sps_len = 0;
        return;
    }
    s->g_param.pps = realloc(s->g_param.pps, s->g_param.pps_len);
    if (s->g_param.pps)
        memcpy(s->g_param.pps, data + 16 + s->g_param.sps_len, s->g_param.pps_len);
    else
        s->g_param.pps_len = 0;
}

/* set r = 0 if don't quit, set r = 1 to close connection to backend live stream */
static void
append_live_stream(struct stream *s, int *r)
{
    FLVTag *tag;
    buffer *b, *b2;
    int len, is_seekable = 0, is_avc0 = 0, changed = 0;
    uint32_t ts = 0;
    uint32_t offset;

    struct segment *seg = NULL;
    struct stream_map *m = NULL;

    if (s == NULL || s->r == NULL || s->r->ptr == NULL || s->r->used < s->tagsize) return;

    b = s->r;
    tag = (FLVTag *)b->ptr;

    if (tag->type != FLV_AUDIODATA && tag->type != FLV_VIDEODATA)
        return;

    ts = (tag->timestamp_ex << 24) + (tag->timestamp[0] << 16) + (tag->timestamp[1] << 8) + tag->timestamp[2];

    /* check specific audio/video tag */
    len = sizeof(FLVTag);
    if (tag->type == FLV_AUDIODATA && ((b->ptr[len] & 0xf0) == 0xA0) && (b->ptr[len+1] == 0)) {
        /* AAC Audio config data */
        b2 = buffer_init(s->tagsize + 1);
        if (b2) {
            memcpy(b2->ptr, b->ptr, s->tagsize);
            b2->used = s->tagsize;
            if (s->aac0) buffer_free(s->aac0);
            s->aac0 = b2;
            /* reset aac0 tag timestamp */
            b2->ptr[4] = b2->ptr[5] = b2->ptr[6] = b2->ptr[7] = '\0';
            parse_flv_aac0(s);
        }
    } else if (tag->type == FLV_VIDEODATA && ((b->ptr[len] & 0xf) == 0x7) && (b->ptr[len+1] == 0)) {
        /* AVC Video config data */
        b2 = buffer_init(s->tagsize + 1);
        if (b2) {
            memcpy(b2->ptr, b->ptr, s->tagsize);
            b2->used = s->tagsize;
            if (s->avc0) buffer_free(s->avc0);
            s->avc0 = b2;
            /* reset avc0 tag timestamp */
            b2->ptr[4] = b2->ptr[5] = b2->ptr[6] = b2->ptr[7] = '\0';
            parse_flv_avc0(s);
            if (s->ipad_mem) s->ipad_mem->used = 0; /* DROP PREVIOUS IPAD BUFFER on AVC0*/
        }
        is_avc0 = 1;
    }

    /* update tag timestamp */
    if (ts == 0) {
        ts = s->last_stream_ts;
        changed = 1;
    } else if (s->ts_diff > 0) {
        ts += s->ts_diff;
        changed = 1;
    } else if (s->ts_diff < 0) {
        if (ts < (0 - s->ts_diff)) {
            if (r) *r = 2; /* to reset */
            return;
        }
        ts += s->ts_diff;
        changed = 1;
    }

    if (ts < s->last_stream_ts) {
        /* make sure tag's timestamp is increasing */
        s->ts_diff += s->last_stream_ts - ts + s->video_interval;
        ts = s->last_stream_ts + s->video_interval;
    }

    if (changed) {
        tag->timestamp_ex = (ts >> 24) & 0xff;
        tag->timestamp[0] = (ts >> 16) & 0xff;
        tag->timestamp[1] = (ts >> 8) & 0xff;
        tag->timestamp[2] = ts & 0xff;
    }

    /* keyframe */
    if (is_avc0 == 0 && tag->type == FLV_VIDEODATA && (((b->ptr[len] & 0xf0) == 0x10) || ((b->ptr[len] & 0xf0) == 0x40))) {
        is_seekable = 1;
        if (s->switch_backend == 1) {
            if (verbose_mode)
                ERROR_LOG(conf.logfp, "stream #%d/#%d: TO SWITCH BACKEND AT KEYFRAME\n", s->si.streamid, s->si.bps);
            s->switch_backend = 0;

            if (s->max_ts_count > 0) {
                /* don't forget the last stream data */
                if (s->ipad_start_ts > 0 && ts > 0)
                    generate_ts_stream(s, s->ipad_start_ts, ts);

                if (s->ipad_mem) s->ipad_mem->used = 0; /* DROP BUFFER */
                s->ipad_start_ts = ts;
            }

            if (r) *r = 1;
            return;
        }
    }

    /* normal audio/video tag, put them into queue */
    if (s->head == NULL || s->tail == NULL) {
        /* first segment */
        seg = s->tail = s->head = init_segment(s->si.bps, s->live_buffer_time);
    } else {
        if ((s->tail->memory->used + s->tagsize) > s->tail->memory->size) {
            if (s->head == s->tail) {
                /* only one segment, allocate new one */
                s->head->is_full = 1;
                seg = init_segment(s->si.bps, s->live_buffer_time);
                if (seg) s->tail = seg;
                if (verbose_mode)
                    ERROR_LOG(stderr, "stream #%d/#%d: NEW %s SEG 2#\n",
                            s->si.streamid, s->si.bps, is_seekable?"SEEKABLE":"NORMAL");
            } else {
                /* we already has two segments, swap head with tail */
                seg = s->head;
                s->head = s->tail;
                s->tail = seg;

                s->head->is_full = 1;
                /* reset segment data */
                seg->map_used = seg->keymap_used = 0;
                seg->first_ts = seg->last_ts = 0;
                seg->memory->used = 0;
                seg->is_full = 0;
                if (verbose_mode)
                    ERROR_LOG(stderr, "stream #%d/#%d: %s SWAP SEG 1# WITH SEG #2\n",
                            s->si.streamid, s->si.bps, is_seekable?"SEEKABLE":"NORMAL");
            }
        } else {
            seg = s->tail;
        }
    }

    if (seg == NULL) {
        ERROR_LOG(conf.logfp, "stream #%d/#%d: out of memory for new segment\n", s->si.streamid, s->si.bps);
        return;
    }

    if (seg->map_used >= seg->map_size) {
        char *pp;
#define SEGMENT_STEP 128
        pp = realloc(seg->map, sizeof(struct stream_map)*(seg->map_size + SEGMENT_STEP));
        if (pp) {
            seg->map = (struct stream_map *)pp;
            seg->map_size += SEGMENT_STEP;
        } else {
            ERROR_LOG(conf.logfp, "stream #%d/#%d: out of memory for segment's maps\n", s->si.streamid, s->si.bps);
            return;
        }
#undef SEGMENT_STEP
    }

    if (seg->keymap_used >= seg->keymap_size) {
        char *pp;
#define KEYSEGMENT_STEP 32
        pp = realloc(seg->keymap, sizeof(struct stream_map)*(seg->keymap_size + KEYSEGMENT_STEP));
        if (pp) {
            seg->keymap = (struct stream_map *)pp;
            seg->keymap_size += KEYSEGMENT_STEP;
        } else {
            ERROR_LOG(conf.logfp, "stream #%d/#%d: out of memory for segment's keymaps\n", s->si.streamid, s->si.bps);
            return;
        }
#undef KEYSEGMENT_STEP
    }

    offset = seg->memory->used;
    /* append buffer content */
    memcpy(seg->memory->ptr + seg->memory->used, b->ptr, s->tagsize);
    seg->memory->used += s->tagsize;

    if (tag->type == FLV_VIDEODATA) {
        /* we only save segment of video tag
         * to update video segment's maps and keymaps
         */
        m = seg->map + seg->map_used;
        m->ts = ts;
        m->start = offset;
        m->end = seg->memory->used; /* including prev_tagsize */
        ++ seg->map_used;

        /* update first_ts & last_stream_ts */
        if (seg->first_ts == 0) seg->first_ts = ts;
        s->last_stream_ts = seg->last_ts = ts;

        /* update keyframe map */
        if (is_seekable) {
            m = seg->keymap + seg->keymap_used;
            m->ts = ts;
            m->start = offset;
            m->end = seg->memory->used; /* including prev_tagsize */
            ++ seg->keymap_used;

            if (s->max_ts_count > 0) {
                if (s->ipad_start_ts > 0 && ts > 0) {
                    /* to generate mpeg ts stream */
                    generate_ts_stream(s, s->ipad_start_ts, ts);
                } else {
                    if (s->ipad_mem) s->ipad_mem->used = 0; /* DROP BUFFER */
                }

                s->ipad_start_ts = ts;
            }
        }
        /* to process waiting job queue when VIDEO frame arrived */
        process_waitqueue(s);
    }

    if (s->max_ts_count > 0 && s->ipad_mem) {
        /* append ipad_memory buffer */
        if (s->ipad_mem->size < (s->tagsize + s->ipad_mem->used)) {
            buffer_expand(s->ipad_mem, s->ipad_mem->used + s->tagsize + 10);
        }
        memcpy(s->ipad_mem->ptr + s->ipad_mem->used, b->ptr, s->tagsize);
        s->ipad_mem->used += s->tagsize;
    }
}

static void
parse_flv_stream(struct stream *s)
{
    int to_continue = 1, pos = 0, r;
    char *p;
    FLVTag *flvtag;

    if (s == NULL) return;

    while(to_continue) {
        if(s->state == state_http_header) {
            to_continue = 0;
            if ((p = strstr(s->r->ptr, "\r\n\r\n"))) {
                /* end of HTTP response header */
                /* HTTP/1.0 200 OK, HTTP/1.0 302 Found */
                if (s->r->used > 10 && strncmp(s->r->ptr, "HTTP/1.", 7) == 0) {
                    pos = strtol(s->r->ptr + 9, NULL, 10);
                    if (pos == 200) {
                        /* matched status code */
                        pos = p - s->r->ptr + 4;
                        p += 4;
                        s->state = state_flv_header;
                        if (pos < s->r->used)
                            to_continue = 1;
                        s->connected = 1;
                        s->retry_count = 0;
                    } else {
                        close_live_stream(s);
                    }
                }
            }
        }

        if (to_continue && s->state == state_flv_header) {
            to_continue = 0;
            if ((s->r->used - pos) >= (sizeof(FLVHeader) + 4)) { /* 4 is UINT32 of previous tag size */
                if (strncmp(s->r->ptr + pos, "FLV", 3) != 0) {
                    close_live_stream(s);
                    return;
                }
                s->state = state_flv_tag; /* start of next tag */
                pos += sizeof(FLVHeader) + 4;
                if (s->r->used > pos)
                    to_continue = 1;
            }
        }

        if (to_continue && s->state == state_flv_tag) {
            to_continue = 0;
            if ((s->r->used - pos) >= sizeof(FLVTag)) {
                flvtag = (FLVTag *)(s->r->ptr + pos);

                s->tagsize = (flvtag->datasize[0] << 16) + (flvtag->datasize[1] << 8) +
                    flvtag->datasize[2] + sizeof(FLVTag) + 4;

                if (s->tagsize > 131072) { /* > 128K */
                    ERROR_LOG(conf.logfp, "stream #%d/#%d: too big flv tag size: %d, closing connection\n",
                            s->si.streamid, s->si.bps, s->tagsize);
                    close_live_stream(s);
                    return;
                }

                if (s->tagsize > s->r->size)
                    buffer_expand(s->r, s->tagsize);

                s->state = state_flv_data;
                if (s->r->used > pos)
                    to_continue = 1;
            }
        }

        if (to_continue && s->state == state_flv_data) {
            /* FLVTag + Data */
            to_continue = 0;
            if ((s->r->used - pos) >= s->tagsize) {
                memmove(s->r->ptr, s->r->ptr + pos, s->r->used - pos);
                s->r->used -= pos;
                pos = 0;
                /* full of tag data */
                flvtag = (FLVTag *)s->r->ptr;
                if (flvtag->type == FLV_AUDIODATA || flvtag->type == FLV_VIDEODATA ) {
                    r = 0;
                    append_live_stream(s, &r);
                    if (r) {
                        close_live_stream(s);
                        if (r == 2) {
                            /* reset live stream */
                            if (verbose_mode)
                                ERROR_LOG(conf.logfp, "stream #%d/#%d: RESETTING\n", s->si.streamid, s->si.bps);

                            close_stream_clients(s); /* close connected clients for this stream */

                            /* reset stream data */
                            s->start_ts = 0;
                            s->ts_diff = 0;
                            s->last_stream_ts = 0;
                            s->backend_start_ts = 0;

                            s->width = s->height = s->framerate = 0;

                            s->switch_backend = 0;

                            buffer_free(s->onmetadata);
                            buffer_free(s->avc0);
                            buffer_free(s->aac0);
                            s->onmetadata = s->avc0 = s->aac0 = NULL;

                            if (s->head == s->tail) s->head = NULL;
                            free_segment(s->tail);
                            free_segment(s->head);
                            s->head = s->tail = NULL;

                            reset_stream_ts_data(s);
                        }
                        connect_live_stream_server(s);
                        return;
                    }
                } else if (flvtag->type == FLV_SCRIPTDATAOBJECT ) {
                    parse_flv_script_data(s);
                }
                /* to read next flv tag */
                s->state = state_flv_tag;
                if (s->r->used > s->tagsize) {
                    memmove(s->r->ptr, s->r->ptr + s->tagsize, s->r->used - s->tagsize);
                    s->r->used -= s->tagsize;
                    if (s->r->used >= sizeof(FLVTag))
                        to_continue = 1;
                } else {
                    s->r->used = 0;
                }
            }
        } else {
            to_continue = 0;
        }
    }

    if (pos > 0) {
            if (pos < s->r->used) {
            memmove(s->r->ptr, s->r->ptr + pos, s->r->used - pos);
            s->r->used -= pos;
        } else {
            s->r->used = 0;
        }
    }
}

static void
live_stream_handler(const int fd, const short which, void *arg)
{
    struct stream *s;
    int toread = -1, r, socket_error, to_change = 1;
    socklen_t socket_error_len;

    s = (struct stream *)arg;
    if (s == NULL) return;

    if (which & EV_READ) {
        r = read_buffer(s->fd, s->r);
        if (r < 0) {
            close_live_stream(s);
        } else if (r > 0) {
            parse_flv_stream(s);
        }
    } else if (which & EV_WRITE) {
        if (s->state == state_start) {
            /* not connected */
            socket_error_len = sizeof(socket_error);
            socket_error = 0;
            /* try to finish the connect() */
            if (0 != getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_len) || socket_error != 0) {
                close_live_stream(s);
                return ;
            }
            s->state = state_http_header;
        }

        if (s->w->used > 0 && s->w->used > s->wpos) {
            toread = s->w->used - s->wpos;
            r = write(fd, s->w->ptr + s->wpos, toread);
            if (r < 0) {
                if (errno != EINTR && errno != EAGAIN) {
                    close_live_stream(s);
                    return;
                }
                r = 0;
            }

            s->wpos += r;
            if (s->wpos == s->w->used) {
                s->w->used = s->wpos = 0;
            } else {
                to_change = 0;
            }
        }

        if (to_change) {
            event_del(&s->ev);
            event_set(&s->ev, s->fd, EV_READ|EV_PERSIST, live_stream_handler, (void *) s);
            event_add(&s->ev, 0);
        }
    }

}

static int
init_live_connection(struct stream *s)
{
#define DEFAULT_BUFSIZE 32768 /* 32K */
    if (s->r) s->r->used = 0;
    else s->r = buffer_init(DEFAULT_BUFSIZE); /* response buffer */
#undef DEFAULT_BUFSIZE

    if (s->w) s->w->used = 0;
    else s->w = buffer_init(512); /* request header */
    if (s->r == NULL || s->w == NULL) return 1;
    s->state = 0;
    s->backend_start_ts = s->ts_diff = 0;
    s->wpos = s->tagsize = 0;
    s->switch_backend = 0;

    if (s->ipad_mem) s->ipad_mem->used = 0;
    else s->ipad_mem = buffer_init(400*1024); /* 400K */
    return 0;
}

/* return 0 when success, return 1 when failed */
int
connect_live_stream_server(struct stream *s)
{
    int r;
    socklen_t len;
    struct sockaddr_in server;

    if (s == NULL || s->connected == 1) return 0;

    if (s->fd > 0) return 0; /* connecting */

    if (init_live_connection(s)) {
        ERROR_LOG(conf.logfp, "stream #%d/#%d: out of memory for live stream connection\n", s->si.streamid, s->si.bps);
        return 1;
    }

    s->retry_count ++;
    s->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->fd < 0) {
        ERROR_LOG(conf.logfp, "stream #%d/#%d: can't create tcp socket\n", s->si.streamid, s->si.bps);
        return 1;
    }

    len = sizeof(struct sockaddr);
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(s->si.port);
    server.sin_addr.s_addr = s->si.ip;

    if (verbose_mode)
        ERROR_LOG(conf.logfp, "stream #%d/#%d: #%d try connect to backend server(%s:%d)\n",
                    s->si.streamid, s->si.bps, s->retry_count, inet_ntoa(server.sin_addr), s->si.port);
    s->state = state_start;
    set_nonblock(s->fd);
    r = connect(s->fd, (struct sockaddr *) &server, len);
    if (r == -1 && errno != EINPROGRESS && errno != EALREADY) {
        /* connect failed */
        if (verbose_mode)
            ERROR_LOG(conf.logfp, "stream #%d/#%d: can't connect to %s:%d\n",
                    s->si.streamid, s->si.bps, inet_ntoa(server.sin_addr), s->si.port);
        close(s->fd);
        s->fd = 0;
        s->r->used = s->w->used = 0;
        return 1;
    }

    s->backend_start_ts = time(NULL) * 1000;

    s->w->used = snprintf(s->w->ptr, s->w->size, "GET /%d/%d HTTP/1.1\r\nHost: %s\r\nYouku: TIC-TAC-TOE\r\n\r\n",
            s->si.streamid, s->si.bps, inet_ntoa(server.sin_addr));

    if (r == -1)
        event_set(&s->ev, s->fd, EV_WRITE|EV_PERSIST, live_stream_handler, (void *)s);
    else {
        s->state = state_http_header;
        /* r == 0, connection success */
        if (s->w->used > 0 && s->w->used > s->wpos) {
            r = write(s->fd, s->w->ptr, s->w->used);
            if (r < 0) {
                if (errno != EINTR && errno != EAGAIN) {
                    close(s->fd);
                    s->fd = 0;
                    s->r->used = s->w->used = 0;
                    return 1;
                }
                r = 0;
            }

            s->wpos += r;
            if (s->wpos == s->w->used)
                s->w->used = s->wpos = 0;
        }

        if (s->w->used == s->wpos)
            event_set(&s->ev, s->fd, EV_READ|EV_PERSIST, live_stream_handler, (void *)s);
        else
            event_set(&s->ev, s->fd, EV_WRITE|EV_PERSIST, live_stream_handler, (void *)s);
    }

    event_base_set(main_base, &s->ev);
    event_add(&s->ev, 0);
    return 0;
}
