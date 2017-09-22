/*************************************************
 * YueHonghui, 2013-05-16
 * hhyue@tudou.com
 * copyright:youku.com
 * ***********************************************/
#ifndef HASHTABLE_H_
#define HASHTABLE_H_
#include "common.h"
#include "rbtree.h"

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif
typedef struct hashnode
{
    unsigned long key;
    struct rb_node entry;
} hashnode;

typedef struct hashtable
{
    unsigned int width;
    struct rb_root hashentry[0];
} hashtable;

//power < 32
hashtable *hash_create(unsigned int power);

//@return
//  0 : insert success
//  -1: key already exist
int hash_insert(hashtable * h, unsigned long key, hashnode * value);

hashnode *hash_get(hashtable * h, unsigned long key);

hashnode *hash_delete(hashtable * h, unsigned long key);

void hash_delete_node(hashtable * h, hashnode * node);

typedef void (*hashnode_handler) (hashnode *);
typedef void (*hashnode_handler_ex) (hashnode *, void *);
void hash_destroy(hashtable * h, hashnode_handler handler);

typedef struct hash_iterator
{
    unsigned int i;
    struct rb_node *next;
} hash_iterator;

void hash_iterator_init(hashtable * h, hash_iterator * hi);

boolean hash_iterate(hashtable * h, hash_iterator * hi, hashnode ** node);

#define hash_for_each(h,type,member,handler)\
        {\
            unsigned int i = 0;\
            struct rb_node *pos = NULL;\
            struct rb_node *n = NULL;\
            for (i = 0; i < h->width; i++)\
            {\
                rb_for_each_safe (pos, n, &h->hashentry[i])\
                {\
                    handler(container_of(rb_entry (pos, hashnode, entry),type,member));\
                }\
            }\
        } \

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
