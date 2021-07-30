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

#define HERE do{ \
	printf("%s %d\r\n", __FUNCTION__, __LINE__); \
}while(0)

enum hal_thread_e{
	P_HAL_RX,		/*RX驱动收包线程*/
	P_HAL_TX,		/*TX驱动发包线程*/
	P_HAL_MAX
};

enum l2_thread_e{
	P_L2_RX_H,		/*L2RX-H高优先将收包线程，处理内部通信*/
	P_L2_RX_MH,		/*L2RX-M中优先级收包线程，处理高优先级业务报文*/
	P_L2_RX_ML,		/*L2RX-M中优先级收包线程，处理低优先级业务报文*/
	P_L2_RX_L,		/*L2RX-L低优先级收包线程，处理管理报文*/
	P_L2_TX_H,		/*L2TX-H高优先级发包线程，处理内部通信*/
	P_L2_TX_MH,		/*L2TX-M中优先级发包线程，处理高优先级业务报文*/
	P_L2_TX_ML,		/*L2TX-M中优先级发包线程，处理低优先级业务报文*/
	P_L2_TX_L,		/*L2TX-L低优先级发包线程，处理管理报文*/
	P_L2_MAX
};

enum process_e{
	P_HAL = 0,		/*HAL进程，包含TX/RX收发包线程*/
	P_L2,			/*L2进程，包含L2收发包线程*/
	P_WORK_H1,		/*WORK-H1高优先级业务进程1*/
	P_WORK_H2,		/*WORK-H1高优先级业务进程2*/
	P_WORK_L1,		/*WORK-L1低优先级业务进程1*/
	P_WORK_L2,		/*WORK-L1低优先级业务进程2*/
	P_SSH,			/*SSH管理进程*/
	P_WORKRPC_H1,	/*WORKRPC-H1高优先级业务进程1的RPC进程*/
	P_WORKRPC_L1,	/*WORKRPC-L1低优先级业务进程1的RPC进程*/
	P_DB,			/*DB数据库进程*/
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

typedef struct{
	char *name;
	int sched_policy;
	int sched_priority;
	int(*func)();
}subprocess_t;

typedef struct{
    long type;
}msg_t;

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

sem_t *psem = NULL;
mib_t *pLastMib, *pCurrentMib;

static int my8_hal_rx_thread(){
	/*模拟报文冲击*/
	int rv;
	unsigned int i = 0;
	msg_t m;
	
	sem_wait(psem);	//等待开启端口
	printf("rx_process start\r\n");
	
	while(i<200000){
		i++;
		//内部通信报文间无法QOS
		if(i%10 == 0){
			pCurrentMib->sdk_rx++;
			//一些高优先级业务内部通信报文
			m.type = T_WORKRPC_H1_RX;
			rv = msgsnd(l2rx_h, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
			if(rv < 0)
				pCurrentMib->l2rx_h_drop++;
			pCurrentMib->sdk_rx++;
			//一些低优先级业务内部通信报文
			m.type = T_WORKRPC_L1_RX;
			rv = msgsnd(l2rx_h, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
			if(rv < 0)
				pCurrentMib->l2rx_h_drop++;
		}
		
		//大量高优先级业务报文
		pCurrentMib->sdk_rx++;
		m.type = T_WORK_H1_RX;
		rv = msgsnd(l2rx_mh, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃		
		if(rv < 0)
			pCurrentMib->l2rx_mh_drop++;
		pCurrentMib->sdk_rx++;
		m.type = T_WORK_H2_RX;
		rv = msgsnd(l2rx_mh, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
		if(rv < 0)
			pCurrentMib->l2rx_mh_drop++;

		//大量低优先级业务报文
		pCurrentMib->sdk_rx++;
		m.type = T_WORK_L1_RX;
		rv = msgsnd(l2rx_ml, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃		
		if(rv < 0)
			pCurrentMib->l2rx_ml_drop++;
		pCurrentMib->sdk_rx++;
		m.type = T_WORK_L2_RX;
		rv = msgsnd(l2rx_ml, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
		if(rv < 0)
			pCurrentMib->l2rx_ml_drop++;
		
		//少量管理报文
		if(i%20 == 0){
			pCurrentMib->sdk_rx++;
			m.type = T_SSH_RX;
			rv = msgsnd(l2rx_l, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
			if(rv < 0)
				pCurrentMib->l2rx_l_drop++;		
		}
	}
	return 0;
}

static int my8_hal_tx_thread(){
	msg_t m;
	while(1){
		msgrcv(sdk_tx, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收		
		pCurrentMib->sdk_tx++;
	}
	return 0;
}

static int my8_l2rx_h_thread(){
	int rv;
	msg_t m;
	while(1){
		msgrcv(l2rx_h, &m, sizeof(m)-sizeof(m.type), 0, 0);	//阻塞等待接收
		pCurrentMib->l2rx_h++;
		switch(m.type){
			case T_WORKRPC_H1_RX:
				rv = msgsnd(workrpc_h1_rx, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
				if(rv < 0)
					pCurrentMib->workrpc_h1_rx_drop++;
				break;
			case T_WORKRPC_L1_RX:
				rv = msgsnd(workrpc_l1_rx, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
				if(rv < 0)
					pCurrentMib->workrpc_l1_rx_drop++;
				break;
			default:
				break;
		}
	}
	return 0;
}

static int my8_l2rx_mh_thread(){
	int rv;
	msg_t m;
	while(1){
		msgrcv(l2rx_mh, &m, sizeof(m)-sizeof(m.type), 0, 0);	//阻塞等待接收
		pCurrentMib->l2rx_mh++;
		switch(m.type){
			case T_WORK_H1_RX:
				rv = msgsnd(work_h1_rx, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
				if(rv < 0)
					pCurrentMib->work_h1_rx_drop++;
				break;
			case T_WORK_H2_RX:				
				rv = msgsnd(work_h2_rx, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
				if(rv < 0)
					pCurrentMib->work_h2_rx_drop++;
				break;
			default:
				break;
		}
	}
	return 0;
}

static int my8_l2rx_ml_thread(){
	int rv;
	msg_t m;
	while(1){
		msgrcv(l2rx_ml, &m, sizeof(m)-sizeof(m.type), 0, 0);	//阻塞等待接收
		pCurrentMib->l2rx_ml++;
		switch(m.type){
			case T_WORK_L1_RX:
				rv = msgsnd(work_l1_rx, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
				if(rv < 0)
					pCurrentMib->work_l1_rx_drop++;
				break;
			case T_WORK_L2_RX:
				rv = msgsnd(work_l2_rx, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
				if(rv < 0)
					pCurrentMib->work_l2_rx_drop++;
				break;
			default:
				break;
		}
	}
	return 0;
}

static int my8_l2rx_l_thread(){
	int rv;
	msg_t m;
	while(1){		
		msgrcv(l2rx_l, &m, sizeof(m)-sizeof(m.type), 0, 0);	//阻塞等待接收
		pCurrentMib->l2rx_l++;
		switch(m.type){
			case T_SSH_RX:
				rv = msgsnd(ssh_rx, &m, sizeof(m)-sizeof(m.type), IPC_NOWAIT);	//队列满则丢弃
				if(rv < 0)
					pCurrentMib->ssh_rx_drop++;
				break;
			default:
				break;
		}
	}
	return 0;
}

static int my8_l2tx_h_thread(){
	msg_t m;
	while(1){
		msgrcv(l2tx_h, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收		
		pCurrentMib->l2tx_h++;
		msgsnd(sdk_tx, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
	}
	return 0;
}

static int my8_l2tx_mh_thread(){
	msg_t m;
	while(1){
		msgrcv(l2tx_mh, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收		
		pCurrentMib->l2tx_mh++;
		msgsnd(sdk_tx, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
	}
	return 0;
}

static int my8_l2tx_ml_thread(){
	msg_t m;
	while(1){
		msgrcv(l2tx_ml, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收		
		pCurrentMib->l2tx_ml++;
		msgsnd(sdk_tx, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
	}
	return 0;
}

static int my8_l2tx_l_thread(){
	msg_t m;
	while(1){
		msgrcv(l2tx_l, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收		
		pCurrentMib->l2tx_l++;
		msgsnd(sdk_tx, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
	}
	return 0;
}

static int my8_work_h1_process(){	
	msg_t m;
	while(1){		
		msgrcv(work_h1_rx, &m, sizeof(m)-sizeof(m.type), 0, 0);	//阻塞等待接收
		pCurrentMib->work_h1_rx++;
		//访问数据库
		m.type = T_DB_WORK_H1_REQUEST;
		msgsnd(db_request, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
		pCurrentMib->db_request++;
		msgrcv(db_replay, &m, sizeof(m)-sizeof(m.type), T_DB_WORK_H1_REPLAY, 0);	//阻塞等待接收		
		pCurrentMib->db_replay++;
		//耗时操作
		pCurrentMib->work_h1_tx++;
		m.type = T_WORK_H1_TX;
		msgsnd(l2tx_mh, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
	}
	return 0;
}

static int my8_work_h2_process(){
	msg_t m;
	while(1){		
		msgrcv(work_h2_rx, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收
		pCurrentMib->work_h2_rx++;
		//访问数据库
		m.type = T_DB_WORK_H2_REQUEST;
		msgsnd(db_request, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
		pCurrentMib->db_request++;
		msgrcv(db_replay, &m, sizeof(m)-sizeof(m.type), T_DB_WORK_H2_REPLAY, 0);	//阻塞等待接收		
		pCurrentMib->db_replay++;
		//耗时操作
		pCurrentMib->work_h2_tx++;
		m.type = T_WORK_H2_TX;
		msgsnd(l2tx_mh, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
	}
	return 0;
}

static int my8_work_l1_process(){
	msg_t m;
	while(1){		
		msgrcv(work_l1_rx, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收
		pCurrentMib->work_l1_rx++;
		//访问数据库
		m.type = T_DB_WORK_L1_REQUEST;
		msgsnd(db_request, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
		pCurrentMib->db_request++;
		msgrcv(db_replay, &m, sizeof(m)-sizeof(m.type), T_DB_WORK_L1_REPLAY, 0);	//阻塞等待接收		
		pCurrentMib->db_replay++;
		//耗时操作
		pCurrentMib->work_l1_tx++;
		m.type = T_WORK_L1_TX;
		msgsnd(l2tx_ml, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
	}
	return 0;
}

static int my8_work_l2_process(){
	msg_t m;
	while(1){
		msgrcv(work_l2_rx, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收
		pCurrentMib->work_l2_rx++;
		//访问数据库
		m.type = T_DB_WORK_L2_REQUEST;
		msgsnd(db_request, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
		pCurrentMib->db_request++;
		msgrcv(db_replay, &m, sizeof(m)-sizeof(m.type), T_DB_WORK_L2_REPLAY, 0);	//阻塞等待接收		
		pCurrentMib->db_replay++;
		//耗时操作
		pCurrentMib->work_l2_tx++;
		m.type = T_WORK_L2_TX;
		msgsnd(l2tx_ml, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
	}
	return 0;
}

static int my8_ssh_process(){
	msg_t m;
	while(1){
		msgrcv(ssh_rx, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收		
		pCurrentMib->ssh_rx++;
		//耗时操作
		pCurrentMib->ssh_tx++;
		m.type = T_SSH_TX;
		msgsnd(l2tx_l, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
	}
	return 0;
}

static int my8_workrpc_h1_process(){	
	msg_t m;
	while(1){
		msgrcv(workrpc_h1_rx, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收		
		pCurrentMib->workrpc_h1_rx++;
		//访问数据库
		m.type = T_DB_WORKRPC_H1_REQUEST;
		msgsnd(db_request, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
		pCurrentMib->db_request++;
		msgrcv(db_replay, &m, sizeof(m)-sizeof(m.type), T_DB_WORKRPC_H1_REPLAY, 0);	//阻塞等待接收		
		pCurrentMib->db_replay++;
		//耗时操作
		pCurrentMib->workrpc_h1_tx++;
		m.type = T_WORKRPC_H1_TX;
		msgsnd(l2tx_h, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
	}
	return 0;
}

static int my8_workrpc_l1_process(){	
	msg_t m;
	while(1){
		msgrcv(workrpc_l1_rx, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收		
		pCurrentMib->workrpc_l1_rx++;
		//访问数据库
		m.type = T_DB_WORKRPC_L1_REQUEST;
		msgsnd(db_request, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
		pCurrentMib->db_request++;
		msgrcv(db_replay, &m, sizeof(m)-sizeof(m.type), T_DB_WORKRPC_L1_REPLAY, 0);	//阻塞等待接收		
		pCurrentMib->db_replay++;
		//耗时操作
		pCurrentMib->workrpc_l1_tx++;
		m.type = T_WORKRPC_L1_TX;
		msgsnd(l2tx_h, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
	}
	return 0;
}

static int my8_db_process(){	
	msg_t m;
	while(1){
		msgrcv(db_request, &m, sizeof(m)-sizeof(m.type), 0, 0); //阻塞等待接收		
		switch(m.type){
			case T_DB_WORK_H1_REQUEST:
				m.type = T_DB_WORK_H1_REPLAY;
				msgsnd(db_replay, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
				break;
			case T_DB_WORK_H2_REQUEST:
				m.type = T_DB_WORK_H2_REPLAY;
				msgsnd(db_replay, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
				break;
			case T_DB_WORK_L1_REQUEST:
				m.type = T_DB_WORK_L1_REPLAY;
				msgsnd(db_replay, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
				break;
			case T_DB_WORK_L2_REQUEST:
				m.type = T_DB_WORK_L2_REPLAY;
				msgsnd(db_replay, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
				break;
			case T_DB_WORKRPC_H1_REQUEST:
				m.type = T_DB_WORKRPC_H1_REPLAY;
				msgsnd(db_replay, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
				break;
			case T_DB_WORKRPC_L1_REQUEST:
				m.type = T_DB_WORKRPC_L1_REPLAY;
				msgsnd(db_replay, &m, sizeof(m)-sizeof(m.type), 0);	//队列满则等待
				break;
			default:
				break;
		}
	}
}



static subprocess_t hal_threads[P_HAL_MAX] = {
	{"HAL_RX", SCHED_RR, 35, my8_hal_rx_thread},		//比高优先级业务低一点，QOS+限速和cgroup匹配，防止硬件收包缓存溢出
	{"HAL_TX", SCHED_RR, 50, my8_hal_tx_thread},		//说明有任务处理到一半，需要尽快完成
};

static int my8_hal_process(){
	int i;
	pthread_t tid[P_HAL_MAX];
	pthread_attr_t attr;
	struct sched_param param;

	for(i=0;i<P_HAL_MAX;i++){
		//设置线程调度算法及优先级
		switch(hal_threads[i].sched_policy){
			case SCHED_FIFO:
			case SCHED_RR:
				pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
				pthread_attr_setschedpolicy(&attr, hal_threads[i].sched_policy);
				param.sched_priority = hal_threads[i].sched_priority;
				pthread_attr_setschedparam(&attr, &param);
				break;
			default:
				break;
		}
		pthread_attr_destroy(&attr);

		//创建线程
		pthread_create(&tid[i], &attr, hal_threads[i].func, NULL);

		//设置线程名称
		pthread_setname_np(tid[i], hal_threads[i].name); 
	}

	for(i=0;i<P_HAL_MAX;i++){
		pthread_join(tid[i], NULL);
	}

	return 0;
}

static subprocess_t l2_threads[P_L2_MAX] = {
	{"L2RX_H", SCHED_RR, 50, my8_l2rx_h_thread},		//说明有任务处理到一半，需要尽快完成
	{"L2RX_MH", SCHED_RR, 40, my8_l2rx_mh_thread},	//说明有任务处理到一半，需要尽快完成
	{"L2RX_ML", SCHED_OTHER, 0, my8_l2rx_ml_thread},	//nice不同，need fixed
	{"L2RX_L", SCHED_OTHER, 0, my8_l2rx_l_thread},		//nice不同，need fixed
	{"L2TX_H", SCHED_RR, 50, my8_l2tx_h_thread},		//说明有任务处理到一半，需要尽快完成
	{"L2TX_MH", SCHED_RR, 40, my8_l2tx_mh_thread},	//说明有任务处理到一半，需要尽快完成
	{"L2TX_ML", SCHED_OTHER, 0, my8_l2tx_ml_thread},	//nice不同，need fixed
	{"L2TX_L", SCHED_OTHER, 0, my8_l2tx_l_thread},		//nice不同，need fixed
};

static int my8_l2_process(){
	int i;
	pthread_t tid[P_L2_MAX];
	pthread_attr_t attr;
	struct sched_param param;

	for(i=0;i<P_L2_MAX;i++){
		//设置线程调度算法及优先级
		pthread_attr_init(&attr);
		//pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		//pthread_attr_setstacksize(&attr, stacksize);
		switch(l2_threads[i].sched_policy){
			case SCHED_FIFO:
			case SCHED_RR:
				pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
				pthread_attr_setschedpolicy(&attr, l2_threads[i].sched_policy);
				param.sched_priority = l2_threads[i].sched_priority;
				pthread_attr_setschedparam(&attr, &param);
				break;
			default:
				break;
		}
		pthread_attr_destroy(&attr);

		//创建线程
		pthread_create(&tid[i], &attr, l2_threads[i].func, NULL);

		//设置线程名称
		pthread_setname_np(tid[i], l2_threads[i].name);
	}

	for(i=0;i<P_L2_MAX;i++){
		pthread_join(tid[i], NULL);
	}

	return 0;
}

static subprocess_t subprocesses[PROCESS_MAX] = {
	{"HAL", SCHED_OTHER, 0, my8_hal_process},	
	{"L2", SCHED_OTHER, 0, my8_l2_process},
	{"WORK_H1", SCHED_RR, 40, my8_work_h1_process},		//说明有任务处理到一半，需要尽快完成
	{"WORK_H2", SCHED_RR, 40, my8_work_h2_process},		//说明有任务处理到一半，需要尽快完成
	{"WORK_L1", SCHED_OTHER, 0, my8_work_l1_process},		//nice不同，need fixed
	{"WORK_L2", SCHED_OTHER, 0, my8_work_l2_process},		//nice不同，need fixed
	{"SSH", SCHED_OTHER, 0, my8_ssh_process},				//nice不同，need fixed
	{"WORKRPC_H1", SCHED_RR, 50, my8_workrpc_h1_process},	//说明有任务处理到一半，需要尽快完成
	{"WORKRPC_L1", SCHED_OTHER, 0, my8_workrpc_l1_process},	//nice不同，need fixed
	{"DB", SCHED_RR, 50, my8_db_process},					//说明有任务处理到一半，需要尽快完成
};


int my8(){
	int i;
	pid_t pid[PROCESS_MAX] = {0};
	key_t key;
	struct sched_param param;
	/*初始化各队列及同步信号*/
	psem = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	pLastMib = (mib_t *)mmap(NULL, sizeof(mib_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	pCurrentMib = (mib_t *)mmap(NULL, sizeof(mib_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	sem_init(psem, 1, 0);	/*信号量在进程间共享*/
	memset(pLastMib, 0, sizeof(mib_t));
	memset(pCurrentMib, 0, sizeof(mib_t));
	key = ftok("/home", 1);
	l2rx_h = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 2);
	l2rx_mh = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 3);
	l2rx_ml = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 4);
	l2rx_l = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 5);
	work_h1_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 6);
	work_h2_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 7);
	work_l1_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 8);
	work_l2_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 9);
	ssh_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 10);
	workrpc_h1_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 11);
	workrpc_l1_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 12);
	l2tx_h = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 13);
	l2tx_mh = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 14);
	l2tx_ml = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 15);
	l2tx_l = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 16);
	work_h1_tx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 17);
	work_h2_tx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 18);
	work_l1_tx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 19);
	work_l2_tx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 20);
	ssh_tx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 21);
	workrpc_h1_tx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 22);
	workrpc_l1_tx = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 23);
	db_request = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 24);
	db_replay = msgget(key, IPC_CREAT|O_RDWR|0777);
	key = ftok("/home", 25);
	sdk_tx = msgget(key, IPC_CREAT|O_RDWR|0777);
	
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
			prctl(PR_SET_NAME, subprocesses[i].name);
#if 0			
			cpu_set_t mask;
			CPU_ZERO(&mask);
			CPU_SET(7, &mask);
			sched_setaffinity(0, sizeof(mask), &mask);
#endif
#if 1
			param.sched_priority = subprocesses[i].sched_priority;
			rc = sched_setscheduler(0, subprocesses[i].sched_policy, &param);	
			if(rc <0)
				printf("sched_setscheduler failed %d\r\n", rc);
#else
			nice(subprocesses[i].sched_priority);
#endif
			subprocesses[i].func();
			return 0;
		}
	}

	/*命令循环*/	
	printf("cmd loop start\r\n");
	nice(-20);
	while(1){
		char cmd[100];
		fgets(cmd, 100, stdin);
		printf("exec %s\r\n", cmd);
		if(strcmp(cmd, "start\n") == 0){
			/*开启端口*/
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

