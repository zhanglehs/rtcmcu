#ifndef _XML_H
#define _XML_H

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif
struct xmlattribute
{
    char *name;
    char *value;
};

struct xmlnode
{
    char *name;
    char *value;
    struct xmlattribute **attr;
    struct xmlnode **child;
    struct xmlnode *parent;

    /* info */
    int nchild;                 /* number of child nodes */
    int nattr;                  /* attribute count */
};

int xmlnchild(struct xmlnode *, const char *);
char *xmlgetattr(struct xmlnode *, const char *);
struct xmlnode *xmlgetchild(struct xmlnode *, const char *, int);
void freexml(struct xmlnode *);
struct xmlnode *xmlloadfile(const char *);

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
