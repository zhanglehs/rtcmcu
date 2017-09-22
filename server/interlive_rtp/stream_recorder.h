/*
 * stream_recorder.h
 *
 *  Created on: 2013-8-13
 *      Author: zzhang
 */  
    
#ifndef STREAM_RECORDER_H_
#define STREAM_RECORDER_H_
    
#include <stdint.h>
#include <event.h>
#if (defined __cplusplus && !defined _WIN32)
extern "C" 
{
#endif
int stream_recorder_init(const char *record_dir);
void stream_recorder_fini();
void start_record_stream(uint32_t streamid);
void stop_record_stream(uint32_t streamid);

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif  /* STREAM_RECORDER_H_ */
