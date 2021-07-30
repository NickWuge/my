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
#include <unistd.h>
#include <sys/types.h>

enum process_e{
	P_RX = 0,	/*RX驱动收包进程*/
	P_L2RX_H,	/*L2RX-H高优先将收包进程，处理内部通信*/
	P_L2RX_MH,	/*L2RX-M中优先级收包进程，处理高优先级业务报文*/
	P_L2RX_ML,	/*L2RX-M中优先级收包进程，处理低优先级业务报文*/
	P_L2RX_L,	/*L2RX-L低优先级收包进程，处理管理报文*/
	P_WORK_H1,	/*WORK-H1高优先级业务进程1*/
	P_WORK_H2,	/*WORK-H1高优先级业务进程2*/
	P_WORK_L1,	/*WORK-L1低优先级业务进程1*/
	P_WORK_L2,	/*WORK-L1低优先级业务进程2*/
	P_SSH,		/*SSH管理进程*/
	P_WORKRPC_H1,	/*WORKRPC-H1高优先级业务进程1的RPC进程*/
	P_WORKRPC_L1,	/*WORKRPC-L1低优先级业务进程1的RPC进程*/
	P_DB,		/*DB数据库进程*/
	P_TX,		/*TX驱动发包进程*/
	P_L2TX_H,	/*L2TX-H高优先级发包进程，处理内部通信*/
	P_L2TX_MH,	/*L2TX-M中优先级发包进程，处理高优先级业务报文*/
	P_L2TX_ML,	/*L2TX-M中优先级发包进程，处理低优先级业务报文*/
	P_L2TX_L,	/*L2TX-L低优先级发包进程，处理管理报文*/
	PROCESS_MAX
};

enum type_e{
	T_WORK_H1_RX = 1,
	T_WORK_H2_RX,
	T_WORK_L1_RX,
	T_WORK_L2_RX,
	T_SSH_RX,
	T_WORKRPC_H1_RX,
	T_WORKRPC_L1_RX,
	T_WORK_H1_TX,
	T_WORK_H2_TX,
	T_WORK_L1_TX,
	T_WORK_L2_TX,
	T_SSH_TX,
	T_WORKRPC_H1_TX,
	T_WORKRPC_L1_TX,
	T_DB_WORK_H1_REQUEST,
	T_DB_WORK_H1_REPLAY,
	T_DB_WORK_H2_REQUEST,
	T_DB_WORK_H2_REPLAY,
	T_DB_WORK_L1_REQUEST,
	T_DB_WORK_L1_REPLAY,
	T_DB_WORK_L2_REQUEST,
	T_DB_WORK_L2_REPLAY,
	T_DB_WORKRPC_H1_REQUEST,
	T_DB_WORKRPC_H1_REPLAY,
	T_DB_WORKRPC_L1_REQUEST,
	T_DB_WORKRPC_L1_REPLAY,
	TYPE_MAX
};

static int l2rx_h, l2rx_ml, l2rx_mh, l2rx_l;
static int work_h1_rx, work_h2_rx, work_l1_rx, work_l2_rx;
static int ssh_rx;
static int workrpc_h1_rx, workrpc_l1_rx;
static int l2tx_h, l2tx_ml, l2tx_mh, l2tx_l;
static int work_h1_tx, work_h2_tx, work_l1_tx, work_l2_tx;
static int ssh_tx;
static int workrpc_h1_tx, workrpc_l1_tx;
static int db_request, db_replay;
static int sdk_tx;

typedef struct{
	unsigned int sdk_rx;
	unsigned int sdk_tx;
	unsigned int l2rx_h;
	unsigned int l2rx_mh;
	unsigned int l2rx_ml;
	unsigned int l2rx_l;
	unsigned int l2tx_h;
	unsigned int l2tx_mh;
	unsigned int l2tx_ml;
	unsigned int l2tx_l;
	unsigned int work_h1_rx;
	unsigned int work_h1_tx;
	unsigned int workrpc_h1_rx;
	unsigned int workrpc_h1_tx;
	unsigned int work_h2_rx;
	unsigned int work_h2_tx;
	unsigned int work_l1_rx;
	unsigned int work_l1_tx;
	unsigned int workrpc_l1_rx;
	unsigned int workrpc_l1_tx;
	unsigned int work_l2_rx;
	unsigned int work_l2_tx;
	unsigned int ssh_rx;
	unsigned int ssh_tx;
	unsigned int db_request;
	unsigned int db_replay;
	unsigned int l2rx_h_drop;
	unsigned int l2rx_mh_drop;
	unsigned int l2rx_ml_drop;
	unsigned int l2rx_l_drop;
	unsigned int workrpc_h1_rx_drop;
	unsigned int workrpc_l1_rx_drop;
	unsigned int work_h1_rx_drop;
	unsigned int work_h2_rx_drop;
	unsigned int work_l1_rx_drop;
	unsigned int work_l2_rx_drop;
	unsigned int ssh_rx_drop;
}mib_t;

mib_t *pLastMib, *pCurrentMib;

typedef struct{
	char *name;
	int priority;
	int(*func)();
}subprocess_t;

typedef struct{
    long type;
	//int offset;	/*预留头部减少拷贝*/
	int length;
	char buffer[1024];
}msg_t;

static int my9_rx_process(){
	/*模拟报文接受*/
	int rv;
	unsigned int i = 0;
	msg_t m;
	key_t key;
	key = ftok("/home", 2);
	l2rx_mh = msgget(key, IPC_CREAT|O_RDWR|0777);
	printf("rx_process start\r\n");
	while(i<20){
		i++;
		pCurrentMib->sdk_rx++;
		if(i%2 == 0){
			m.type = T_WORK_H1_RX;
			m.length = 16;
			memset(m.buffer, i, m.length);
			printf("rx_process, %d %d %d %d %d\r\n", m.length, m.buffer[0], m.buffer[1], m.buffer[2], m.buffer[3]);
			rv = msgsnd(l2rx_mh, &m, sizeof(m.length)+m.length, IPC_NOWAIT);	//队列满则丢弃		
			if(rv < 0)
				pCurrentMib->l2rx_mh_drop++;
		}else{
			m.type = T_WORK_H2_RX;
			m.length = 16;
			memset(m.buffer, i, m.length);
			printf("rx_process, %d %d %d %d %d\r\n", m.length, m.buffer[0], m.buffer[1], m.buffer[2], m.buffer[3]);
			rv = msgsnd(l2rx_mh, &m, sizeof(m.length)+m.length, IPC_NOWAIT);	//队列满则丢弃
			if(rv < 0)
				pCurrentMib->l2rx_mh_drop++;
		}
	}
	return 0;
}

static int my9_l2rx_mh_process(){
	int rv;
	msg_t m;
	key_t key;
	key = ftok("/home", 2);
	l2rx_mh = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 5);
	work_h1_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 6);
	work_h2_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	while(1){
		msgrcv(l2rx_mh, &m, sizeof(m)-sizeof(m.type), 0, 0);	//阻塞等待接收
		pCurrentMib->l2rx_mh++;
		switch(m.type){
			case T_WORK_H1_RX:
				rv = msgsnd(work_h1_rx, &m, sizeof(m.length)+m.length, IPC_NOWAIT);	//队列满则丢弃
				if(rv < 0)
					pCurrentMib->work_h1_rx_drop++;
				break;
			case T_WORK_H2_RX:				
				rv = msgsnd(work_h2_rx, &m, sizeof(m.length)+m.length, IPC_NOWAIT);	//队列满则丢弃
				if(rv < 0)
					pCurrentMib->work_h2_rx_drop++;
				break;
			default:
				break;
		}
	}
	return 0;
}

static int my9_work_h1_process(){	
	msg_t m;
	key_t key;
	key = ftok("/home", 5);
	work_h1_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 13);
	l2tx_mh = msgget(key, IPC_CREAT|O_RDWR|0777);
	while(1){		
		msgrcv(work_h1_rx, &m, sizeof(m)-sizeof(m.type), 0, 0);	//阻塞等待接收
		pCurrentMib->work_h1_rx++;
		printf("work_h1_process, %d %d %d %d %d\r\n", m.length, m.buffer[0], m.buffer[1], m.buffer[2], m.buffer[3]);
		//读取数据
		pCurrentMib->work_h1_tx++;
		m.type = T_WORK_H1_TX;
		msgsnd(l2tx_mh, &m, sizeof(m.length)+m.length, 0);	//队列满则等待
	}
	return 0;
}

static int my9_work_h2_process(){
	msg_t m;
	key_t key;
	key = ftok("/home", 6);
	work_h2_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 13);
	l2tx_mh = msgget(key, IPC_CREAT|O_RDWR|0777);
	while(1){		
		msgrcv(work_h2_rx, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收
		pCurrentMib->work_h2_rx++;
		printf("work_h2_process, %d %d %d %d %d\r\n", m.length, m.buffer[0], m.buffer[1], m.buffer[2], m.buffer[3]);
		//读取数据
		pCurrentMib->work_h2_tx++;
		m.type = T_WORK_H2_TX;
		msgsnd(l2tx_mh, &m, sizeof(m.length)+m.length, 0);	//队列满则等待
	}
	return 0;
}

static int my9_tx_process(){
	msg_t m;
	key_t key;
	key = ftok("/home", 25);
	sdk_tx = msgget(key, IPC_CREAT|O_RDWR|0777);
	while(1){
		msgrcv(sdk_tx, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收		
		printf("tx_process, %d, %d %d %d %d\r\n", m.length, m.buffer[0], m.buffer[1], m.buffer[2], m.buffer[3]);
		pCurrentMib->sdk_tx++;
	}
	return 0;
}

static int my9_l2tx_mh_process(){
	msg_t m;
	key_t key;
	key = ftok("/home", 25);
	sdk_tx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 13);
	l2tx_mh = msgget(key, IPC_CREAT|O_RDWR|0777);
	while(1){
		msgrcv(l2tx_mh, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收		
		pCurrentMib->l2tx_mh++;
		msgsnd(sdk_tx, &m, sizeof(m.length)+m.length, 0);	//队列满则等待
	}
	return 0;
}

static subprocess_t subprocesses[PROCESS_MAX] = {
	{"RX", 30, my9_rx_process},
	{"L2RX_H", 30, NULL},
	{"L2RX_MH", 40, my9_l2rx_mh_process},
	{"L2RX_ML", 50, NULL},
	{"L2RX_L", 60, NULL},
	{"WORK_H1", 40, my9_work_h1_process},
	{"WORK_H2", 40, my9_work_h2_process},
	{"WORK_L1", 50, NULL},
	{"WORK_L2", 50, NULL},
	{"SSH", 80, NULL},
	{"WORKRPC_H1", 40, NULL},
	{"WORKRPC_L1", 50, NULL},
	{"DB", 20, NULL},
	{"TX", 30, my9_tx_process},
	{"L2TX_H", 30, NULL},
	{"L2TX_MH", 40, my9_l2tx_mh_process},
	{"L2TX_ML", 50, NULL},
	{"L2TX_L", 60, NULL},
};

int my9(){
	int i;
	pid_t pid[PROCESS_MAX] = {0};
	sem_t *psem = NULL;

	/*初始化各队列及同步信号*/
	psem = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	pLastMib = (mib_t *)mmap(NULL, sizeof(mib_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	pCurrentMib = (mib_t *)mmap(NULL, sizeof(mib_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	sem_init(psem, 1, 0);	/*信号量在进程间共享*/
	memset(pLastMib, 0, sizeof(mib_t));
	memset(pCurrentMib, 0, sizeof(mib_t));
	
	/*创建业务进程*/
	for(i=0;i<PROCESS_MAX;i++){
		pid[i] = fork();
		if(pid[i] == -1){
			//父进程创建子进程失败
			printf("fork %s failed\r\n", subprocesses[i].name);
			return -1;
		}else if(pid[i] == 0){
			if(!subprocesses[i].func)
				return 0;
			//子进程上下文
			int rc;
			char name[128];
			cpu_set_t mask;
			sprintf(name, "my%s", subprocesses[i].name);
			prctl(PR_SET_NAME, name);
			CPU_ZERO(&mask);
			CPU_SET(7, &mask);
			sched_setaffinity(0, sizeof(mask), &mask);
#if 0
			param.sched_priority = my9_subprocesses[i].priority;
			rc = sched_setscheduler(0, SCHED_FIFO, &param);	
			if(rc <0)
				printf("sched_setscheduler failed %d\r\n", rc);
#else
			nice(subprocesses[i].priority);
#endif
			if(i == P_RX){
				printf("sem_wait:%p\r\n", psem);
				sem_wait(psem);	//等待开启端口
			}
			subprocesses[i].func();
			return 0;
		}
	}

	/*命令循环*/	
	printf("cmd loop start\r\n");
#if 0
	param.sched_priority = 80;
	sched_setscheduler(0, SCHED_FIFO, &param);
#else
	nice(80);
#endif
	while(1){
		char cmd[100];
		fgets(cmd, 100, stdin);
		printf("exec %s\r\n", cmd);
		if(strcmp(cmd, "start\n") == 0){
			/*开启端口*/
			printf("sem_post:%p\r\n", psem);
			sem_post(psem);
		}else if(strcmp(cmd, "status\n") == 0){
			printf("msgid\tname\t\tinc\ttotal\tdropInc\tdropTotal\r\n");
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", 0, "SDK_RX", \
				pCurrentMib->sdk_rx-pLastMib->sdk_rx, pCurrentMib->sdk_rx, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", sdk_tx, "SDK_TX", \
				pCurrentMib->sdk_tx-pLastMib->sdk_tx, pCurrentMib->sdk_tx, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", l2rx_h, "L2RX_H", \
				pCurrentMib->l2rx_h-pLastMib->l2rx_h, pCurrentMib->l2rx_h, \
				pCurrentMib->l2rx_h_drop-pLastMib->l2rx_h_drop, pCurrentMib->l2rx_h_drop);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", l2rx_mh, "L2RX_MH", \
				pCurrentMib->l2rx_mh-pLastMib->l2rx_mh, pCurrentMib->l2rx_mh, \
				pCurrentMib->l2rx_mh_drop-pLastMib->l2rx_mh_drop, pCurrentMib->l2rx_mh_drop);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", l2rx_ml, "L2RX_ML", \
				pCurrentMib->l2rx_ml-pLastMib->l2rx_ml, pCurrentMib->l2rx_ml, \
				pCurrentMib->l2rx_ml_drop-pLastMib->l2rx_ml_drop, pCurrentMib->l2rx_ml_drop);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", l2rx_l, "L2RX_L", \
				pCurrentMib->l2rx_l-pLastMib->l2rx_l, pCurrentMib->l2rx_l, \
				pCurrentMib->l2rx_l_drop-pLastMib->l2rx_l_drop, pCurrentMib->l2rx_l_drop);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", l2tx_h, "L2TX_H", \
				pCurrentMib->l2tx_h-pLastMib->l2tx_h, pCurrentMib->l2tx_h, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", l2tx_mh, "L2TX_MH", \
				pCurrentMib->l2tx_mh-pLastMib->l2tx_mh, pCurrentMib->l2tx_mh, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", l2tx_ml, "L2TX_ML", \
				pCurrentMib->l2tx_ml-pLastMib->l2tx_ml, pCurrentMib->l2tx_ml, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", l2tx_l, "L2TX_L", \
				pCurrentMib->l2tx_l-pLastMib->l2tx_l, pCurrentMib->l2tx_l, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", workrpc_h1_rx, "WORKRPC_H1_RX", \
				pCurrentMib->workrpc_h1_rx-pLastMib->workrpc_h1_rx, pCurrentMib->workrpc_h1_rx, \
				pCurrentMib->workrpc_h1_rx_drop-pLastMib->workrpc_h1_rx_drop, pCurrentMib->workrpc_h1_rx_drop);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", workrpc_l1_rx, "WORKRPC_L1_RX", \
				pCurrentMib->workrpc_l1_rx-pLastMib->workrpc_l1_rx, pCurrentMib->workrpc_l1_rx, \
				pCurrentMib->workrpc_l1_rx_drop-pLastMib->workrpc_l1_rx_drop, pCurrentMib->workrpc_l1_rx_drop);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", workrpc_h1_tx, "WORKRPC_H1_TX", \
				pCurrentMib->workrpc_h1_tx-pLastMib->workrpc_h1_tx, pCurrentMib->workrpc_h1_tx, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", workrpc_l1_tx, "WORKRPC_L1_TX", \
				pCurrentMib->workrpc_l1_tx-pLastMib->workrpc_l1_tx, pCurrentMib->workrpc_l1_tx, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", work_h1_rx, "WORK_H1_RX", \
				pCurrentMib->work_h1_rx-pLastMib->work_h1_rx, pCurrentMib->work_h1_rx, \
				pCurrentMib->work_h1_rx_drop-pLastMib->work_h1_rx_drop, pCurrentMib->work_h1_rx_drop);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", work_h2_rx, "WORK_H2_RX", \
				pCurrentMib->work_h2_rx-pLastMib->work_h2_rx, pCurrentMib->work_h2_rx, \
				pCurrentMib->work_h2_rx_drop-pLastMib->work_h2_rx_drop, pCurrentMib->work_h2_rx_drop);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", work_l1_rx, "WORK_L1_RX", \
				pCurrentMib->work_l1_rx-pLastMib->work_l1_rx, pCurrentMib->work_l1_rx, \
				pCurrentMib->work_l1_rx_drop-pLastMib->work_l1_rx_drop, pCurrentMib->work_l1_rx_drop);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", work_l2_rx, "WORK_L2_RX", \
				pCurrentMib->work_l2_rx-pLastMib->work_l2_rx, pCurrentMib->work_l2_rx, \
				pCurrentMib->work_l2_rx_drop-pLastMib->work_l2_rx_drop, pCurrentMib->work_l2_rx_drop);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", work_h1_tx, "WORK_H1_TX", \
				pCurrentMib->work_h1_tx-pLastMib->work_h1_tx, pCurrentMib->work_h1_tx, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", work_h2_tx, "WORK_H2_TX", \
				pCurrentMib->work_h2_tx-pLastMib->work_h2_tx, pCurrentMib->work_h2_tx, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", work_l1_tx, "WORK_L1_TX", \
				pCurrentMib->work_l1_tx-pLastMib->work_l1_tx, pCurrentMib->work_l1_tx, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", work_l2_tx, "WORK_L2_TX", \
				pCurrentMib->work_l2_tx-pLastMib->work_l2_tx, pCurrentMib->work_l2_tx, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", ssh_rx, "SSH_RX", \
				pCurrentMib->ssh_rx-pLastMib->ssh_rx, pCurrentMib->ssh_rx, \
				pCurrentMib->ssh_rx_drop-pLastMib->ssh_rx_drop, pCurrentMib->ssh_rx_drop);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", ssh_tx, "SSH_TX", \
				pCurrentMib->ssh_tx-pLastMib->ssh_tx, pCurrentMib->ssh_tx, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", db_request, "DB_REPUEST", \
				pCurrentMib->db_request-pLastMib->db_request, pCurrentMib->db_request, \
				0, 0);
			printf("%u\t%-10s\t%u\t%u\t%u\t%u\r\n", db_replay, "DB_REPLAY", \
				pCurrentMib->db_replay-pLastMib->db_replay, pCurrentMib->db_replay, \
				0, 0);
			memcpy(pLastMib, pCurrentMib, sizeof(mib_t));
		}
	}
	printf("cmd loop stop\r\n");
	while(1){
		sleep(1);
	}

	return 0;
}

