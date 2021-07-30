#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		/*sleep*/
#include <pthread.h>
#include <sys/prctl.h>

static void *thr_fn(void *arg)
{
	char name[128];
	int index = *(int *)arg;
	cpu_set_t mask;
	//重命名程序名称
	sprintf(name, "mythread%d", index);
	prctl(PR_SET_NAME, name);
	//设置线程的CPU亲和性
	CPU_ZERO(&mask);
	CPU_SET(index, &mask);
	sched_setaffinity(0, sizeof(mask), &mask);
    while(1){
    	sleep(1);
    }
    return NULL;
}

int my7(){
	pthread_t tid[4];
	int arg[4] = {1,2,3,4};
	//创建4个线程，分别仅跑在0,1,2,3核上
	pthread_create(&tid[0], NULL, thr_fn, &arg[0]);
	pthread_create(&tid[1], NULL, thr_fn, &arg[1]);
	pthread_create(&tid[2], NULL, thr_fn, &arg[2]);
	pthread_create(&tid[3], NULL, thr_fn, &arg[3]);
	pthread_join(tid[0], NULL);
	pthread_join(tid[1], NULL);
	pthread_join(tid[2], NULL);
	pthread_join(tid[3], NULL);	
	return 0;
}

