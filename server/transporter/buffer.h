#ifndef BUFFER_H_
#define BUFFER_H_

#include <stdint.h>

#define FLV_AUDIODATA   8
#define FLV_VIDEODATA   9
#define FLV_SCRIPTDATAOBJECT    18

typedef struct
{
    unsigned char signature[3];
    unsigned char version;
    unsigned char flags;
    unsigned char headersize[4];
} FLVHeader;

typedef struct
{
    unsigned char type;
    unsigned char datasize[3];
    unsigned char timestamp[3];
    unsigned char timestamp_ex;
    unsigned char streamid[3];
} FLVTag;

struct buffer
{
    char *ptr;
    int size, used;
};

typedef struct buffer buffer;

struct stream_map
{
    uint32_t ts; /* timestamp * 1000 */
    uint32_t start, end;
};

struct segment
{
    buffer *memory;

    struct stream_map *map;
    uint32_t map_used, map_size;

    struct stream_map *keymap;
    uint32_t keymap_used, keymap_size;

    uint32_t first_ts, last_ts;

    int is_full;
};

/* functions */
buffer *buffer_init(int);
void buffer_free(buffer *);
void buffer_expand(buffer *, int );
void buffer_copy(buffer *, buffer *);
void buffer_append(buffer *, buffer *);
int buffer_ignore_safe(buffer * buf, int bytes);
int buffer_ignore(buffer * buf, int bytes);


/* flv tag */
uint32_t FLV_UI16(unsigned char *);
uint32_t FLV_UI24(unsigned char *);
uint32_t FLV_UI32(unsigned char *);
int32_t flv_get_timestamp(unsigned char * ts, unsigned char ts_ex);
void flv_set_timestamp(unsigned char * ts, unsigned char * ts_ex, int32_t value);
double get_double(unsigned char* );
int memstr(char *, char *, int , int);
void append_script_datastr(buffer *, char *, int);
void append_script_var_str(buffer *, char *, int);
void append_script_var_double(buffer *, double);
void append_script_var_bool(buffer *, int);
void append_script_var_emca_array(buffer *, char *, int);
void append_script_dataobject(buffer *, char *, int);
void append_script_emca_array_end(buffer *);

/* common used functions */
int read_buffer(int, buffer *);
int read_dvb_buffer(int, buffer *);
void set_nonblock(int);
int daemonize(int, int);

void free_segment(struct segment *);
struct segment * init_segment(int, int);
char * query_str_get(char * src, const int src_len, char * key, 
        char * value_buf, const int buf_len);
int http_parse_req_line(char * src, const int src_len, 
        char * method, const int method_len,
        char * path, const int path_len,
        char * query_str, const int query_str_len);
uint64_t get_curr_tick();
#endif
