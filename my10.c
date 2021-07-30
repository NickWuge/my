#define _GNU_SOURCE
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
#include <unistd.h>		/*sleep*/

typedef struct{
    long type;
	//int offset;	/*预留头部减少拷贝*/
	int length;
	char buffer[1024];
}msg_t;

int my10(){
	extern int errno;
	int i, rv;
	key_t key;
	int msg1, msg2;
	msg_t m1, m2;
	struct msqid_ds ds;
	key = ftok("/home", 1);
	msg1 = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 2);
	msg2 = msgget(key, IPC_CREAT|O_RDWR|0777);
	msgctl(msg1,IPC_RMID,0);
	msgctl(msg2,IPC_RMID,0);
	key = ftok("/home", 1);
	printf("msg1 key, %x\r\n", key);
	msg1 = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 2);
	printf("msg2 key, %x\r\n", key);
	msg2 = msgget(key, IPC_CREAT|O_RDWR|0777);

	while(1){
		rv = msgrcv(msg1, &m1, 1028, 0, IPC_NOWAIT);
		if(rv < 0)
			break;
	}

	while(1){
		rv = msgrcv(msg2, &m2, 1028, 0, IPC_NOWAIT);
		if(rv < 0)
			break;
	}

	for(i=1;i<1000000;i++){
		m1.type=i;
		rv = msgsnd(msg1, &m1, 4, IPC_NOWAIT);	//队列满则丢弃
		if(rv < 0){
			printf("msg1 %d %d\r\n", i, errno);
			break;
		}
	}
	msgctl(msg1, IPC_STAT, &ds);
	printf("msg1 msg_qnum %u\r\n", (unsigned int)ds.msg_qnum);

	for(i=1;i<1000000;i++){
		m2.type=i;
		rv = msgsnd(msg2, &m2, 1028, IPC_NOWAIT);	//队列满则丢弃
		if(rv < 0){
			printf("msg2 %d %d\r\n", i, errno);
			break;
		}
	}
	msgctl(msg2, IPC_STAT, &ds);
	printf("msg2 msg_qnum %u\r\n", (unsigned int)ds.msg_qnum);
	
	while(1){
		sleep(1);
	}

	return 0;
}

