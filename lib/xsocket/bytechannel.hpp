#ifndef _BYTE_CHANNEL_HPP_
#define _BYTE_CHANNEL_HPP_
#include "bytebuffer.hpp"

class ByteChannel
{
public:
    ByteChannel(){}
    virtual ~ByteChannel() {}
    virtual int read(ByteBuffer& bb) = 0;
    virtual int write(ByteBuffer& bb) = 0;
};

#endif

