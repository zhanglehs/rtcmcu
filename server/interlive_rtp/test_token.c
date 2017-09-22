#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include "../util/util.h"

#define TOKEN_TIMEOUT (12 * 60 * 60)
void
test_token(time_t time_val, unsigned int streamid, const char *private_key,
           const char *ua, size_t ua_sz, const char *server_ip,
           const char *token)
{

    char new_token[68];
    int ret =
        util_gen_token(time_val, streamid, private_key, strlen(private_key),
                       ua, ua_sz, server_ip, strlen(server_ip), new_token,
                       sizeof(new_token));

    assert(0 == ret);
    assert(0 == strcmp(new_token, token));

    ret =
        util_check_token(time_val + TOKEN_TIMEOUT + 60, streamid,
                         private_key, strlen(private_key), ua, ua_sz,
                         server_ip, strlen(server_ip), TOKEN_TIMEOUT, token);
    assert(FALSE == ret);

    ret =
        util_check_token(time_val + 60, streamid, private_key,
                         strlen(private_key), ua, ua_sz, server_ip,
                         strlen(server_ip), TOKEN_TIMEOUT, token);
    assert(TRUE == ret);
}

int
main(void)
{
    time_t time_val = 0x51a70e15;
    unsigned int streamid = 3930867938;
    const char *private_key = "abDEO982@#$:b";
    const char *ua =
        "curl/7.19.7 (x86_64-redhat-linux-gnu) libcurl/7.19.7 NSS/3.13.1.0 zlib/1.2.3 libidn/1.18 libssh2/1.2.2";
    const char *server_ip = "10.10.69.195";
    const char *token = "51a70e152f28508fe5cf50164d3ad0f4";

    time_t t1 = 9897;
    unsigned int s1 = 89;
    const char *p1 = "01234567890123456789012345678901";
    const char *ua1 = "Mozilla/5.0 (MSIE 9.0; Windows NT 6.1; Trident/5.0)";
    const char *si1 = "10.11.11.68";
    const char *tk1 = "000026a9b2a018319b93f113524544ff";

    time_t t2 = 9090898990;
    unsigned int s2 = 938;
    const char *p2 = "9876543219";
    const char *ua2 =
        "The callback argument to many asynchronous methods is now optional, and these methods return a Future. The tornado.gen module now understands Futures, and these methods can be used directly without a gen.Task wrapper.";
    const char *si2 = "0.0.0.0";
    const char *tk2 = "1ddc1c2e6f5617d4203c59bebb9d87d8";
    char tk[128];
    const char *tk3 = "1ddc1c2eca5db3466e31ff584e640c5d";

    time_t t4 = t2;
    unsigned int s4 = s2;
    const char *p4 = "123123123123123123123123123123123123123123123123";
    const char *ua4 = ua1;
    const char *si4 = si2;
    const char *tk4 = "1ddc1c2ef1b34a1cfffd8967d2cd565b";

//    util_gen_token(t2, s2, p3, strlen(p3), ua1, strlen(ua1), si2, tk, sizeof(tk));
//    printf("%s\n", tk);
//    printf("len = %zu\n", strlen(ua2));
    test_token(time_val, streamid, private_key, ua, strlen(ua), server_ip,
               token);
    test_token(t1, s1, p1, ua1, strlen(ua1), si1, tk1);
    test_token(t2, s2, p2, ua2, strlen(ua2), si2, tk2);
    test_token(t2, s2, p2, NULL, 0, si2, tk3);
    test_token(t4, s4, p4, ua4, strlen(ua4), si4, tk4);
    printf("pass\n");
    return 0;
}
