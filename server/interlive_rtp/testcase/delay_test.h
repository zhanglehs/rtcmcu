#ifndef __DELAY_TEST_H_
#define __DELAY_TEST_H_

#include "stdint.h"
#include "../util/flv.h"
#include "streamid.h"
#include <string>

//#define DELAY_TEST 1

void print_delay_log(StreamId_Ext streamid, flv_tag* tag, std::string module);

#endif /* __DELAY_TEST_H_ */
