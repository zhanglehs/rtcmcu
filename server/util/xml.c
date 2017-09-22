/* simple xml parser based on expat */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <expat.h>

#include "xml.h"
#include "utils/memory.h"
#if defined(__amigaos__) && defined(__USE_INLINE__)
#include <proto/expat.h>
#endif

#ifdef XML_LARGE_SIZE
#if defined(XML_USE_MSC_EXTENSIONS) && _MSC_VER < 1400
#define XML_FMT_INT_MOD "I64"
#else
#define XML_FMT_INT_MOD "ll"
#endif
#else
#define XML_FMT_INT_MOD "l"
#endif

int
xmlnchild(struct xmlnode *xml, const char *name)
{
    struct xmlnode *x;
    int i, j = 0;

    if(xml == NULL || xml->nchild == 0 || name == NULL || name[0] == '\0')
        return 0;
    for(i = 0; i < xml->nchild; i++) {
        x = xml->child[i];
        if(x->name && strcmp(x->name, name) == 0)
            j++;
    }
    return j;
}

char *
xmlgetattr(struct xmlnode *xml, const char *name)
{
    int i;
    struct xmlattribute *xa;

    if(xml == NULL || xml->nattr == 0 || name == NULL || name[0] == '\0')
        return NULL;

    for(i = 0; i < xml->nattr; i++) {
        xa = xml->attr[i];
        if(xa->name && strcmp(xa->name, name) == 0)
            return xa->value;
    }
    return NULL;
}

struct xmlnode *
xmlgetchild(struct xmlnode *xml, const char *name, int num)
{
    struct xmlnode *x;
    int i, j = 0;

    if(xml == NULL || xml->nchild == 0 || name == NULL || name[0] == '\0')
        return 0;
    for(i = 0; i < xml->nchild; i++) {
        x = xml->child[i];
        if(x->name && strcmp(x->name, name) == 0) {
            if(num == j)
                return x;
            j++;
        }
    }
    return NULL;
}

/*
static void
printxml(struct xmlnode *xml, int depth)
{
	int i;
	struct xmlattribute *attr;

	if (xml == NULL) return;

	for (i = 0; i < depth; i++) printf(" ");
	if (xml->name) {
		if (xml->nattr) {
			printf("<%s", xml->name);
			for (i = 0; i < xml->nattr; i ++) {
				attr = xml->attr[i];
				printf(" %s=\"%s\"", attr->name, attr->value);
			}
			printf(">");
		} else {
			printf("<%s>", xml->name);
		}
	}

	if (xml->value)
		printf("%s", xml->value);
	else
		printf("\n");

	for (i = 0; i < xml->nchild; i ++) {
		printxml(xml->child[i], depth+1);
	}

	if (xml->name) {
		if (xml->nchild)
			for (i = 0; i < depth; i++) printf(" ");
		printf("</%s>\n", xml->name);
	}
}
*/
void
freexml(struct xmlnode *xml)
{
    int i;
    struct xmlattribute *attr;

    if(xml == NULL)
        return;

    for(i = 0; i < xml->nattr; i++) {
        attr = xml->attr[i];
        if(attr->name)
            free(attr->name);
        if(attr->value)
            free(attr->value);
        free(attr);
    }

    if(xml->attr)
        free(xml->attr);

    for(i = 0; i < xml->nchild; i++) {
        freexml(xml->child[i]);
    }

    free(xml->child);

    if(xml->name)
        free(xml->name);
    if(xml->value)
        free(xml->value);

    free(xml);

}

#define XML_ERR_MEMORY 1
#define XML_ERR_PARSER 2

static int xml_depth = 0, xml_error = 0, parent_depth = 0, max_parent_depth =
    0;
struct xmlnode *parser_node = NULL;
struct xmlnode **parser_parent = NULL;

static void XMLCALL
startElement(void *data, const char *name, const char **atts)
{
    int i, j;
    struct xmlattribute *xmlattr;

    /* starting parsing */
    if(xml_error)
        return;
    parser_node = (struct xmlnode *) mcalloc(sizeof(struct xmlnode), 1);
    if(parser_node == NULL) {
        printf("%s: %d\n", __FILE__, __LINE__);
        xml_error = XML_ERR_MEMORY;
        return;
    }
    parser_node->parent = parser_parent[xml_depth];
    /* update parser_parent child structure */
    if(parser_parent[xml_depth]->nchild) {
        /* add new child */
        parser_parent[xml_depth]->child =
            (struct xmlnode **) mrealloc((void *)
                                         parser_parent[xml_depth]->child,
                                         (parser_parent[xml_depth]->nchild +
                                          1) * sizeof(struct xmlnode *));
        if(parser_parent[xml_depth]->child == NULL) {
            printf("%s: %d\n", __FILE__, __LINE__);
            xml_error = XML_ERR_MEMORY;
            return;
        }
        parser_parent[xml_depth]->child[parser_parent[xml_depth]->nchild] =
            parser_node;
        parser_parent[xml_depth]->nchild++;
    }
    else {
        /* first child */
        parser_parent[xml_depth]->child =
            (struct xmlnode **) mcalloc(sizeof(struct xmlnode *), 1);
        if(parser_parent[xml_depth]->child == NULL) {
            xml_error = XML_ERR_MEMORY;
            printf("%s: %d\n", __FILE__, __LINE__);
            return;
        }
        parser_parent[xml_depth]->child[0] = parser_node;
        parser_parent[xml_depth]->nchild = 1;
    }

    xml_depth++;
    if(xml_depth == parent_depth) {
        if(parent_depth == max_parent_depth) {
            xml_error = XML_ERR_MEMORY;
            printf("%s: %d\n", __FILE__, __LINE__);
            return;
        }
        parent_depth++;
    }
    parser_parent[xml_depth] = parser_node;
    parser_node->name = strdup(name);

    /* get xml node's attributes */
    for(i = 0, j = 0; atts[i]; i += 2)
        j++;
    if(j) {
        parser_node->attr =
            (struct xmlattribute **) mcalloc(sizeof(struct xmlattribute *),
                                             j);
        if(parser_node->attr == NULL) {
            xml_error = XML_ERR_MEMORY;
            printf("%s: %d\n", __FILE__, __LINE__);
            return;
        }
        parser_node->nattr = j;
        for(i = 0; i < j; i++) {
            xmlattr =
                (struct xmlattribute *) mcalloc(sizeof(struct xmlattribute),
                                                1);
            if(xmlattr == NULL) {
                xml_error = XML_ERR_MEMORY;
                printf("%s: %d\n", __FILE__, __LINE__);
                return;
            }
            xmlattr->name = strdup(atts[2 * i]);
            xmlattr->value = strdup(atts[2 * i + 1]);
            parser_node->attr[i] = xmlattr;
        }
    }
}

static void XMLCALL
endElement(void *data, const char *name)
{
    xml_depth--;
    parser_node = NULL;
}

static void XMLCALL
setElement(void *data, const char *el, int len)
{
    int i, j;

    if(len <= 0 || parser_node == NULL)
        return;

    /* ignore prefixed '\n \r \t space' */
    for(i = 0; i < len; i++) {
        if(el[i] != '\r' && el[i] != '\n' && el[i] != '\t' && el[i] != ' ')
            break;
    }
    if(i == len)
        return;
    if(el[len - 1] == '\n')
        len--;

    if(parser_node->value == NULL) {
        parser_node->value = (char *) mcalloc(len - i + 1, 1);
        if(parser_node->value == NULL) {
            xml_error = XML_ERR_MEMORY;
            printf("%s: %d\n", __FILE__, __LINE__);
            return;
        }
        memcpy(parser_node->value, el + i, len - i);
        parser_node->value[len - i] = '\0';
    }
    else {
        /* expands parser's value */
        j = strlen(parser_node->value);
        parser_node->value =
            (char *) mrealloc((void *) parser_node->value, j + (len - i) + 1);
        if(parser_node->value == NULL) {
            xml_error = XML_ERR_MEMORY;
            printf("%s: %d\n", __FILE__, __LINE__);
            return;
        }
        memcpy(parser_node->value + j, el + i, len - i);
        parser_node->value[j + len - i] = '\0';
    }
}

struct xmlnode *
xmlloadfile(const char *file)
{
    char *buf;
    int fd, len;
    struct xmlnode *node;
    XML_Parser parser;

    if(file == NULL || file[0] == '\0')
        return NULL;

    /* read file content into buffer */
    fd = open(file, O_RDONLY);
    if(fd == -1)
        return NULL;
    len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    buf = (char *) mmalloc(len + 1);
    if(buf == NULL) {
        close(fd);
        return NULL;
    }

    if(len != read(fd, buf, len)) {
        /* read error */
        free(buf);
        close(fd);
        return NULL;
    }
    buf[len] = '\0';
    close(fd);

    /* starting parsing */
    node = (struct xmlnode *) mcalloc(sizeof(struct xmlnode), 1);
    if(node == NULL) {
        free(buf);
        return NULL;
    }

    xml_depth = xml_error = 0;
    parent_depth = 1;
    max_parent_depth = 10;
    parser_parent =
        (struct xmlnode **) mcalloc(sizeof(struct xmlnode **),
                                    max_parent_depth);
    parser = XML_ParserCreate(NULL);
    if(parser == NULL || parser_parent == NULL) {
        if(parser)
            XML_ParserFree(parser);
        free(parser_parent);
        free(node);
        free(buf);
        return NULL;
    }

    parser_parent[0] = node;
    parser_node = NULL;
    XML_SetUserData(parser, NULL);
    XML_SetElementHandler(parser, startElement, endElement);
    XML_SetDefaultHandler(parser, setElement);
    if(XML_Parse(parser, buf, len, 1) == XML_STATUS_ERROR) {
        fprintf(stderr, "%s at line %u\n",
                XML_ErrorString(XML_GetErrorCode(parser)),
                (unsigned int) XML_GetCurrentLineNumber(parser));
        free(node);
        node = NULL;
    }

    XML_ParserFree(parser);

    free(buf);
    free(parser_parent);

    return node;
}

#ifdef TEST
int
main(void)
{
    struct xmlnode *x;
    time_t t1, t2;

    t1 = time(NULL);
    x = xmlloadfile("config.xml");
    t2 = time(NULL);
    printf("t2-t1=%d\n", t2 - t1);
    if(x && xml_error == 0) {
        /* successed! */
        printxml(x, 0);
    }
    freexml(x);
    return 0;
}
#endif
