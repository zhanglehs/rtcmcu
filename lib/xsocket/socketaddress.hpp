#ifndef _BASE_NET_SOCKETADDRESS_H_
#define _BASE_NET_SOCKETADDRESS_H_

#include <string>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;


class Net;

class SocketAddress
{

};


class InetSocketAddress: public SocketAddress
{
private:
    bool _valid;

    unsigned int _address;
    int _family;
    int _port;

public:
    InetSocketAddress()
            :_valid(false)
    {
    }

    InetSocketAddress(int port)
    {
        _port = port;
        _address = INADDR_ANY;

        _valid = true;
    }

    InetSocketAddress(int addr, int port)
    {
        _port = port;
        if (addr == 0)
        {
            _address = INADDR_ANY;
        }
        else
        {
            _address = addr;
        }

        _valid = true;
    }


    InetSocketAddress(string& hostname, int port)
    {
        _address = ntohl(::inet_addr(hostname.c_str()));
        _port = port;
        _valid = true;
    }

    InetSocketAddress(const char* hostname, int port)
    {
        if(hostname == NULL)
        {
           _address = INADDR_ANY;
        }
        else
        {
            _address = ntohl(::inet_addr(hostname));
        }
        
        _port = port;
        _valid = true;
    }

    bool is_valid()
    {
        return _valid;
    }

    int get_port()
    {
        return _port;
    }

    unsigned int get_address()
    {
        return _address;
    }

    const char* get_dot_address()
    {
        in_addr in4_addr;
        in4_addr.s_addr = htonl(_address);     
        return inet_ntoa(in4_addr);
    }
};

class UNSocketAddress: public SocketAddress
{
public:
    static const int MAX_UN_SOCKET_ADDR_LEN = 1024;
    UNSocketAddress()
    :_valid(false)
    {
    }

    UNSocketAddress(const char* un_path)
    {
        snprintf(_un_path, MAX_UN_SOCKET_ADDR_LEN, "%s", un_path);
        _valid = true;
    }

    bool is_valid()
    {
        return _valid;
    }

    const char* get_address()
    {
        return _un_path;
    }

private:
    bool _valid;
    char  _un_path[MAX_UN_SOCKET_ADDR_LEN];
    int     _family;
};

#endif

