#include "server_controller.h"

using namespace std;
server_controller::server_controller(vector<server_info> &shards)
    : ketama_hash(shards)
{
}

server_info server_controller::get_server_info(string key)
{
    // keep the key 32 bits long. if less than, padding zero on the left.
    if (key.length() < 32) {
        const string tmp("00000000000000000000000000000000");
        key = tmp + key;
        key = key.substr(key.length() - 32, key.length() - 1);
    }
    else {
        key = key.substr(0, 31);
    }

    return ketama_hash::get_server_info(key);
}

server_controller::~server_controller()
{
}
