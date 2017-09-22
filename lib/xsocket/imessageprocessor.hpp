#ifndef _IMESSAGE_PROCESSOR_HPP_
#define _IMESSAGE_PROCESSOR_HPP_

#include <ext/hash_map>
#include <ext/hash_set>

using namespace __gnu_cxx;

class ConnectionInfo;
class ByteBuffer;


class IMessageProcessor
{
public:
    IMessageProcessor(){}
    virtual ~IMessageProcessor(){}
    virtual void accept(ConnectionInfo& conn) = 0;
    virtual void remove(ConnectionInfo& conn) = 0;

    virtual void process_input(ConnectionInfo& conn, ByteBuffer& bb) = 0;
    virtual void process_output() = 0;
    virtual int frame(ByteBuffer& bb)=0;
};


namespace __gnu_cxx
{
template<> 
struct hash<IMessageProcessor*>
{
    size_t operator()(const IMessageProcessor* __s) const
        { return  (size_t)__s;  }



};
}

#endif

