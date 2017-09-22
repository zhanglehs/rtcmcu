/*****************************************
 * YueHonghui, 2013-05-26
 * hhyue@tudou.com
 * copyright:youku.com
 * ***************************************/
#include "hashtable.h"
#include "common.h"
#include "rbtree.h"
#include "memory.h"
//@tree     the start node of this search
//@key      the key to match
//@parent   if success match, *entry will be ptr of matched node; if failed match, *entry will be ptr of the parent node.
//@left_right_node
//          if failed matched, *left_right_node will point to rb_left or rb_right of parent of the node. 
//          this would be useful when plan to insert node after failed matching.
//    
//@return   0 : success matched
//          1 : failed match
//          -1: error
static int
my_rb_search(struct rb_root *tree, unsigned int key,
             struct rb_node **parent, struct rb_node ***left_right_node)
{
    if(NULL == parent)
        return -1;

    int iret = 0;
    struct rb_node **p;

    *parent = NULL;
    p = &(tree->rb_node);
    hashnode *tmp = NULL;

    while(NULL != *p) {
        *parent = *p;
        tmp = rb_entry(*p, hashnode, entry);

        iret = key - tmp->key;
        if(iret > 0) {
            p = &(*p)->rb_right;
        }
        else if(iret < 0) {
            p = &(*p)->rb_left;
        }
        else {
            return 0;
        }
    }

    if(NULL != left_right_node)
        *left_right_node = p;
    return 1;
}

#define HASH_MODE(base, value) (value & (base - 1))
#define HASHSIZE(width) (sizeof(hashtable) + width * sizeof(hashnode))

hashtable *
hash_create(unsigned int power)
{
    if(power > 31)
        return NULL;
    unsigned int width = 1u << power;
    hashtable *h = (hashtable *) mmalloc(HASHSIZE(width));

    if(NULL == h)
        return NULL;

    h->width = width;
    unsigned int i = 0;

    for(; i < h->width; i++) {
        rb_init_root(&(h->hashentry[i]));
    }

    return h;
}

int
hash_insert(hashtable * h, unsigned long key, hashnode * value)
{
    struct rb_node *parent = NULL;
    struct rb_node **left_right_node = NULL;

    value->key = key;
    struct rb_root *root = &(h->hashentry[HASH_MODE(h->width, key)]);
    int ret = my_rb_search(root, key, &parent, &left_right_node);

    if(1 != ret)
        return -1;

    rb_link_node(&(value->entry), parent, left_right_node);
    rb_insert_color(&(value->entry), root);
    return 0;
}

hashnode *
hash_get(hashtable * h, unsigned long key)
{
    struct rb_node *parent = NULL;
    struct rb_node **left_right_node = NULL;
    struct rb_root *root = &(h->hashentry[HASH_MODE(h->width, key)]);
    int ret = my_rb_search(root, key, &parent, &left_right_node);

    if(0 != ret)
        return NULL;

    return rb_entry(parent, hashnode, entry);
}

hashnode *
hash_delete(hashtable * h, unsigned long key)
{
    struct rb_root *root = &(h->hashentry[HASH_MODE(h->width, key)]);
    struct rb_node **left_right_node = NULL;
    struct rb_node *parent = NULL;
    int ret = my_rb_search(root, key, &parent, &left_right_node);

    if(0 != ret)
        return NULL;

    rb_erase(parent, root);
    return rb_entry(parent, hashnode, entry);
}

void
hash_delete_node(hashtable * h, hashnode * node)
{
    struct rb_root *root = &(h->hashentry[HASH_MODE(h->width, node->key)]);

    rb_erase(&node->entry, root);
}

void
hash_destroy(hashtable * h, hashnode_handler handler)
{
    unsigned int i = 0;
    struct rb_node *pos = NULL;
    struct rb_node *n = NULL;

    for(i = 0; i < h->width; i++) {
        rb_for_each_safe(pos, n, &h->hashentry[i]) {
            rb_erase(pos, &h->hashentry[i]);
            if(handler) {
                handler(rb_entry(pos, hashnode, entry));
            }
        }
    }
}

void
hash_destroy_ex(hashtable * h, hashnode_handler_ex handler, void *arg)
{
    unsigned int i = 0;
    struct rb_node *pos = NULL;
    struct rb_node *n = NULL;

    for(i = 0; i < h->width; i++) {
        rb_for_each_safe(pos, n, &h->hashentry[i]) {
            rb_erase(pos, &h->hashentry[i]);
            if(handler) {
                handler(rb_entry(pos, hashnode, entry), arg);
            }
        }
    }
}

void
hash_iterator_init(hashtable * h, hash_iterator * hi)
{
    unsigned int i = 0;

    for(i = 0; i < h->width; i++) {
        hi->next = rb_first(&h->hashentry[i]);
        if(NULL != hi->next) {
            hi->i = i;
            return;
        }
    }
}

boolean
hash_iterate(hashtable * h, hash_iterator * hi, hashnode ** node)
{
    if(NULL == hi->next)
        return FALSE;

    unsigned int i = 0;

    *node = container_of(hi->next, hashnode, entry);
    hi->next = rb_next(hi->next);
    if(NULL != hi->next)
        return TRUE;

    for(i = hi->i + 1; i < h->width; i++) {
        hi->next = rb_first(&h->hashentry[i]);
        if(NULL != hi->next) {
            hi->i = i;
            return TRUE;
        }
    }
    hi->next = NULL;
    return TRUE;
}
