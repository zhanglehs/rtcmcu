/* Author: Wang shaogang(wangshaogang@youku.com)
 * Date:   2010-08-20
 *
 * Purpose: convert H264 flv/mp4 to MPEG2 TS stream on the fly
 *
 * port to live stream system by QHY
 */

#include <sys/types.h>
#include <stdint.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <unistd.h>

#include "transporter.h"

static const char revision[] __attribute__((used)) = { "$Id: ipad.c 516 2011-05-17 09:19:21Z qhy $" };

static int g_audio_num = 0, g_cc = 0, g_cc2 = 0;
static int g_audio_size = 0, g_audio_size2 = 0, g_video_left_size = 0;
static unsigned char* g_audio_pos = NULL, g_video_left[189];

extern struct config conf;
extern int verbose_mode;

struct ts_node
{
	struct ts_node* next;
	void* data;
};

struct ts_list
{
	struct ts_node* head;
	struct ts_node* tail;
};

struct ts_video
{
	double dts;
	double pts;
	int size;
	unsigned char* data;
};

struct ts_audio
{
	double pts;
	int used;
	int size;
	unsigned char* data;
};

static void
add_ts_list(struct ts_list* list, void* data)
{
	struct ts_node* node = NULL;
	node = calloc(1, sizeof(struct ts_node));
	if (node) {
		node->data = data;
		node->next = NULL;
		if (list->head) {
			list->tail->next = node;
			list->tail = node;
		} else {
			list->tail = list->head = node;
		}
	}
}

static void
set_audio_size(void)
{
	if (g_audio_pos) {
		g_audio_pos[0] = g_audio_size2 >> 8;
		g_audio_pos[1] = g_audio_size2 & 0xff;
	}
}

static void
pts_str(int fourbits, double pts, unsigned char* data)
{
	uint64_t t = (uint64_t) (pts + 0.5);
	int ret;

	data[0] = (fourbits << 4) | (((t >> 30) & 0x07) << 1) | 1;
	ret = (((t >> 15) & 0x7fff) << 1) | 1;
	data[1] = ret >> 8;
	data[2] = ret & 0xff;
	ret = (((t) & 0x7fff) << 1) | 1;
	data[3] = ret >> 8;
	data[4] = ret & 0xff;
}

static void
pcr_str(double pcr, unsigned char* data)
{
	uint64_t t = pcr * 27000 + 0.5;
	unsigned int pcr_base = t / 300;
	unsigned int pcr_ext = t % 300;

	data[0] = (pcr_base >> 25) & 0xff;
	data[1] = (pcr_base >> 17) & 0xff;
	data[2] = (pcr_base >> 9) & 0xff;
	data[3] = (pcr_base >> 1) & 0xff;
	data[4] = ((pcr_base & 1) << 7) + (pcr_ext >> 8);
	data[5] = pcr_ext & 0xff;
}

static void
append_frame(struct buffer *dst, unsigned char *frame, int size)
{
	if (dst == NULL || frame == NULL || size == 0) return;

	if ((dst->used + size) > dst->size)
	       buffer_expand(dst, dst->used + size + 10);	

	if ((dst->used + size) < dst->size) {
		memcpy(dst->ptr + dst->used, frame, size);
		dst->used += size;
	}
}

static void
write_ts_audio_tail(struct buffer *mem, unsigned char* data, int size)
{
	unsigned char buf[189];
	int header_len, stuffing_len, used;
	header_len = 4;
	stuffing_len = 188 - header_len - size;
	memcpy(buf, "\x47\x01\x02", 3);
	used = 3;
	if (stuffing_len) {
		buf[used] = g_cc2 | 0x30;
		used ++;
		buf[used] = stuffing_len - 1;
		used ++;
		if (stuffing_len > 1) {
			buf[used] = 0;
			used ++;
			if (stuffing_len > 2) {
				memset(buf + used, '\xff', stuffing_len - 2);
				used += stuffing_len - 2;
			}
		}
	} else {
		buf[used] = g_cc2 | 0x10;
		used ++;
	}
	memcpy(buf+used, data, size);
	
	append_frame(mem, buf, 188);

	g_audio_size2 += size;
	g_cc2 = (g_cc2 + 1) & 0xf;
}

static int
get_audio(struct ts_list* ts_audios, double dts, unsigned char* buf, int size)
{
	int used = 0;
	struct ts_node* node = NULL;
	struct ts_audio* audio = NULL;

	node = ts_audios->head;
	while (node) {
		audio = node->data;
		if (audio->pts >= dts)
			return 0;
		used += audio->size;
		if (used >= size)
			break;
		node = node->next;
	}
	used = 0;

	while (ts_audios->head) {
		audio = ts_audios->head->data;
		if (audio->pts >= dts)
			break;

		if ((used + audio->size) > size) {
			memcpy(buf + used, audio->data + audio->used, size - used);
			audio->used += size - used;
			audio->size -= size - used;
			used = size;
			break;
		} else {
			memcpy(buf + used, audio->data + audio->used, audio->size);
			used += audio->size;

			node = ts_audios->head;
			ts_audios->head = node->next;
			audio = node->data;

			free(audio->data);
			free(audio);
			free(node);

			if (used == size) break;
		}
	}
	return used;
}

static void
write_ts_audio_normal(struct buffer *mem, unsigned char* data, int size, double pts, int flag)
{
	unsigned char buf[189];
	int used;

	if (170 == size)
		memcpy(buf, "\x47\x41\x02", 3);
	else
		memcpy(buf, "\x47\x01\x02", 3);
	buf[3] = g_cc2 | 0x10;
	used = 4;

	if (170 == size) {
		g_audio_size2 = size + 8;
		memcpy(buf + used, "\x00\x00\x01\xc0\x00\x00\x80\x80\x05", 9);
		if (flag)
			buf[used+6] = '\x84';
		used += 9;
		pts_str(2, (pts+10000)*90, buf+used);
		used += 5;
	} else {
		g_audio_size2 += 184;
	}
	memcpy(buf+used, data, size);
	
	append_frame(mem, buf, 188);

	if (170 == size)
		g_audio_pos = mem->ptr + mem->used + 8 - 188;

	g_cc2 = (g_cc2 + 1) & 0xf;
}

static void
write_ts_audio(struct buffer *dst, double dts, struct ts_list* ts_audios)
{
	int size = 0;
	unsigned char buf[189];
	int used = 0;
	double pts = -1;
	int flag = -1;
	struct ts_audio* audio = NULL;
	struct ts_node *node;

	while (ts_audios->head) {
		audio = ts_audios->head->data;
		if (audio->pts >= dts)
			return;
		if (0 == g_audio_num)
			size = 170;
		else
			size = 184;
		used = 0;
		pts = audio->pts;

		if (audio->used) flag = 0;
		else flag = 1;

		if (g_audio_num > 3 && g_audio_size > 2*184 && audio->size <= size) {
			node = ts_audios->head;
			ts_audios->head = node->next;
			audio = node->data;
			free(node);

			memcpy(buf, audio->data + audio->used, audio->size);
			used = audio->size;
			write_ts_audio_tail(dst, buf, used);
			free(audio->data);
			free(audio);
			g_audio_num = 0;
			set_audio_size();
			continue;
		}

		used = get_audio(ts_audios, dts, buf, size);
		if (0 == used)
			return;
		if (used < size) {
			write_ts_audio_tail(dst, buf, used);
			set_audio_size();
		} else {
			write_ts_audio_normal(dst, buf, size, pts, flag);
		}
		g_audio_num++;
	}
}

static void
write_ts_video2(struct buffer *dst, unsigned char* data, int size)
{
	unsigned char buf[189];
	int header_len, stuffing_len, used;

	header_len = 4;
	stuffing_len = 188 - header_len - size;
	memcpy(buf, "\x47\x01\x01", 3);
	used = 3;
	if (stuffing_len) {
		buf[used] = g_cc | 0x30;
		used ++;
		buf[used] = stuffing_len - 1;
		used ++;
		if (stuffing_len > 1) {
			buf[used] = 0;
			used ++;
			if (stuffing_len > 2) {
				memset(buf + used, 0xff, stuffing_len - 2);
				used += stuffing_len - 2;
			}
		}
	} else {
		buf[used] = g_cc | 0x10;
		used ++;
	}
	memcpy(buf+used, data, size);
	
	append_frame(dst, buf, 188);
	g_cc = (g_cc + 1) & 0xf;
}

static void
write_ts_video1(struct buffer* dst, double dts, unsigned char* data, int size)
{
	unsigned char buf[189];
	int header_len, stuffing_len, used;

	if (g_video_left_size)
		data[6] = 0x80;
	header_len = 4 + 8;
	stuffing_len = 188 - header_len - size - g_video_left_size;
	memcpy(buf, "\x47\x41\x01", 3);
	buf[3] = g_cc | 0x30;
	buf[4] = stuffing_len + 8 - 1;
	buf[5] = 0x10;
	pcr_str(dts + 9900, buf + 6);
	used = 12;
	if (stuffing_len > 0) {
		memset(buf + used, 0xff, stuffing_len);
		used += stuffing_len;
	}
	memcpy(buf + used, data, 19);
	used += 19;
	if (g_video_left_size)
		memcpy(buf + used, g_video_left, g_video_left_size);
	used += g_video_left_size;
	memcpy(buf + used, data + 19, 188 - used);
	
	append_frame(dst, buf, 188);

	g_cc = (g_cc + 1) & 0xf;
}

static void
write_ts_video(struct buffer *mem, struct ts_video* video, struct ts_list* ts_audios, double audio_dts)
{
	int used = 0;
	int size = 0;

	if ((video->size + g_video_left_size) <= 176) {
		if (g_video_left_size) {
			write_ts_video2(mem, g_video_left, g_video_left_size);
			g_video_left_size = 0;
		}
		write_ts_video1(mem, video->dts, video->data, video->size);
		write_ts_audio(mem, audio_dts, ts_audios);
	} else {
		used = 176 - g_video_left_size;
		write_ts_video1(mem, video->dts, video->data, used);
		write_ts_audio(mem, audio_dts, ts_audios);
		while ((video->size - used) > 130) {
			if (video->size - used >= 184)
				size = 184;
			else
				size = video->size - used;
			write_ts_video2(mem, video->data + used, size);
			used += size;
		}
		g_video_left_size = video->size - used;
		if (g_video_left_size)
			memcpy(g_video_left, video->data + used, g_video_left_size);
	}
}

static void
get_pes(struct stream *s, struct ts_video* video)
{
	int used, used2;
	uint32_t size;

	memcpy(video->data, "\x00\x00\x01\xe0\x00\x00\x84\xc0", 8);
	video->data[8] = 10;
	pts_str(3, (video->pts + 10000)*90, video->data + 9);
	pts_str(1, (video->dts + 10000)*90, video->data + 14);
	memcpy(video->data + 19, "\x00\x00\x00\x01\t\xf0\x00", 7);
	used = 26;
	memcpy(video->data + used, "\x00\x00\x01", 3);
	used += 3;
	memcpy(video->data + used, s->g_param.sps, s->g_param.sps_len);
	used += s->g_param.sps_len;
	memcpy(video->data + used, "\x00\x00\x01", 3);
	used += 3;
	memcpy(video->data + used, s->g_param.pps, s->g_param.pps_len);
	used += s->g_param.pps_len;

	used2 = used;
	while ((used2 + 4) < video->size) {
		size = FLV_UI32(video->data + used2);
		if ((used2 + 4 + size) > video->size) {
			/* wrong data, make sure that there has enough memory */
			if (verbose_mode)
				ERROR_LOG(conf.logfp, "#%d/#%d: BAD TS VIDEO DATA: used/size/video->size/sps/pps -> %d/0x%x/%d/%d/%d\n",
						(int) s->si.streamid, (int) s->si.bps, used2, size, video->size, s->g_param.sps_len, s->g_param.pps_len);
			break;
		} else {
			memcpy(video->data + used, "\x00\x00\x01", 3);
			used += 3;
			memmove(video->data + used, video->data + used2 + 4, size);
			used += size;
			used2 += 4 + size;
		}
	}
	video->size = used;
}

static void
write_pat(struct buffer* dst, int cc)
{
	unsigned char buf[189];
	memset(buf, '\xff', 188);
	memcpy(buf, "\x47\x40\x00", 3);
	buf[3] = cc | 0x10;
	memcpy(buf + 4, "\x00\x00\xb0\x0d\x00\x01\xc1\x00\x00\x00\x01\xe1\x00\xe8\xf9\x5e\x7d", 17);
	
	append_frame(dst, buf, 188);
}

static void
write_pmt(struct stream* s, struct buffer* dst, int cc)
{
	unsigned char buf[189];

	memset(buf, '\xff', 188);
	memcpy(buf, "\x47\x41\x00", 3);
	buf[3] = cc | 0x10;
	if (s->g_param.channel_conf)
		memcpy(buf+4, "\x00\x02\xb0\x17\x00\x01\xc1\x00\x00\xe1\x01\xf0\x00\x1b\xe1\x01\xf0\x00\x0f\xe1\x02\xf0\x00\x9e\x28\xc6\xdd", 27);
	else
		memcpy(buf+4, "\x00\x02\xb0\x17\x00\x01\xc1\x00\x00\xe1\x01\xf0\x00\x1b\xe1\x01\xf0\x00\x03\xe1\x02\xf0\x00\xff\x35\x42\x58", 27);
	
	append_frame(dst, buf, 188);
}

static void
write_ts(struct buffer* dst, struct ts_list* ts_videos, struct ts_list* ts_audios)
{
	struct ts_video* video = NULL, *video2 = NULL;
	struct ts_audio* audio = NULL;
	struct ts_node *node;
	double dts;

	g_audio_num = 0;
	g_audio_size = 0;
	g_video_left_size = 0;
	g_audio_size2 = 0;
	g_audio_pos = NULL;

	node = ts_audios->head;
	while (node) {
		audio = node->data;
		g_audio_size += audio->size;
		node = node->next;
	}

	while (ts_videos->head) {
		node = ts_videos->head;
		video = node->data;
		ts_videos->head = node->next;
		free(node);
		if (ts_videos->head) {
			video2 = ts_videos->head->data;
			dts = video2->dts;
		} else {
			dts = 3600000.*24*365;
		}

		write_ts_video(dst, video, ts_audios, dts);

		free(video->data);
		free(video);
	}
	set_audio_size();
}

void
generate_ts_stream(struct stream *s, time_t start_ts, time_t end_ts)
{
	struct ts_chunkqueue *cq = NULL;

	int pos = 0;
	FLVTag *flvtag;
	int tagsize, len, first_audio = 1, first_video = 1;
	struct ts_list ts_videos, ts_audios;
	struct ts_video *video;
	struct ts_audio *audio;
	double audio_pts = 0.0, video_dts = 0.0;
	unsigned char *data;
	int size, offset, pre_size;
	time_t ts, flv_start_ts = 0, flv_end_ts = 0;

	struct buffer *m;
	struct buffer *buf;

	if (s == NULL || s->ipad_mem == NULL || s->ipad_mem->used == 0 || start_ts >= end_ts) return;

	if (s->ts_squence == 0) s->ts_squence = random() & 0xffff; /* new squence number */

	ts_videos.head = ts_videos.tail = NULL;
	ts_audios.head = ts_audios.tail = NULL;

	buf = s->ipad_mem;
#if 0
	{
		/* write FLV to FILE */
		int fd;
		char name[256];

		snprintf(name, 255, "ts/%d-%d-%d.flv", (int)s->si.channel_id, (int) s->si.bps, (int) start_ts);
		fd = open(name, O_CREAT|O_WRONLY|O_TRUNC, 0644);
		if (fd > 0) {
#define FLVHEADER "FLV\x1\x5\0\0\0\x9\0\0\0\0" /* 9 plus 4bytes of UINT32 0 */
#define FLVHEADER_SIZE 13
			write(fd, FLVHEADER, FLVHEADER_SIZE);
			if (s->onmetadata) write(fd, s->onmetadata->ptr, s->onmetadata->used);
			if (s->avc0) write(fd, s->avc0->ptr, s->avc0->used);
			if (s->aac0) write(fd, s->aac0->ptr, s->aac0->used);
			write(fd, buf->ptr, buf->used);
			close(fd);
		}
	}
#endif

	while (pos < buf->used) {
		flvtag = (FLVTag *)(buf->ptr + pos);
		len = sizeof(FLVTag);
		tagsize = (flvtag->datasize[0] << 16) + (flvtag->datasize[1] << 8) + flvtag->datasize[2] + sizeof(FLVTag);
		ts = ((flvtag->timestamp_ex << 24) + (flvtag->timestamp[0] << 16) +
				(flvtag->timestamp[1] << 8) + flvtag->timestamp[2]);
		if (ts > end_ts) break; /* put left buffer in next roud */

		if (flv_start_ts == 0) flv_start_ts = ts;
		if (ts > flv_end_ts) flv_end_ts = ts;

		if (flvtag->type == FLV_AUDIODATA &&
			(((buf->ptr[len] & 0xf0) != 0xA0) || (buf->ptr[len+1] != 0))) {
			/* AUDIO and not AAC0 */
			if (first_audio) {
				audio_pts = ts * 1.0;
				first_audio = 0;
			}

			/* ---------------- */
			data = buf->ptr + pos + sizeof(FLVTag);
			size = tagsize - sizeof(FLVTag);

			audio = calloc(1, sizeof(struct ts_audio));
			if (audio == NULL) {
				ERROR_LOG(conf.logfp, "OUT OF MEMORY WHILE PROCESSING AUDIO TS\n");
				break;
			}

			audio->pts = audio_pts;
			if (s->g_param.channel_conf) {
				audio->size = size - 2 + 7;
				audio->data = calloc(1, audio->size);
				if (audio->data == NULL) {
					ERROR_LOG(conf.logfp, "OUT OF MEMORY WHILE PROCESSING AUDIO TS\n");
					free(audio);
					break;
				}
				memcpy(audio->data + 7, data + 2, size - 2);

				memcpy(audio->data, "\xff\xf1", 2);
				audio->data[2] = (s->g_param.objecttype << 6) + (s->g_param.sample_rate_index << 2) +
					(s->g_param.channel_conf >> 2);
				audio->data[3] = (s->g_param.channel_conf & 3) << 6;
				audio->data[4] = audio->size >> 3;
				audio->data[5] = ((audio->size & 7) << 5) + 0x1f;
				audio->data[6] = '\xfc';
			} else {
				audio->size = size - 1;
				audio->data = calloc(1, audio->size);
				if (audio->data == NULL) {
					ERROR_LOG(conf.logfp, "OUT OF MEMORY WHILE PROCESSING AUDIO TS\n");
					free(audio);
					break;
				}
				memcpy(audio->data, data + 1, size - 1);
			}

			add_ts_list(&ts_audios, audio);
			audio_pts += s->audio_interval;
		} else if (flvtag->type == FLV_VIDEODATA &&
			(((buf->ptr[len] & 0xf) != 0x7) || (buf->ptr[len+1] != 0))) {
			/* VIDEO and not AVC0 */

			data = buf->ptr + pos + sizeof(FLVTag);
			size = tagsize - sizeof(FLVTag);

			if (first_video) {
				video_dts = ts * 1.0;
				first_video = 0;
			}

			offset = FLV_UI24(data + 2) * 1.0 / s->video_interval + 0.5;
			video = calloc(1, sizeof(struct ts_video));
			if (video == NULL) {
				ERROR_LOG(conf.logfp, "OUT OF MEMORY WHILE PROCESSING VIDEO TS\n");
				break;
			}

			video->dts = video_dts;
			video->pts = video->dts + offset * s->video_interval;

			pre_size = 19 + 7 + 3 + s->g_param.sps_len + 3 + s->g_param.pps_len;
			video->size = size - 5 + pre_size;
			video->data = calloc(1, video->size);
			if (video->data == NULL) {
				ERROR_LOG(conf.logfp, "OUT OF MEMORY WHILE PROCESSING VIDEO TS\n");
				free(video);
				break;
			}

			memcpy(video->data + pre_size, data + 5, size - 5);

			get_pes(s, video);

			add_ts_list(&ts_videos, video);

			video_dts += s->video_interval;
		}

		pos += tagsize + 4; /* next tag */
	}

	/* FINISH PROCESS AUDIO & VIDEO TS */
	m = buffer_init(buf->used * 1.5);

	if (pos < buf->used) {
		/* KEEP UN-PROCESSED BUFFER */
		memmove(buf->ptr, buf->ptr + pos, buf->used - pos);
		buf->used -= pos;
	} else {
		buf->used = 0;
	}

	if (m == NULL) {
		/* out of memory */
		struct ts_node *node;
		while (ts_videos.head) {
			node = ts_videos.head;
			ts_videos.head = node->next;

			video = node->data;
			free(video->data);
			free(video);
			free(node);
		}

		while(ts_audios.head) {
			node = ts_audios.head;
			ts_audios.head = node->next;

			audio = node->data;
			free(audio->data);
			free(audio);
			free(node);
		}
		return;
	}

	write_pat(m, s->ts_squence % 16);
	write_pmt(s, m, s->ts_squence % 16);

	g_cc = s->g_video_c;
	g_cc2 = s->g_audio_c;
	write_ts(m, &ts_videos, &ts_audios);
	s->g_video_c = g_cc;
	s->g_audio_c = g_cc2;

#if 0
	{
		/* write TS to FILE */
		int fd;
		char name[256];

		snprintf(name, 255, "ts/%d-%d-%d.ts", (int)s->si.channel_id, (int) s->si.bps, (int) start_ts);
		fd = open(name, O_CREAT|O_WRONLY|O_TRUNC, 0644);
		if (fd > 0) {
			write(fd, m->ptr, m->used);
			close(fd);
		}
	}
#endif
	cq = (struct ts_chunkqueue *) calloc(1, sizeof(struct ts_chunkqueue));
	if (cq == NULL) {
		buffer_free(m);
		return;
	}
	cq->next = NULL;
	cq->mem = m;
	cq->ts = start_ts;
	cq->duration = flv_end_ts - flv_start_ts;

	if (s->max_ts_duration < cq->duration) s->max_ts_duration = cq->duration;

	cq->squence = s->ts_squence ++;

	if (s->ipad_head == NULL) {
		s->cq_number = 1;
		s->ipad_head = s->ipad_tail = cq;
	} else {
		s->ipad_tail->next = cq;
		s->ipad_tail = cq;
		++ s->cq_number;
	}

	if (s->cq_number > s->max_ts_count) {
		cq = s->ipad_head;
		s->ipad_head = cq->next;
		-- s->cq_number;
		buffer_free(cq->mem);
		free(cq);
	}

	if (verbose_mode)
		ERROR_LOG(conf.logfp, "stream #%d/#%d: generate ts from [%d, %d, %4.2fs], FLV LEN %d -> TS LEN %d\n",
				(int) s->si.streamid, (int) s->si.bps,
				(int) s->ipad_start_ts, (int) s->ipad_end_ts,
				(s->ipad_end_ts - s->ipad_start_ts)/1000.0,
				pos, s->ipad_tail->mem->used);
}

