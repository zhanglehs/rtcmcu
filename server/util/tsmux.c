/*****************************************************************************

 * tsmux.c: 

 *****************************************************************************

 * Copyright (C) 2005-2012 Tudou 

 *

 * Authors: Hualong Jiao <hljiao@tudou.com>

 *****************************************************************************/

#include <assert.h>

#include <stdio.h>

//#include <malloc.h>

//#include <memory.h>

#include <stdlib.h>

#include <string.h>

#include "tsmux.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#endif

static const int crc_table[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,

    0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3,
    0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
    0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b,
    0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,

    0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b,
    0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
    0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033,
    0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,

    0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43,
    0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
    0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c,
    0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,

    0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c,
    0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
    0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4,
    0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,

    0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044,
    0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
    0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c,
    0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,

    0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85,
    0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
    0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d,
    0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,

    0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d,
    0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
    0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65,
    0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,

    0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615,
    0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
    0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad,
    0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,

    0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a,
    0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
    0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082,
    0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,

    0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12,
    0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
    0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa,
    0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,

    0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda,
    0xb5365d03, 0xb1f740b4
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

typedef struct tsmuxer_handle
{

    unsigned char tsbuffer[188];

    PAT_SECTION_HEADER patSectionHeader;

    unsigned char patbuffer[8];

    PAT_ENTRY pat_table[32];

    unsigned char pat_table_buffer[128];

    PMT_SECTION pmtSection;

    unsigned char pmtbuffer[12];

    unsigned char streamheader[5];

    STREAM_DESCRIPTOR stream_descriptor[32];

    unsigned char programe_descp_buffer[1024];

    int patCounter;

    int pmtCounter;

    int aCounter;

    int vCounter;

    unsigned char *pOutBuffer;

    unsigned int sizeOfOutBuffer;

    unsigned int posOfOutBuffer;

} TSMUXER_HANDLE;

#if 0

static unsigned int
mpegts_crc32(const char *data, int len)
{

    register int i;

    unsigned int crc = 0xffffffff;

    for(i = 0; i < len; i++)

        crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *data++) & 0xff];

    return crc;

}

#endif /* 
        */

static int
mpegts_write_buffer(void *tsmux_handle, char *buffer, unsigned int bufSize)
{

    TSMUXER_HANDLE *pHandle;

    unsigned char *tmp_ptr = 0;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    if(bufSize + pHandle->posOfOutBuffer > pHandle->sizeOfOutBuffer)
    {

        pHandle->sizeOfOutBuffer += MEMORY_UNIT_SIZE;

        tmp_ptr = pHandle->pOutBuffer;

        pHandle->pOutBuffer =
            (unsigned char *) realloc(pHandle->pOutBuffer,
                                      pHandle->sizeOfOutBuffer);

        if(pHandle->pOutBuffer == 0)
        {

            if(tmp_ptr)

                free(tmp_ptr);

            return -1;

        }

    }

    memcpy(pHandle->pOutBuffer + pHandle->posOfOutBuffer, buffer, bufSize);

    pHandle->posOfOutBuffer += bufSize;

    return 0;

}

static int
ts_packet_buffer(void *tsmux_handle, TS_PACKET * header)
{

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    pHandle->tsbuffer[0] = (unsigned char) (header->sync_byte);

    pHandle->tsbuffer[1] = 0;

    pHandle->tsbuffer[1] |= (header->transport_error_indicator << 7);

    pHandle->tsbuffer[1] |= (header->payload_unit_start_indicator << 6);

    pHandle->tsbuffer[1] |= (header->transport_priority << 5);

    pHandle->tsbuffer[1] |= (header->PID >> 8);

    pHandle->tsbuffer[2] = 0;

    pHandle->tsbuffer[2] |= (header->PID & 0xff);

    pHandle->tsbuffer[3] = 0;

    pHandle->tsbuffer[3] |= header->transport_scrambling_control << 6;

    pHandle->tsbuffer[3] |= header->adaptation_field_control << 4;

    pHandle->tsbuffer[3] |= header->continuity_counter & 0x0f;

    return 0;

}

static int
ts_packet_extention_buffer(void *tsmux_handle, TS_PACKET_EXTENTION * header)
{

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    pHandle->tsbuffer[0] = (unsigned char) (header->sync_byte);

    pHandle->tsbuffer[1] = 0;

    pHandle->tsbuffer[1] |= (header->transport_error_indicator << 7);

    pHandle->tsbuffer[1] |= (header->payload_unit_start_indicator << 6);

    pHandle->tsbuffer[1] |= (header->transport_priority << 5);

    pHandle->tsbuffer[1] |= (header->PID >> 8);

    pHandle->tsbuffer[2] = 0;

    pHandle->tsbuffer[2] |= (header->PID & 0xff);

    pHandle->tsbuffer[3] = 0;

    pHandle->tsbuffer[3] |= header->transport_scrambling_control << 6;

    pHandle->tsbuffer[3] |= header->adaptation_field_control << 4;

    pHandle->tsbuffer[3] |= header->continuity_counter & 0x0f;

    pHandle->tsbuffer[4] = 0;

    pHandle->tsbuffer[4] |= header->adaptation_field_length;

    if(header->adaptation_field_length > 0)
    {

        pHandle->tsbuffer[5] = 0;

        pHandle->tsbuffer[5] |= header->discontinuity_indicator << 7;

        pHandle->tsbuffer[5] |= header->random_access_indicator << 6;

        pHandle->tsbuffer[5] |=
            header->elementary_stream_priority_indicator << 5;

        pHandle->tsbuffer[5] |= header->PCR_flag << 4;

        pHandle->tsbuffer[5] |= header->OPCR_flag << 3;

        pHandle->tsbuffer[5] |= header->splicing_point_flag << 2;

        pHandle->tsbuffer[5] |= header->transport_private_data_flag << 1;

        pHandle->tsbuffer[5] |= header->adaptation_field_extension_flag;

    }

    return 0;

}

static int
pat_buffer(void *tsmux_handle, PAT_SECTION_HEADER * header)
{

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    pHandle->patbuffer[0] = (char) (header->table_id);

    pHandle->patbuffer[1] = 0;

    pHandle->patbuffer[1] |= header->section_syntax_indicator << 7;

    pHandle->patbuffer[1] |= header->zero << 6;

    pHandle->patbuffer[1] |= header->reserved1 << 4;

    pHandle->patbuffer[1] |= header->section_length >> 4;

    pHandle->patbuffer[2] = header->section_length & 0xff;

    pHandle->patbuffer[3] = header->transport_stream_id >> 8;

    pHandle->patbuffer[4] = header->transport_stream_id & 0xff;

    pHandle->patbuffer[5] = 0;

    pHandle->patbuffer[5] |= header->reserved2 << 6;

    pHandle->patbuffer[5] |= header->version_number << 1;

    pHandle->patbuffer[5] |= header->current_next_indicator;

    pHandle->patbuffer[6] = (char) (header->section_number);

    pHandle->patbuffer[7] = (char) (header->last_section_number);

    return 0;

}

static int
pat_entry_buffer(void *tsmux_handle)
{

    int i;

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    for(i = 0; i <= 124; i += 4)
    {

        pHandle->pat_table_buffer[i + 0] =
            pHandle->pat_table[i].program_number >> 8;

        pHandle->pat_table_buffer[i + 1] =
            pHandle->pat_table[i].program_number & 0xff;

        pHandle->pat_table_buffer[i + 2] =
            pHandle->pat_table[i].reserved << 5;

        pHandle->pat_table_buffer[i + 2] |= pHandle->pat_table[i].PID >> 8;

        pHandle->pat_table_buffer[i + 3] |= pHandle->pat_table[i].PID & 0xff;

    }

    return 0;

}

static int
mpegts_add_pat_entry(void *tsmux_handle, TS_INIT_PARA * pTsInitPara)
{

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    pHandle->pat_table[0].program_number = 1;

    pHandle->pat_table[0].reserved = 7;

    pHandle->pat_table[0].PID = pTsInitPara->pmt_pid;

    pHandle->patSectionHeader.pat_entry_number++;

    pHandle->patSectionHeader.section_length += sizeof(PAT_ENTRY);

    pat_buffer(pHandle, &pHandle->patSectionHeader);

    pat_entry_buffer(pHandle);

    return 0;

}

static int
pmt_buffer(void *tsmux_handle, PMT_SECTION * const header)
{

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    pHandle->pmtbuffer[0] = (char) (header->table_id);

    pHandle->pmtbuffer[1] = 0;

    pHandle->pmtbuffer[1] |= header->section_syntax_indicator << 7;

    pHandle->pmtbuffer[1] |= header->zero << 6;

    pHandle->pmtbuffer[1] |= header->reserved1 << 4;

    pHandle->pmtbuffer[1] |= header->section_length >> 8;

    pHandle->pmtbuffer[2] = header->section_length & 0xff;

    pHandle->pmtbuffer[3] = header->program_number >> 8;

    pHandle->pmtbuffer[4] = header->program_number & 0xff;

    pHandle->pmtbuffer[5] = 0;

    pHandle->pmtbuffer[5] |= header->reserved2 << 6;

    pHandle->pmtbuffer[5] |= header->version_number << 1;

    pHandle->pmtbuffer[5] |= header->current_next_indicator;

    pHandle->pmtbuffer[6] = (char) (header->section_number);

    pHandle->pmtbuffer[7] = (char) (header->last_section_number);

    pHandle->pmtbuffer[8] = 0;

    pHandle->pmtbuffer[8] |= header->reserved3 << 5;

    pHandle->pmtbuffer[8] |= header->PCR_PID >> 8;

    pHandle->pmtbuffer[9] = header->PCR_PID & 0xff;

    pHandle->pmtbuffer[10] = 0;

    pHandle->pmtbuffer[10] |= header->reserved4 << 4;

    pHandle->pmtbuffer[10] |= header->program_info_length >> 8;

    pHandle->pmtbuffer[11] = header->program_info_length & 0xff;

    return 0;

}

static int
mpegts_add_pmt_entry(void *tsmux_handle, TS_INIT_PARA * pTsInitPara,
                     unsigned char *sBuffer, unsigned short sLength)
{

    unsigned short infoLen, length, number, descriptorLen;

    unsigned char *buffer;

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    pHandle->pmtSection.table_id = 0x02;    //0x02 is table id for PMT  

    pHandle->pmtSection.section_syntax_indicator = 1;

    pHandle->pmtSection.zero = 0x0;

    pHandle->pmtSection.reserved1 = 3;

    pHandle->pmtSection.program_number = 1;

    pHandle->pmtSection.reserved2 = 3;

    pHandle->pmtSection.version_number = 0;

    pHandle->pmtSection.current_next_indicator = 1;

    pHandle->pmtSection.section_number = 0;

    pHandle->pmtSection.last_section_number = 0;

    pHandle->pmtSection.reserved3 = 7;

    pHandle->pmtSection.PCR_PID = pTsInitPara->pcr_pid;

    pHandle->pmtSection.reserved4 = 0xf;

    pHandle->pmtSection.section_length = 13;

    pHandle->pmtSection.program_info_length = 0;

    descriptorLen = 0;

    buffer = sBuffer;

    length = sLength;

    number = 0;

    while(length > 0)
    {

        pHandle->stream_descriptor[number].stream_type = (unsigned char) *buffer;   //stream_type

        pHandle->stream_descriptor[number].reserved1 = 7;

#if DEBUG_PRINTF

        printf("Get Stream Type: 0x%02X!check Table 2-36!\n",
               (unsigned char) *buffer);

        printf("Get elementary_stream_id: 0x%02X!\n",
               (unsigned char) *(buffer + 1));

#endif /* 
        */

        if(pHandle->stream_descriptor[number].stream_type ==
           STREAM_TYPE_AUDIO_AAC)

            pHandle->stream_descriptor[number].elementary_PID =
                pTsInitPara->aud_pkt_pid;

        else if(pHandle->stream_descriptor[number].stream_type ==
                STREAM_TYPE_VIDEO_H264)

            pHandle->stream_descriptor[number].elementary_PID =
                pTsInitPara->vid_pkt_pid;

        infoLen = (unsigned short) *(buffer + 2);

        pHandle->stream_descriptor[number].reserved2 = 0xf;

        pHandle->stream_descriptor[number].ES_info_length = infoLen;

        length -= 4 + infoLen;

        memcpy(pHandle->stream_descriptor[number].descriptor,
               (unsigned char *) (buffer + 4), infoLen);

        buffer += 4 + infoLen;

        pHandle->pmtSection.section_length += 5 + infoLen;

        descriptorLen = 0;

        while(infoLen > 0)
        {

#if DEBUG_PRINTF

            printf("descriptor_tag of stream_info: 0x%02X\n",
                   (unsigned
                    char) (stream_descriptor[programe][number].descriptor
                           [descriptorLen]));

            switch ((unsigned
                     char) (stream_descriptor[programe][number].descriptor
                            [descriptorLen]))
            {

            case VIDEO_STREAM_DESCRIPTOR:

                printf("video stream descriptor found!\n");

                break;

            case AUDIO_STREAM_DESCRIPTOR:

                printf("audio stream descriptor found!\n");

                break;

            case HIERARCHY_DESCRIPTOR:

                printf("hierarchy descriptor found!\n");

                break;

            case REGISTRATION_DESCRIPTOR:

                printf("registration descriptor found!\n");

                break;

            case DATA_STREAM_ALIGNMENT_DESCRIPTOR:

                printf("data stream alignment descriptor found!\n");

                break;

            case TARGET_BACKGROUND_GRID_DESCRIPTOR:

                printf("target background grid descriptor found!\n");

                break;

            case VIDEO_WINDOW_DESCRIPTOR:

                printf("video window descriptor found!\n");

                break;

            case CA_DESCRIPTOR:

                printf("CA descriptor found!\n");

                break;

            case ISO_639_LANGUAGE_DESCRIPTOR:

                printf("ISO 639 language descriptor found!\n");

                break;

            case SYSTEM_CLOCK_DESCRIPTOR:

                printf("system clock descriptor found!\n");

                break;

            case MULTIPLEX_BUFFER_UTILIZATION_DECSRIPTOR:

                printf("multiplex buffer utilization descriptor found!\n");

                break;

            case COPYRIGHT_DESCRIPTOR:

                printf("copyright descriptor found!\n");

                break;

            case PRIVATE_DATA_INDICATOR_DESCRIPTOR:

                printf("private data indicator descriptor found!\n");

                break;

            case SMOOTHING_BUFFER_DESCRIPTOR:

                printf("smoothing buffer descriptor found!\n");

                break;

            case IBP_DESCRIPTOR:

                printf("IBP descriptor found!\n");

                break;

            default:

                printf("unknown descriptor!\n");

            }

#endif /* 
        */
            descriptorLen +=
                (unsigned char) (pHandle->
                                 stream_descriptor[number].descriptor
                                 [descriptorLen + 1]);

            infoLen -= descriptorLen;

        }

        number++;

        pHandle->pmtSection.stream_number++;

    }

    pmt_buffer(pHandle, &pHandle->pmtSection);

    return 0;

}

int
mpegts_init(void **tsmux_handle, TS_INIT_PARA * pTsInitPara, int bVideoflag,
            int bAudioflag)
{

    TSMUXER_HANDLE *pHandle;

    unsigned char data[1024];

    if(pTsInitPara->bfirstInit)
    {

        *tsmux_handle = 0;

        *tsmux_handle = (TSMUXER_HANDLE *) malloc(sizeof(TSMUXER_HANDLE));

        pHandle = (TSMUXER_HANDLE *) (*tsmux_handle);

        if(0 == pHandle)

            return -1;

        pHandle->patSectionHeader.table_id = 0x0;

        pHandle->patSectionHeader.section_syntax_indicator = 0x1;

        pHandle->patSectionHeader.zero = 0;

        pHandle->patSectionHeader.reserved1 = 3;

        pHandle->patSectionHeader.transport_stream_id =
            pTsInitPara->transportId;

        pHandle->patSectionHeader.reserved2 = 3;

        pHandle->patSectionHeader.version_number = 0;

        pHandle->patSectionHeader.current_next_indicator = 1;

        pHandle->patSectionHeader.section_number = 0;

        pHandle->patSectionHeader.last_section_number = 0;

        pHandle->patSectionHeader.pat_entry = pHandle->pat_table;

        pHandle->patSectionHeader.pat_entry_number = 0;

        pHandle->patSectionHeader.section_length = 5 + 4;   //including CRC

        pHandle->pmtSection.table_id = 0x02;    //0x02 is table id for PMT  

        pHandle->pmtSection.section_syntax_indicator = 0x1;

        pHandle->pmtSection.zero = 0x0;

        pHandle->pmtSection.reserved1 = 3;

        pHandle->pmtSection.program_number = 1;

        pHandle->pmtSection.reserved2 = 3;

        pHandle->pmtSection.version_number = 0;

        pHandle->pmtSection.current_next_indicator = 1;

        pHandle->pmtSection.section_number = 0;

        pHandle->pmtSection.last_section_number = 0;

        pHandle->pmtSection.reserved3 = 7;

        pHandle->pmtSection.PCR_PID = pTsInitPara->pcr_pid; //PCR_PID

        pHandle->pmtSection.reserved4 = 0xf;

        pHandle->pmtSection.program_info_length = 0;

        pHandle->pmtSection.stream_number = 0;

        mpegts_add_pat_entry(pHandle, pTsInitPara);

        if(bVideoflag && bAudioflag)
        {

            data[0] = STREAM_TYPE_VIDEO_H264;

            data[1] = pTsInitPara->vid_stream_id;

            *(uint16_t *) & data[2] = 0x0000;

            data[4] = STREAM_TYPE_AUDIO_AAC;

            data[5] = pTsInitPara->aud_stream_id;

            *(uint16_t *) & data[6] = 0x0000;

            mpegts_add_pmt_entry(pHandle, pTsInitPara, data, 8);

        }

        else if(bVideoflag)
        {

            data[0] = STREAM_TYPE_VIDEO_H264;

            data[1] = pTsInitPara->vid_stream_id;

            *(uint16_t *) & data[2] = 0x0000;

            mpegts_add_pmt_entry(pHandle, pTsInitPara, data, 4);

        }

        else if(bAudioflag)
        {

            data[0] = STREAM_TYPE_AUDIO_AAC;

            data[1] = pTsInitPara->aud_stream_id;

            *(uint16_t *) & data[2] = 0x0000;

            mpegts_add_pmt_entry(pHandle, pTsInitPara, data, 4);

        }

        else
        {

            return -1;

        }

        pTsInitPara->bfirstInit = 0x00;

        /////////////////////////////////////////////////////////

        pHandle->sizeOfOutBuffer = MEMORY_UNIT_SIZE;

        pHandle->pOutBuffer = 0;

        pHandle->pOutBuffer =
            (unsigned char *) malloc(pHandle->sizeOfOutBuffer);

        pHandle->posOfOutBuffer = 0;

        if(0 == pHandle->pOutBuffer)
        {

            return -1;

        }

    }

    else
    {

        pHandle = (TSMUXER_HANDLE *) (*tsmux_handle);

    }

    pHandle->aCounter =
        pHandle->vCounter = pHandle->patCounter = pHandle->pmtCounter = 0;

    return 0;

}

int
mpegts_buf_reset(void *tsmux_handle)
{

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    pHandle->posOfOutBuffer = 0;

    return 0;

}

int
mpegts_reset(void *tsmux_handle)
{

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    pHandle->aCounter =
        pHandle->vCounter =
        pHandle->patCounter =
        pHandle->pmtCounter = pHandle->posOfOutBuffer = 0;

    return 0;

}

int
mpegts_close(void *tsmux_handle)
{

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    if(pHandle)
    {

        if(pHandle->pOutBuffer)
        {

            free(pHandle->pOutBuffer);

            pHandle->pOutBuffer = 0;

            pHandle->posOfOutBuffer = 0;

            pHandle->sizeOfOutBuffer = 0;

        }

        free(pHandle);

        pHandle = 0;

    }

    return 0;

}

int
mpegts_write_pat(void *tsmux_handle)
{

    int i, bytes, packet_count;

    char buffer[1024], *pointer;

    unsigned int crc = 0xffffffff;

    TSMUXER_HANDLE *pHandle;

    TS_PACKET tsPacket;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    tsPacket.sync_byte = 0x47;

    tsPacket.transport_error_indicator = 0;

    tsPacket.payload_unit_start_indicator = 0x1;

    tsPacket.transport_priority = 0;

    tsPacket.PID = 0x0; //PAT pid

    tsPacket.transport_scrambling_control = 0x0;

    tsPacket.adaptation_field_control = 0x01;

    tsPacket.continuity_counter = pHandle->patCounter & 0x0f;

    ts_packet_buffer(pHandle, &tsPacket);

    pat_buffer(pHandle, &pHandle->patSectionHeader);

    pat_entry_buffer(pHandle);

    memcpy(buffer, &pHandle->patbuffer, 8);

    memcpy(&buffer[8], pHandle->pat_table_buffer,
           (pHandle->patSectionHeader.pat_entry_number) * sizeof(int));

    bytes = 8 + (pHandle->patSectionHeader.pat_entry_number) * sizeof(int);

    for(i = 0; i < bytes; i++)

        crc = (crc << 8) ^ crc_table[((crc >> 24) ^ buffer[i]) & 0xff];

    buffer[bytes + 0] = crc >> 24;

    buffer[bytes + 1] = crc >> 16;

    buffer[bytes + 2] = crc >> 8;

    buffer[bytes + 3] = crc & 0xff;

    bytes += 4;

    pointer = buffer;

    packet_count = 0;

    while(bytes > 0)
    {

        if(tsPacket.payload_unit_start_indicator == 1)
        {

            pHandle->tsbuffer[4] = 0x0; //pointer_field

            if(bytes < 183)
            {

                memcpy(&pHandle->tsbuffer[5], pointer, bytes);

                for(i = bytes + 5; i < 188; i++)
                {

                    pHandle->tsbuffer[i] = 0xff;

                }

                mpegts_write_buffer(pHandle, (char *) pHandle->tsbuffer, 188);

                pHandle->patCounter++;

                packet_count++;

                break;

            }

            else
            {

                memcpy(&pHandle->tsbuffer[5], pointer, 183);

                mpegts_write_buffer(pHandle, (char *) pHandle->tsbuffer, 188);

                bytes -= 183;

                pointer += 183;

                tsPacket.payload_unit_start_indicator = 0x0;

                pHandle->patCounter++;

                packet_count++;

                tsPacket.continuity_counter = pHandle->patCounter & 0x0f;

                ts_packet_buffer(pHandle, &tsPacket);

            }
        }

        else
        {

            if(bytes < 184)
            {

                memcpy(&pHandle->tsbuffer[4], pointer, bytes);

                for(i = 4 + bytes; i < 188; i++)
                {

                    pHandle->tsbuffer[i] = 0xff;

                }

                mpegts_write_buffer(pHandle, (char *) pHandle->tsbuffer, 188);

                pHandle->patCounter++;

                packet_count++;

                break;

            }

            else
            {

                memcpy(&pHandle->tsbuffer[4], buffer, 184);

                mpegts_write_buffer(pHandle, (char *) pHandle->tsbuffer, 188);

                bytes -= 184;

                pointer += 184;

                tsPacket.payload_unit_start_indicator = 0x0;

                pHandle->patCounter++;

                packet_count++;

                tsPacket.continuity_counter = pHandle->patCounter & 0x0f;

                ts_packet_buffer(pHandle, &tsPacket);

            }
        }
    }
    return packet_count;

}

int
mpegts_write_pes(void *tsmux_handle, char *payload, int payload_size,
                 int stream_id, int keyframe)
{

    int i, packet_count;

    unsigned int bytes;

    unsigned char *buffer;

    TS_PACKET_EXTENTION tsPacketHeader;

    TS_PACKET tsPacket;

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    tsPacketHeader.sync_byte = 0x47;

    tsPacketHeader.transport_error_indicator = 0;

    tsPacketHeader.payload_unit_start_indicator = 0x1;

    tsPacketHeader.transport_priority = 0;

    tsPacketHeader.PID = VIDEO_PACKET_PID;

    tsPacketHeader.transport_scrambling_control = 0x0;

    tsPacketHeader.adaptation_field_control = 0x3;  //for TS stuffing

    tsPacketHeader.discontinuity_indicator = 0x0;

    tsPacketHeader.random_access_indicator = 0x0;

    tsPacketHeader.elementary_stream_priority_indicator = 0x0;

    tsPacketHeader.PCR_flag = 0x0;

    tsPacketHeader.OPCR_flag = 0x0;

    tsPacketHeader.splicing_point_flag = 0x0;

    tsPacketHeader.transport_private_data_flag = 0x0;

    tsPacketHeader.adaptation_field_extension_flag = 0x0;

    tsPacket.sync_byte = 0x47;

    tsPacket.transport_error_indicator = 0;

    tsPacket.payload_unit_start_indicator = 0x1;

    tsPacket.transport_priority = 0;

    tsPacket.PID = VIDEO_PACKET_PID;

    tsPacket.transport_scrambling_control = 0x0;

    tsPacket.adaptation_field_control = 0x01;

    tsPacket.continuity_counter = 0;

    packet_count = 0;

    buffer = (unsigned char *) payload;

    bytes = payload_size;

    while(bytes > 0)
    {

        if(bytes <= 183)
        {

            tsPacketHeader.adaptation_field_length = 183 - bytes;

            for(i = 6; i < (int) (188 - bytes); i++)
            {

                pHandle->tsbuffer[i] = 0xff;

            }

            memcpy(&pHandle->tsbuffer[188 - bytes], buffer, bytes);

            if(VIDEO_STREAM_ID == stream_id)
            {

                tsPacketHeader.continuity_counter = pHandle->vCounter & 0x0f;

                tsPacketHeader.PID = VIDEO_PACKET_PID;

                pHandle->vCounter++;

            }

            else if(AUDIO_STREAM_ID == stream_id)
            {

                tsPacketHeader.continuity_counter = pHandle->aCounter & 0x0f;

                tsPacketHeader.PID = AUDIO_PACKET_PID;

                pHandle->aCounter++;

            }

            ts_packet_extention_buffer(pHandle, &tsPacketHeader);

            mpegts_write_buffer(pHandle, (char *) &pHandle->tsbuffer[0], 188);

            packet_count++;

            break;

        }

        else
        {

            if(VIDEO_STREAM_ID == stream_id)
            {

                tsPacket.continuity_counter = pHandle->vCounter & 0x0f;

                tsPacket.PID = VIDEO_PACKET_PID;

                pHandle->vCounter++;

            }

            else if(AUDIO_STREAM_ID == stream_id)
            {

                tsPacket.continuity_counter = pHandle->aCounter & 0x0f;

                tsPacket.PID = AUDIO_PACKET_PID;

                pHandle->aCounter++;

            }

            ts_packet_buffer(pHandle, &tsPacket);

            memcpy(&pHandle->tsbuffer[4], buffer, 184);

            mpegts_write_buffer(pHandle, (char *) &pHandle->tsbuffer[0], 188);

            bytes -= 184;

            buffer += 184;

            tsPacket.payload_unit_start_indicator = 0x0;

            tsPacketHeader.payload_unit_start_indicator = 0x0;

            packet_count++;

        }
    }
    return packet_count;

}

int
mpegts_write_pes_with_pcr(void *tsmux_handle, char *payload, int payload_size,
                          int stream_id, int keyframe, uint64_t pcr_base,
                          unsigned int pcr_extention)
{

    int i, packet_count;

    unsigned int bytes;

    unsigned char *buffer;

    TS_PACKET_EXTENTION tsPacketHeader;

    TS_PACKET tsPacket;

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    tsPacketHeader.sync_byte = 0x47;

    tsPacketHeader.transport_error_indicator = 0;

    tsPacketHeader.payload_unit_start_indicator = 0x1;

    tsPacketHeader.transport_priority = 0;

    tsPacketHeader.PID = VIDEO_PACKET_PID;

    tsPacketHeader.transport_scrambling_control = 0x0;

    tsPacketHeader.adaptation_field_control = 0x3;

    tsPacketHeader.discontinuity_indicator = 0x0;

    tsPacketHeader.random_access_indicator = 0x0;

    tsPacketHeader.elementary_stream_priority_indicator = 0x0;

    tsPacketHeader.PCR_flag = 0x1;  //contains PCR

    tsPacketHeader.OPCR_flag = 0x0;

    tsPacketHeader.splicing_point_flag = 0x0;

    tsPacketHeader.transport_private_data_flag = 0x0;

    tsPacketHeader.adaptation_field_extension_flag = 0x0;

    pHandle->tsbuffer[6] = (unsigned char) (pcr_base >> 25);

    pHandle->tsbuffer[7] = (unsigned char) ((pcr_base >> 17) & 0xff);

    pHandle->tsbuffer[8] = (unsigned char) ((pcr_base >> 9) & 0xff);

    pHandle->tsbuffer[9] = (unsigned char) ((pcr_base >> 1) & 0xff);

    pHandle->tsbuffer[10] = 0;

    pHandle->tsbuffer[10] =
        (unsigned char) ((((pcr_base & 0x1) << 7) | 0x7e) |
                         (pcr_extention >> 8));

    pHandle->tsbuffer[11] = (unsigned char) (pcr_extention & 0xff);

    tsPacket.sync_byte = 0x47;

    tsPacket.transport_error_indicator = 0;

    tsPacket.payload_unit_start_indicator = 0x1;

    tsPacket.transport_priority = 0;

    tsPacket.PID = VIDEO_PACKET_PID;

    tsPacket.transport_scrambling_control = 0x0;

    tsPacket.adaptation_field_control = 0x01;

    tsPacket.continuity_counter = 0;

    packet_count = 0;

    buffer = (unsigned char *) payload;

    bytes = payload_size;

    while(bytes > 0)
    {

        if(tsPacketHeader.payload_unit_start_indicator)
        {

            if(bytes <= 188 - 12)
            {

                tsPacketHeader.adaptation_field_length = 183 - bytes;

                for(i = 12; i < (int) (188 - bytes); i++)
                {

                    pHandle->tsbuffer[i] = 0xff;

                }

                memcpy(&pHandle->tsbuffer[188 - bytes], buffer, bytes);

                if(VIDEO_STREAM_ID == stream_id)
                {

                    tsPacketHeader.continuity_counter =
                        pHandle->vCounter & 0x0f;

                    tsPacketHeader.PID = VIDEO_PACKET_PID;

                    pHandle->vCounter++;

                }

                else if(AUDIO_STREAM_ID == stream_id)
                {

                    tsPacketHeader.continuity_counter =
                        pHandle->aCounter & 0x0f;

                    tsPacketHeader.PID = AUDIO_PACKET_PID;

                    pHandle->aCounter++;

                }

                ts_packet_extention_buffer(pHandle, &tsPacketHeader);

                mpegts_write_buffer(pHandle, (char *) &pHandle->tsbuffer[0],
                                    188);

                packet_count++;

                break;

            }

            else
            {

                tsPacketHeader.adaptation_field_length = 7;

                memcpy(&pHandle->tsbuffer[12], buffer, (188 - 12));

                if(VIDEO_STREAM_ID == stream_id)
                {

                    tsPacketHeader.continuity_counter =
                        pHandle->vCounter & 0x0f;

                    tsPacketHeader.PID = VIDEO_PACKET_PID;

                    pHandle->vCounter++;

                }

                else if(AUDIO_STREAM_ID == stream_id)
                {

                    tsPacketHeader.continuity_counter =
                        pHandle->aCounter & 0x0f;

                    tsPacketHeader.PID = AUDIO_PACKET_PID;

                    pHandle->aCounter++;

                }

                ts_packet_extention_buffer(pHandle, &tsPacketHeader);

                mpegts_write_buffer(pHandle, (char *) &pHandle->tsbuffer[0],
                                    188);

                packet_count++;

                bytes -= (188 - 12);

                buffer += (188 - 12);

                tsPacketHeader.payload_unit_start_indicator = 0x0;

                tsPacket.payload_unit_start_indicator = 0x0;

                tsPacketHeader.PCR_flag = 0x0;

            }
        }

        else
        {

            if(bytes <= 183)
            {

                tsPacketHeader.adaptation_field_length = 183 - bytes;

                for(i = 6; i < (int) (188 - bytes); i++)
                {

                    pHandle->tsbuffer[i] = 0xff;

                }

                memcpy(&pHandle->tsbuffer[188 - bytes], buffer, bytes);

                if(VIDEO_STREAM_ID == stream_id)
                {

                    tsPacketHeader.continuity_counter =
                        pHandle->vCounter & 0x0f;

                    tsPacketHeader.PID = VIDEO_PACKET_PID;

                    pHandle->vCounter++;

                }

                else if(AUDIO_STREAM_ID == stream_id)
                {

                    tsPacketHeader.continuity_counter =
                        pHandle->aCounter & 0x0f;

                    tsPacketHeader.PID = AUDIO_PACKET_PID;

                    pHandle->aCounter++;

                }

                ts_packet_extention_buffer(pHandle, &tsPacketHeader);

                mpegts_write_buffer(pHandle, (char *) &pHandle->tsbuffer[0],
                                    188);

                packet_count++;

                break;

            }

            else
            {

                if(VIDEO_STREAM_ID == stream_id)
                {

                    tsPacket.continuity_counter = pHandle->vCounter & 0x0f;

                    tsPacket.PID = VIDEO_PACKET_PID;

                    pHandle->vCounter++;

                }

                else if(AUDIO_STREAM_ID == stream_id)
                {

                    tsPacket.continuity_counter = pHandle->aCounter & 0x0f;

                    tsPacket.PID = AUDIO_PACKET_PID;

                    pHandle->aCounter++;

                }

                ts_packet_buffer(pHandle, &tsPacket);

                memcpy(&pHandle->tsbuffer[4], buffer, 184);

                mpegts_write_buffer(pHandle, (char *) &pHandle->tsbuffer[0],
                                    188);

                bytes -= 184;

                buffer += 184;

                tsPacket.payload_unit_start_indicator = 0x0;

                tsPacketHeader.payload_unit_start_indicator = 0x0;

                packet_count++;

            }
        }
    }
    return packet_count;

}

static int
stream_descriptor_buffer(void *tsmux_handle, STREAM_DESCRIPTOR * header)
{

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    pHandle->streamheader[0] = (char) (header->stream_type);

    pHandle->streamheader[1] = 0;

    pHandle->streamheader[1] |= header->reserved1 << 5;

    pHandle->streamheader[1] |= header->elementary_PID >> 8;

    pHandle->streamheader[2] = header->elementary_PID & 0xff;

    pHandle->streamheader[3] = 0;

    pHandle->streamheader[3] |= header->reserved2 << 4;

    pHandle->streamheader[3] |= header->ES_info_length >> 8;

    pHandle->streamheader[4] = header->ES_info_length & 0xff;

    return 0;

}

int
mpegts_write_pmt(void *tsmux_handle)
{

    int i, packet_count, pointer = 0, bytes;

    char *buffer;

    unsigned int crc = 0xffffffff;

    char buf[1024];

    TS_PACKET tsPacket;

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    tsPacket.sync_byte = 0x47;

    tsPacket.transport_error_indicator = 0;

    tsPacket.payload_unit_start_indicator = 0x1;

    tsPacket.transport_priority = 0;

    tsPacket.PID = PMT_PACKET_PID;

    tsPacket.transport_scrambling_control = 0x0;

    tsPacket.adaptation_field_control = 0x01;

    tsPacket.continuity_counter = pHandle->pmtCounter & 0x0f;

    ts_packet_buffer(pHandle, &tsPacket);

    pmt_buffer(pHandle, &pHandle->pmtSection);

    buf[pointer] = 0x0; //pointer_field

    pointer++;

    memcpy(&buf[pointer], &pHandle->pmtbuffer, 12);

    pointer += 12;

    memcpy(&buf[pointer], &pHandle->programe_descp_buffer[0],
           pHandle->pmtSection.program_info_length);

    pointer += pHandle->pmtSection.program_info_length;

    for(i = 0; i < pHandle->pmtSection.stream_number; i++)
    {

        stream_descriptor_buffer(pHandle, &pHandle->stream_descriptor[i]);

        memcpy(&buf[pointer], pHandle->streamheader, 5);

        pointer += 5;

        memcpy(&buf[pointer], pHandle->stream_descriptor[i].descriptor,
               pHandle->stream_descriptor[i].ES_info_length);

        pointer += pHandle->stream_descriptor[i].ES_info_length;

    }

    for(i = 0; i < pHandle->pmtSection.section_length + 3 - 4; i++)

        crc = (crc << 8) ^ crc_table[((crc >> 24) ^ buf[i + 1]) & 0xff];

    buf[pointer + 0] = crc >> 24;

    buf[pointer + 1] = crc >> 16;

    buf[pointer + 2] = crc >> 8;

    buf[pointer + 3] = crc & 0xff;

    bytes = pHandle->pmtSection.section_length + 3 + 1;

    buffer = &buf[0];

    packet_count = 0;

    while(bytes > 0)
    {

        if(tsPacket.payload_unit_start_indicator)
        {

            if(bytes < 184)
            {

                memcpy(&pHandle->tsbuffer[4], buffer, bytes);

                for(i = bytes + 4; i < 188; i++)
                {

                    pHandle->tsbuffer[i] = 0xff;

                }

                mpegts_write_buffer(pHandle, (char *) pHandle->tsbuffer, 188);

                pHandle->pmtCounter++;

                packet_count++;

                break;

            }

            else
            {

                memcpy(&pHandle->tsbuffer[4], buffer, 184);

                mpegts_write_buffer(pHandle, (char *) &pHandle->tsbuffer,
                                    188);

                bytes -= 184;

                buffer += 184;

                tsPacket.payload_unit_start_indicator = 0x0;

                pHandle->pmtCounter++;

                packet_count++;

                tsPacket.continuity_counter = pHandle->pmtCounter & 0x0f;

                ts_packet_buffer(pHandle, &tsPacket);

            }
        }

        else
        {

            if(bytes < 185)
            {

                memcpy(&pHandle->tsbuffer[4], &buffer[1], (bytes - 1));

                for(i = bytes + 3; i < 188; i++)
                {

                    pHandle->tsbuffer[i] = 0xff;

                }

                mpegts_write_buffer(pHandle, (char *) pHandle->tsbuffer, 188);

                pHandle->pmtCounter++;

                packet_count++;

                break;

            }

            else
            {

                memcpy(&pHandle->tsbuffer[4], &buffer[1], 184);

                mpegts_write_buffer(pHandle, (char *) &pHandle->tsbuffer,
                                    188);

                bytes -= 184;

                buffer += 184;

                pHandle->pmtCounter++;

                packet_count++;

                tsPacket.continuity_counter = pHandle->pmtCounter & 0x0f;

                ts_packet_buffer(pHandle, &tsPacket);

            }
        }
    }

    return packet_count;

}

static int
mpegts_write_dummy_with_PID(void *tsmux_handle, int PID)
{

    TS_PACKET_EXTENTION tsPacketHeader;

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    tsPacketHeader.sync_byte = 0x47;

    tsPacketHeader.transport_error_indicator = 0;

    tsPacketHeader.payload_unit_start_indicator = 0x0;

    tsPacketHeader.transport_priority = 0;

    tsPacketHeader.PID = PID & 0x1fff;

    tsPacketHeader.transport_scrambling_control = 0x0;

    tsPacketHeader.adaptation_field_control = 0x3;  //for TS stuffing

    tsPacketHeader.discontinuity_indicator = 0x0;

    tsPacketHeader.random_access_indicator = 0x0;

    tsPacketHeader.elementary_stream_priority_indicator = 0x0;

    tsPacketHeader.PCR_flag = 0x0;

    tsPacketHeader.OPCR_flag = 0x0;

    tsPacketHeader.splicing_point_flag = 0x0;

    tsPacketHeader.transport_private_data_flag = 0x0;

    tsPacketHeader.adaptation_field_extension_flag = 0x0;

    tsPacketHeader.adaptation_field_length = 183;

    memset(&pHandle->tsbuffer[6], 0xff, 182);

    if(PID == VIDEO_PACKET_PID)
    {

        tsPacketHeader.continuity_counter = pHandle->vCounter & 0x0f;

        pHandle->vCounter++;

    }

    else if(PID == AUDIO_PACKET_PID)
    {

        tsPacketHeader.continuity_counter = pHandle->aCounter & 0x0f;

        pHandle->aCounter++;

    }

    else

        tsPacketHeader.continuity_counter = 0;

    ts_packet_extention_buffer(pHandle, &tsPacketHeader);

    mpegts_write_buffer(pHandle, (char *) &pHandle->tsbuffer[0], 188);

    return 1;

}

int
mpegts_write_dummy(void *tsmux_handle)
{

    TSMUXER_HANDLE *pHandle;

    int dummy_vid, dummy_aud, i;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    dummy_vid = (0x10 - (pHandle->vCounter & 0x0f)) & 0x0f;

    dummy_aud = (0x10 - (pHandle->aCounter & 0x0f)) & 0x0f;

    if(dummy_vid > 0)
    {

        for(i = 0; i < dummy_vid; i++)

            mpegts_write_dummy_with_PID(pHandle, VIDEO_PACKET_PID);

    }

    if(dummy_aud > 0)
    {

        for(i = 0; i < dummy_aud; i++)

            mpegts_write_dummy_with_PID(pHandle, AUDIO_PACKET_PID);

    }

    return 1;

}

int
mpegts_get_buffer(void *tsmux_handle, unsigned char **outputBuffer)
{

    TSMUXER_HANDLE *pHandle;

    pHandle = (TSMUXER_HANDLE *) tsmux_handle;

    *outputBuffer = pHandle->pOutBuffer;

    return pHandle->posOfOutBuffer;

}
