#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		/*sleep*/
#include <pthread.h>
#include <sys/prctl.h>
#include <libsys/timer.h>


static void test_timer_check1(ULONG arg)
{
	struct  timeval  tv_end;
	gettimeofday(&tv_end,NULL);
	printf("%s %d, %d %d. %d.\n",__FUNCTION__,__LINE__,tv_end.tv_sec, tv_end.tv_usec,arg);
}

static void *thr_fn(void *arg)
{
	char name[128];
	int index = *(int *)arg;
	TIMER_USER_DATA test_timer;
	static uint32 test_timerid;

	printf("%s %d, arg %d.\n",__FUNCTION__,__LINE__,*(ULONG *)arg);

	//重命名程序名称
	sprintf(name, "mythread%d", index);
	prctl(PR_SET_NAME, name);
	test_timer.cb.arg = *(ULONG *)arg;
	test_timer.cb.fun = test_timer_check1;
	if(SYS_NOERR != sys_add_timer(TIMER_CALLBACK_METHOD, &test_timer, &test_timerid))
		Print("power add timer failed\n");
	if(test_timerid)
	{	
		sys_start_timer(test_timerid, 10|TIMER_RESOLUTION_S100);
	}
    while(1){
    	sleep(1);
    }
    return NULL;
}

int my22(){
	pthread_t tid[4];
	int arg[4] = {1,2,3,4};
	TIMER_USER_DATA test_timer;
	static uint32 test_timerid;

	sys_timer_task_start();
	
	test_timer.cb.arg = 0;
	test_timer.cb.fun = test_timer_check1;
	if(SYS_NOERR != sys_add_timer(TIMER_CALLBACK_METHOD, &test_timer, &test_timerid))
		Print("power add timer failed\n");
	if(test_timerid)
	{
		sys_start_timer(test_timerid, 1|TIMER_RESOLUTION_S);
	}
	sleep(5);
	ntimer_poll_tick_set(50);
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

