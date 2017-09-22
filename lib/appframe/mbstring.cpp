#include "gb.h"
#include "mbstring.h"


char *  mbsnbcpy(char *dst,  const  char *src, size_t& count)
{
    size_t cnt = count;
    char *start = dst;
    count = 0;

    while (cnt)
    {
        cnt--;
        count++;
        if (IN_RANGE(*src, GBK_HIMIN, GBK_HIMAX))
        {
            *dst++ = *src++;
            if (!cnt) 
            {
                dst[-1] = '\0';
                count--;
                break;
            }
            cnt--;
            count++;
            if ((*dst++ = *src++) == '\0') 
            {
                dst[-2] = '\0';
                count-=2;
                break;
            }
        }
        else
        {
            if ((*dst++ = *src++) == '\0')
            {
                count--;
                break;
            }
            if (!cnt) 
            {
                dst[-1] = '\0';
                count--;
                break;
            }
        }
    }

    
    return start;
}


unsigned char * mbschr(const unsigned char *string,  unsigned int c)
{
    unsigned short cc;


    for (; (cc = *string); string++)
    {
        if (IN_RANGE(cc, GBK_HIMIN, GBK_HIMAX))
        {
            if (*++string == '\0')
            {
                return NULL;        /* error */
            }
            if ( c == (unsigned int)((cc << 8) | *string) ) /* DBCS match */
            {
                return (unsigned char *)(string - 1);
            }
        }
        else if (c == (unsigned int)cc)
            break;  /* SBCS match */
    }


    if (c == (unsigned int)cc)      /* check for SBCS match--handles NUL char */
        return (unsigned char *)(string);

    return NULL;
}

