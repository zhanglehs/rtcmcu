#include <string.h>
#include "selectablechannel.hpp"
//#include "datagramchannel.hpp"
#include "serversocketchannel.hpp"
#include "socketchannel.hpp"
///#include "unserversocketchannel.hpp"
//#include "unsocketchannel.hpp"
#include "pollselector.hpp"
#include "epollselector.hpp"
#include "selectorprovider.hpp"

SelectorProvider* SelectorProvider::_provider = NULL;

Selector* SelectorProvider::open_selector()
{
    return new PollSelector();
}

Selector* SelectorProvider::open_selector(const char* selector_name)
{
    if(strcmp(selector_name,"poll") == 0)
    {
        return new PollSelector();
    }
    else if(strcmp(selector_name,"epoll") == 0)
    {
        return new EPollSelector();
    }
    else
    {
        return NULL;
    }
}
    
ServerSocketChannel* SelectorProvider::openServerSocketChannel()
{
    return new ServerSocketChannel(this);
}

SocketChannel* SelectorProvider::openSocketChannel()
{
    return new SocketChannel(this);
}
/*
UNServerSocketChannel* SelectorProvider::openUNServerSocketChannel()
{
return new UNServerSocketChannel(this);
}

 UNSocketChannel* SelectorProvider::openUNSocketChannel()
{
return new UNSocketChannel(this);
}
DatagramChannel* SelectorProvider::openDatagramChannel()
{
return new DatagramChannel(this);
}
*/
