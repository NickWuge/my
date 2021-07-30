#if 0
*#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <arpa/inet.h>	/*inet_addr*/
#include <libubox/uloop.h>
#include <libubox/ustream.h>
#include <libubox/utils.h>
#include <libubus.h>
#include <json-c/json.h>
#include <libubox/blobmsg_json.h>
//#include <vpp_sapi.h>



/*
git clone https://github.com/json-c/json-c.git
cd json-c
sh autogen.sh
./configure  # --enable-threading
make
make install
make check
make USE_VALGRIND=0 check   # optionally skip using valgrind

git clone http://git.openwrt.org/project/libubox.git
cmake -D CMAKE_INSTALL_PREFIX=/usr ./
make
sudo make install

git clone git://nbd.name/luci2/ubus.git
SET(UBUS_MAX_MSGLEN 4194304)
cmake -D CMAKE_INSTALL_PREFIX=/usr ./
make
sudo make install

*/
int main(int argc,char *argv[]){
	
	printf("Hello World Hanyuanwu");
	return 0;
}




*/

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

static struct uloop_fd server;
static const char *port = "10000";
struct client *next_client = NULL;

struct client {
	struct sockaddr_in sin;

	struct ustream_fd s;
	int ctr;
};

static void client_read_cb(struct ustream *s, int bytes)
{
	struct client *cl = container_of(s, struct client, s.stream);
	struct ustream_buf *buf = s->r.head;
	char *newline, *str;

	do {
		str = ustream_get_read_buf(s, NULL);
		if (!str)
			break;

		newline = strchr(buf->data, '\n');
		if (!newline)
			break;

		*newline = 0;
		ustream_printf(s, "%s\n", str);
		ustream_consume(s, newline + 1 - str);
		cl->ctr += newline + 1 - str;
	} while(1);

	if (s->w.data_bytes > 256 && !ustream_read_blocked(s)) {
		fprintf(stderr, "Block read, bytes: %d\n", s->w.data_bytes);
		ustream_set_read_blocked(s, true);
	}
}

static void client_close(struct ustream *s)
{
	struct client *cl = container_of(s, struct client, s.stream);

	fprintf(stderr, "Connection closed\n");
	ustream_free(s);
	close(cl->s.fd.fd);
	free(cl);
}

static void client_notify_write(struct ustream *s, int bytes)
{
	fprintf(stderr, "Wrote %d bytes, pending: %d\n", bytes, s->w.data_bytes);

	if (s->w.data_bytes < 128 && ustream_read_blocked(s)) {
		fprintf(stderr, "Unblock read\n");
		ustream_set_read_blocked(s, false);
	}
}

static void client_notify_state(struct ustream *s)
{
	struct client *cl = container_of(s, struct client, s.stream);

	if (!s->eof)
		return;

	fprintf(stderr, "eof!, pending: %d, total: %d\n", s->w.data_bytes, cl->ctr);
	if (!s->w.data_bytes)
		return client_close(s);

}

static void server_cb(struct uloop_fd *fd, unsigned int events)
{
	struct client *cl;
	unsigned int sl = sizeof(struct sockaddr_in);
	int sfd;

	if (!next_client)
		next_client = calloc(1, sizeof(*next_client));

	cl = next_client;
	sfd = accept(server.fd, (struct sockaddr *) &cl->sin, &sl);
	if (sfd < 0) {
		fprintf(stderr, "Accept failed\n");
		return;
	}

	cl->s.stream.string_data = true;
	cl->s.stream.notify_read = client_read_cb;
	cl->s.stream.notify_state = client_notify_state;
	cl->s.stream.notify_write = client_notify_write;
	ustream_fd_init(&cl->s, sfd);
	next_client = NULL;
	fprintf(stderr, "New connection\n");
}

static int run_server(void)
{

	server.cb = server_cb;
	server.fd = usock(USOCK_TCP | USOCK_SERVER | USOCK_IPV4ONLY | USOCK_NUMERIC, "127.0.0.1", port);
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
	while ((ch = getopt(argc, argv, "p:")) != -1) {
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

