#ifndef _BASE_NET_SELECTORPROVIDER_H_
#define _BASE_NET_SELECTORPROVIDER_H_

class ServerSocketChannel;
class Selector;
class PollSelector;
class EPollSelector;
class SocketChannel;
class SelectorProvider;
class SelectorProvider
{
private:
    static SelectorProvider* _provider;
    SelectorProvider()
    {
    }
public:
    static SelectorProvider& provider()
    {
        if (_provider == NULL)
        {
            _provider = new SelectorProvider();
        }
        return *_provider;
    }


     ServerSocketChannel* openServerSocketChannel();
     SocketChannel* openSocketChannel();
     Selector* open_selector();
     Selector* open_selector(const char* selector_name);
};

#endif

