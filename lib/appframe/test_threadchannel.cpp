#include <iostream>
#include "thread_channel.hpp"

using namespace std;

typedef ThreadChannel<int>  IntThreadChannel;

int main()
{
    IntThreadChannel::Accessor server, client ;

    IntThreadChannel channel(128);
    server = channel.bind();
    client = channel.connect();
    int a=1;

    server.send(1);
    server.send(2);
    server.send(3);
    server.send(4);

    IntThreadChannel::value_type value;
    client.recv(value);
    cout<<value<<endl;

    client.recv(value);
    cout<<value<<endl;
    client.recv(value);
    cout<<value<<endl;

    client.recv(value);
    cout<<value<<endl;
}

