#if 0
////////////////
//////////////
#endif

#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "ustream.h"
#include "uloop.h"
#include "usock.h"

static struct uloop_fd server;//定义文件描述符server
static const char *port = "10000";//端口号，字符串常量
struct client *next_client = NULL;//客户端指针（建立链表？）

struct client {
	struct sockaddr_in sin;  //定义客户端的套接字

	struct ustream_fd s;    //含有uloop功能，用户流链表
	int ctr; //?
};

static void server_cb(struct uloop_fd *fd, unsigned int events)
{
	struct client *cl;
	unsigned int sl = sizeof(struct sockaddr_in);
	int sfd;

	if (!next_client)
		next_client = calloc(1, sizeof(*next_client));

	cl = next_client;
	//


	//

	

	next_client = NULL;
	fprintf(stderr, "New connection\n");
}

static int run_server(void)
{

	server.cb = server_cb;
	server.fd = usock(USOCK_UDP | USOCK_SERVER | USOCK_IPV4ONLY | USOCK_NUMERIC, "0.0.0.0", port);//端口号从外部输入。
	if (server.fd < 0) {
		perror("usock");
		return 1;
	}

	uloop_init();
	uloop_fd_add(&server, ULOOP_READ);
	uloop_run();

	return 0;
}

static int usage(const char *name)
{
	fprintf(stderr, "Usage: %s -p <port>\n", name);
	return 1;
}

int main(int argc, char **argv)
{
	int ch;
	//自己>>>
	printf("server run!\n");
	//<<<
	while ((ch = getopt(argc, argv, "p:")) != -1) {//外部手动输入端口号
		switch(ch) {
		case 'p':
			port = optarg;
			break;
		default:
			return usage(argv[0]);
		}
	}

	return run_server();
}

