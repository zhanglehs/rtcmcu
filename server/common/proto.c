#define _BSD_SOURCE
#include "proto.h"
#include "util/util.h"
#include <endian.h>
#include <arpa/inet.h>

const uint8_t VER_1 = 1; // v1 tracker protocol: with original forward_client/server
const uint8_t VER_2 = 2; // v2 tracker protocol: with module_tracker in forward
const uint8_t VER_3 = 3; // v3 tracker protocol: with module_tracker in forward, support long stream id
const uint8_t MAGIC_1 = 0xff;
const uint8_t MAGIC_2 = 0xf0;
const uint8_t MAGIC_3 = 0x0f;

void
encode_header(buffer * obuf, uint16_t cmd, uint32_t size)  //=
{
    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(size);

    buffer_append_ptr(obuf, &MAGIC_1, sizeof(uint8_t));
    buffer_append_ptr(obuf, &VER_1, sizeof(uint8_t));
    buffer_append_ptr(obuf, &ncmd, sizeof(uint16_t));
    buffer_append_ptr(obuf, &nsize, sizeof(uint32_t));
}

void encode_header_uint8(uint8_t* obuf, uint32_t& len, uint16_t cmd, uint32_t size) //=
{
    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(size);

    obuf[0] = MAGIC_1;
    obuf[1] = VER_1;
    memcpy(&obuf[2], &ncmd, 2);
    memcpy(&obuf[4], &nsize, 4);

    len = sizeof(proto_header);
}

int decode_header(const buffer * ibuf, proto_header * h) //=
{
    if (buffer_data_len(ibuf) < sizeof(proto_header))
        return -1;
    proto_header *ih = (proto_header *)buffer_data_ptr(ibuf);

    switch (ih->version)
    {
    case VER_1:
        break;

    case VER_2:
        if (ih->magic != MAGIC_2)
            return -1;
        break;

    case VER_3:
        if (ih->magic != MAGIC_3)
            return -1;
        break;

    default:
        return -1;
    }

    if (h != NULL)
    {
        h->magic = ih->magic;
        h->version = ih->version;
        h->cmd = ntohs(ih->cmd);
        h->size = ntohl(ih->size);
    }
    else
    {
        return -1;
    }

	return 0;
}
