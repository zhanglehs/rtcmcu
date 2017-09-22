/*
 * Author: hechao@youku.com 
 *
 */
#ifndef __RTP_TYPE_
#define __RTP_TYPE_

typedef unsigned short rtp_seq_t;
enum rtp_type_t
{
  RTP_TYPE_SDP,
  RTP_TYPE_RTP_NACK,
  RTP_TYPE_RTP_NORMAL,
  RTP_TYPE_RTCP,
  RTP_TYPE_FEC
};

#endif

