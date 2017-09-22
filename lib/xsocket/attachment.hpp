#ifndef _BASE_NET_ATTACHMENT_H_
#define _BASE_NET_ATTACHMENT_H_
#include "selectionkey.hpp"

class Attachment
{
public:
    virtual ~Attachment(){}
    virtual  int run(SelectionKey& key)=0;

};

#endif

