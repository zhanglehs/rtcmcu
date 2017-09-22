#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "proto_define.h"
#include "buffer.hpp"
#define DEFAULT_PORT 18010
#define MAXLINE 4096

using namespace lcdn;
using namespace lcdn::base;
const uint8_t MAGIC = 0xff;
const uint8_t VER = 1;

int encode_header(Buffer* obuf, uint16_t cmd, uint32_t size)
{
    if (obuf->capacity() < size)
    {
        return -1;
    }

    uint16_t ncmd = htons(cmd);
    uint32_t nsize = htonl(size);
    
    obuf->append_ptr(&MAGIC, sizeof(uint8_t));
    obuf->append_ptr(&VER, sizeof(uint8_t));
    obuf->append_ptr(&ncmd, sizeof(uint16_t));
    obuf->append_ptr(&nsize, sizeof(uint32_t));

    return 0;
}
    
int encode_f2t_register_req(Buffer* obuf)
{
    encode_header(obuf, CMD_F2T_REGISTER_REQ, sizeof(proto_header)+6);
    uint16_t port = htons(10080);
    uint16_t asn = htons(3);
    uint16_t region = htons(5);

    obuf->append_ptr(&port, sizeof(port));
    obuf->append_ptr(&asn, sizeof(asn));
    obuf->append_ptr(&region, sizeof(region));

    return 0;
}


void* createClient(void *ipaddr)
{
	int client_fd;
	sockaddr_in servaddr;
	char recvline[MAXLINE];
	char sendline[MAXLINE];
	int n = 0;
    Buffer obuf(MAXLINE);

	if((client_fd = socket(AF_INET, SOCK_STREAM, 0) ) == -1)
	{
		printf("create socket error!\n");
		exit(0);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(DEFAULT_PORT);

	if (inet_pton(AF_INET, (char*)ipaddr, &servaddr.sin_addr) < 0)
	{
		printf("inet_pton error for %s \n", (char*)ipaddr);
		exit(0);
	}

	if (connect(client_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
	{
		printf("connect error! \n");
		exit(0);
	}

    encode_f2t_register_req(&obuf);
	//fgets(sendline, 4096, stdin);

    while (NULL != fgets(sendline, 256, stdin))
    {
        char* str_obuf = (char*)(obuf.data_ptr());
        
        if (send(client_fd, str_obuf, obuf.data_len(), 0) < 0)
        {
            printf("send msg error! \n");
            exit(0);
        }
            

        if ((n = recv(client_fd, recvline, MAXLINE, 0)) == -1)
        {
            printf("recv msg error! \n");
            exit(0);
        }

        recvline[n] = '\0';
        printf("recv msg: %s \n", recvline);
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[])
{

    createClient((void*)("10.10.69.120"));

	return 0;
}

