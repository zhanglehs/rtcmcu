/*************************************************
 * YueHonghui, 2013-06-25
 * hhyue@tudou.com
 * copyright:youku.com
 * **********************************************/
#ifndef LEVENT_H_
#define LEVENT_H_
#include <event.h>
#include <time.h>

//#if (defined __cplusplus && !defined _WIN32)
//extern "C"
//{
//#endif 
int levent_init();

int
levent_set(struct event *ev, int fd, short event,
           void (*fn) (int, short, void *), void *arg);

int levent_add(struct event *ev, struct timeval *tv);

int levent_del(struct event *ev);

int
levtimer_set(struct event *ev, void (*fn) (int, short, void *), void *arg);

void levtimer_add(struct event *ev, struct timeval *);

void levtimer_del(struct event *ev);

void levent_fini();

//#if (defined __cplusplus && !defined _WIN32)
//}
//#endif
#endif
