/* simple flv infomartion dumper
 * to be updated later
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
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

int
tag_timestamp(char *flvfile, int filesize)
{
    FLVHeader *header;
    int fd, datasize, streampos;
    unsigned char *flv;
    FLVTag *tag;
    FILE *fp;
    char filename[128];
    int ts, i = 1, len;

    if (flvfile == NULL || flvfile[0] == '\0') return 1;
    fd = open(flvfile, O_RDONLY);
    if (fd == -1) {
        printf("can't open %s\n", flvfile);
        return 1;
    }

    /* mmap is faster for big files */
    flv = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);

    if(flv == NULL) {
        printf("can't mmap %s\n", flvfile);
        close(fd);
        return 1;
    }

    if (strncmp(flv, "FLV", 3)) {
        printf("not valid flv file %s\n", flvfile);
        munmap(flv, filesize);
        close(fd);
        return 1; /* not flv file */ 
    }

    header = (FLVHeader *)flv;

    streampos = 4 + (header->headersize[0] << 24) + (header->headersize[1] << 16) + 
        (header->headersize[2] << 8) + (header->headersize[3]);

    while (streampos < filesize) {
        if((int)(streampos + sizeof(FLVTag)) > filesize)
            break;
        tag = (FLVTag *)&flv[streampos];
        datasize = sizeof(FLVTag) + (tag->datasize[0] << 16) + (tag->datasize[1] << 8) + (tag->datasize[2]) + 4;
        if((streampos + datasize) > filesize)
            break;
        ts = (tag->timestamp_ex << 24) + (tag->timestamp[0] << 16) + (tag->timestamp[1] << 8) + tag->timestamp[2];

        if(tag->type == FLV_SCRIPTDATAOBJECT) {
            fprintf(stdout, "#%i SCRIPT TS -> %.3f s, #%d\n", i, 1.0 * ts / 1000, streampos);
        } else if (tag->type == FLV_VIDEODATA) {
            len = streampos + sizeof(FLVTag);
            if (((flv[len] & 0xf) == 0x7) && (flv[len+1] == 0)) {
                fprintf(stdout, "#%i VIDEO AVC0 TS -> %.3f s, #%d\n", i, 1.0 * ts/1000, streampos);
                snprintf(filename, 128, "%s.AVC0", flvfile);
                fp = fopen(filename, "wb+");
                if (fp) {
                    fwrite(flv + streampos, datasize, 1, fp);
                    fprintf(stdout, "\tWRITE #%dB AVC0 to %s\n", datasize, filename);
                    fclose(fp);
                }
            } else {
                if (((flv[len] & 0xf0) == 0x10) || ((flv[len] & 0xf0) == 0x40)) {
                    fprintf(stdout, "#%i VIDEO SEEKABLE TS -> %.3f s, #%d\n", i, 1.0 * ts/1000, streampos);
                } else {
                    fprintf(stdout, "#%i VIDEO TS -> %.3f s, #%d\n", i, 1.0 * ts/1000, streampos);
                }
                {
                /* RSL: dump tag */
                    int i = 0, j = 0;
                    while (i < datasize) {
                        printf("%02x", flv[streampos + i]);
                        ++ i;
                        ++ j;
                        if ((j % 2) == 0)
                            printf(" ");
                        if ((j % 8) == 0)
                            printf(" ");
                        if (j == 32) {
                            printf("\n");
                            j = 0;
                        }
                    }
                    printf("\n");
                }
            }
        } else if (tag->type == FLV_AUDIODATA) {
            len = streampos + sizeof(FLVTag);
            if (((flv[len] & 0xf0) == 0xA0) && (flv[len+1] == 0)) {
                fprintf(stdout, "#%i AUDIO AAC0 TS -> %.3f s, #%d\n", i, 1.0 * ts/1000, streampos);

                snprintf(filename, 128, "%s.AAC0", flvfile);
                fp = fopen(filename, "wb+");
                if (fp) {
                    fwrite(flv + streampos, datasize, 1, fp);
                    fprintf(stdout, "\tWRITE #%dB AAC0 to %s\n", datasize, filename);
                    fclose(fp);
                }
            } else {
                fprintf(stdout, "#%i AUDIO TS -> %.3f s, #%d\n", i, 1.0 * ts/1000, streampos);
            }
        }
        streampos += datasize;
        i ++;
    }
    /* close flv file first */
    munmap(flv, filesize);
    close(fd);

    return 0;
}

int
main(int argc, char **argv)
{

    int i;
    struct stat st;

    if (argc < 2 ) {
        printf("%s flvfiles ...\n", argv[0]);
        return 1;
    }

    for (i = 1; i < argc; i ++) {
        memset(&st, 0, sizeof(st));
        if (0 == stat(argv[i], &st)) {
            tag_timestamp(argv[i], st.st_size);
            printf("\n");
        }
    }

    return 0;
}
