#ifndef TARGET_H_
#define TARGET_H_
#include "streamid.h"

extern char g_public_ip[1][32];
extern int g_public_cnt;
extern char g_private_ip[1][32];
extern int g_private_cnt;
extern char g_ver_str[64];

//bool stream_in_whitelist(const StreamId_Ext& id);

//void* get_white_list();

#endif /* TARGET_H_ */
