#if 0

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

//定义自己的包
typedef struct RRQ{
	char opcode[2];
	char name[10];
	char zero1;
	char mode[10];	
	char zero2;
}RRQ;


static void server_cb(struct uloop_fd *fd, unsigned int events)//文件描述符加上事件
{
	struct client *cl;  //定义客户端的对象指针
	unsigned int sl = sizeof(struct sockaddr_in);//socket的大小
	int sfd; //描述符

	if (!next_client)//如果为空，则申请空间。
		next_client = calloc(1, sizeof(*next_client));

	cl = next_client;//申请一个客户端大小的空间后赋值给指针
	//写接收的操作==========================================================================
	printf("server_cb run!\n");

	//定义一个请求包
	RRQ rrq1;
	memset(&rrq1,0,sizeof(RRQ));


	

	//server.fd 代表socket描述符
	char buff[50];
	char* str ="loop";
	memset(buff,0,sizeof(buff));						
	int bytelen = recvfrom(server.fd, buff, sizeof(buff), 0,(struct sockaddr *) &cl->sin,&sl);
	//打印接收请求的ip地址
	printf("receive from %s\n",inet_ntoa((cl->sin).sin_addr));
	if(bytelen >= 0)
	{
		printf(" bytelen = %d receive data : %s  \n",bytelen,buff);
		sendto(server.fd,str,strlen(str),0,(struct sockaddr *) &cl->sin,sl);
	}
		
	//

	

	next_client = NULL;//清空全局指针
	fprintf(stderr, "New connection\n");
}

static int run_server(void)
{
	server.cb = server_cb;//事件回调函数，函数指针
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
	printf("main run!\n");
	
	while ((ch = getopt(argc, argv, "p:")) != -1) {//外部手动输入端口号
		switch(ch) {
		case 'p':
			port = optarg;
			break;
		default:
			return usage(argv[0]);
		}
	}
	//自己修改
	printf("server run!\n");
	
	return run_server();
}

#endif

#if 1
#include "tftpx.h"
#include "work_thread.h"

void config(){
	conf_document_root = ".";//"/home/ideawu/books";
}


int main (int argc, char **argv){
	int sock;//创建socket
	int done = 0;	// Server exit.
	socklen_t addr_len;
	pthread_t t_id;//线程id
	struct sockaddr_in server;//创建server对象地址对象
	unsigned short port = SERVER_PORT;//定义端口地址

	printf("Usage: %s [port]\n", argv[0]);//打印本地路径名
	printf("    port - default 10220\n");//提示默认端口

	if(argc > 1){
		port = (unsigned short)atoi(argv[1]);//获取输入的端口参数
	}

	config();//确定路径名
	
	if((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){//创建serverUDP
		printf("Server socket could not be created.\n");
		return 0;
	}
	//初始化地址
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);
	//绑定
	if (bind(sock, (struct sockaddr *) &server, sizeof(server)) < 0){
		printf("Server bind failed. Server already running? Proper permissions?\n");
		return 2;
	}
	
	printf("Server started at 0.0.0.0:%d.\n", port);
	
	struct tftpx_request *request;//请求，其中一部分内容是要写到UDP协议里面的。
	addr_len = sizeof(struct sockaddr_in);
	while(!done){//服务器循环开始，
		request = (struct tftpx_request *)malloc(sizeof(struct tftpx_request));
		memset(request, 0, sizeof(struct tftpx_request));
		request->size = recvfrom(//对UDP请求解析，返回这个UDP请求大小，获取其中的tftp包
				sock, &(request->packet), MAX_REQUEST_SIZE, 0,//解析UDP包的数据部分，也即是TFTP包
				(struct sockaddr *) &(request->client),//获取这个请求的ip地址
				&addr_len);
		request->packet.cmd = ntohs(request->packet.cmd);//包从网络来，将其TFTP包部分的opcode从网络码转为主机码（ushort）
		printf("Receive request.\n");
		//打开线程
		if(pthread_create(&t_id, NULL, work_thread, request) != 0){//解析包后，打开线程，执行线程主控函数，（work_thread）在phread的头文件中声明
			printf("Can't create thread.\n");
		}
	}

	return 0;
}


#endif

