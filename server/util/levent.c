/*************************************************
 * YueHonghui, 2013-06-25
 * hhyue@tudou.com
 * copyright:youku.com
 * **********************************************/

#include "levent.h"
#include <event.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>
#include "utils/memory.h"
#include "common.h"
#include "hashtable.h"

static hashtable* g_table = NULL;

typedef enum
{
    LEV_NULL = 1,
    LEV_SETED = 2,
    LEV_ADDED = 3,
    LEV_TIMER_SETED = 4,
    LEV_TIMER_ADDED = 5,
} levent_state;

typedef struct levent
{
    levent_state lstate;
    struct event *ev;
    void (*fn) (int, short, void *);
    void *arg;
    hashnode entry;
} levent;

static levent *
find_exist_node(struct event *ev)
{
    hashnode* node = hash_get(g_table, (unsigned long)ev);
    if(NULL == node)
        return NULL;
    return container_of(node, levent, entry);
}

static void
g_timeout(int fd, short which, void *arg)
{
    assert(NULL != arg);
    levent *le = (levent *) arg;
    hash_delete_node(g_table, &le->entry);
    if(le->fn)
        le->fn(fd, which, le->arg);
    mfree(le);
}

int
levent_init()
{
    g_table = hash_create(10);
    if(NULL == g_table){
        return -1;
    }
    return 0;
}

int
levent_set(struct event *ev, int fd, short event,
           void (*fn) (int, short, void *), void *arg)
{
    assert(NULL != ev);
    levent *le = find_exist_node(ev);

    if(le) {
        assert(LEV_SETED == le->lstate);
    }else{
        le = (levent*)mmalloc(sizeof(levent));
        if(NULL == le)
            return -1;
        int ret = hash_insert(g_table, (unsigned long)ev, &le->entry);
        assert(0 == ret);
    }
    event_set(ev, fd, event, fn, arg);
    le->ev = ev;
    le->lstate = LEV_SETED;
    le->fn = NULL;
    le->arg = NULL;

    return 0;
}

int
levent_add(struct event *ev, struct timeval *tv)
{
    assert(NULL != ev);
    levent *le = find_exist_node(ev);

    assert(NULL != le);
    assert(LEV_SETED == le->lstate);
    int ret = event_add(ev, tv);

    if(0 == ret)
        le->lstate = LEV_ADDED;
    return ret;
}

int
levent_del(struct event *ev)
{
    assert(NULL != ev);
    levent *le = find_exist_node(ev);

    assert(NULL != le);
    assert(LEV_SETED == le->lstate || LEV_ADDED == le->lstate);
    hash_delete_node(g_table, &le->entry);
    mfree(le);
    return event_del(ev);
}

int
levtimer_set(struct event *ev, void (*fn) (int, short, void *), void *arg)
{
    assert(NULL != ev);
    levent *le = find_exist_node(ev);
    if(le){
        assert(LEV_TIMER_SETED == le->lstate);
    }else{
        le = (levent*)mmalloc(sizeof(levent));
        if(NULL == le)
            return -1;
        int ret = hash_insert(g_table, (unsigned long)ev, &(le->entry));
        assert(0 == ret);
    }
    le->fn = fn;
    le->arg = arg;
    le->ev = ev;
    evtimer_set(ev, g_timeout, le);
    le->lstate = LEV_TIMER_SETED;
    return 0;
}

void
levtimer_add(struct event *ev, struct timeval *t)
{
    assert(NULL != ev);
    levent *le = find_exist_node(ev);

    assert(NULL != le);
    assert(LEV_TIMER_SETED == le->lstate);
    le->lstate = LEV_TIMER_ADDED;
    evtimer_add(ev, t);
}

void
levtimer_del(struct event *ev)
{
    assert(NULL != ev);
    levent *le = find_exist_node(ev);

    assert(NULL != le);
    assert(LEV_TIMER_SETED == le->lstate || LEV_TIMER_ADDED == le->lstate);
    hash_delete_node(g_table, &le->entry);
    mfree(le);
    evtimer_del(ev);
}

static void
node_delete_handler(hashnode* node)
{
    levent* le = container_of(node, levent, entry);
    mfree(le);
}

void
levent_fini()
{
    hash_destroy(g_table, node_delete_handler);
    g_table = NULL;
}
