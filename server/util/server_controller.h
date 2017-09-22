#ifndef _SERVER_CONTROLLER_H_
#define _SERVER_CONTROLLER_H_

#include "ketama_hash.h"
#include <vector>

//using namespace std;

class server_controller : public ketama_hash
{
public:
    server_controller(std::vector<server_info> &shards);
    ~server_controller();
    server_info get_server_info(std::string key);
};

#endif /* _SERVER_CONTROLLER_H_ */
