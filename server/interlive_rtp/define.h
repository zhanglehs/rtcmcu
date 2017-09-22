#ifndef DEFINE_H_
#define DEFINE_H_

#include "../../lib/version.h"

#if (defined __cplusplus && !defined _WIN32)
extern "C" 
{
#endif

//#define VERSION ("3.1.140911.1") 

//RTP related defines
const int MAX_RTP_CONTENT_LEN = 2048;

#define MAX_STREAM_CNT (20480)
#define DEFAULT_BPS (300)

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
