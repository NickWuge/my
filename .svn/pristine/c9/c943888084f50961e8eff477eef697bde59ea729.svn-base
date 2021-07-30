#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/errno.h>

#define FILE_MODE (S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH)

typedef struct{
    long type;
	//int offset;	/*‘§¡ÙÕ∑≤øºı…ŸøΩ±¥*/
	int length;
	char buffer[1024];
}msg_t;

/*
mkdir /dev/mqueue
mount -t mqueue none /dev/mqueue
*/
int my23(){
	int i, rv;
	struct mq_attr attr;
	msg_t msg;

    attr.mq_maxmsg = 128;
    attr.mq_msgsize = sizeof(msg_t);
    attr.mq_flags = 0;
    mqd_t mqd = mq_open("/mq_test", O_CREAT|O_RDWR, FILE_MODE, &attr);
    if(-1 == mqd)
    {
        perror("mq_open error");
        return 0;
    }
	mq_getattr(mqd, &attr);
	attr.mq_flags = O_NONBLOCK;
	mq_setattr(mqd, &attr, NULL);

	while(1){
		rv = mq_receive(mqd, (char *)&msg, sizeof(msg), NULL);
		if(rv < 0)
			break;
		printf("recv len = [%d], type = [%d]\n", rv, msg.type);
	}

	for(i=1;i<1000000;i++){
		msg.type = i;
		msg.length = 0;
		rv = mq_send(mqd, (char *)&msg, 8, 0);
		if(rv < 0)
		{
			printf("mq_send %d %d\r\n", i, errno);
			break;
		}
	}

	while(1){
		rv = mq_receive(mqd, (char *)&msg, sizeof(msg), NULL);
		if(rv < 0)
			break;
		printf("recv len = [%d], type = [%d]\n", rv, msg.type);
	}

	for(i=1;i<1000000;i++){
		msg.type = i;
		msg.length = 1024;
		rv = mq_send(mqd, (char *)&msg, sizeof(msg), 0);
		if(rv < 0)
		{
			printf("mq_send %d %d\r\n", i, errno);
			break;
		}
	}

#if 0
	while(1){
		rv = mq_receive(mqd, (char *)&msg, sizeof(msg), NULL);
		if(rv < 0)
			break;
		printf("recv len = [%d], type = [%d]\n", rv, msg.type);
	}
#endif

	while(1){
		sleep(1);
	}

	return 0;
}

