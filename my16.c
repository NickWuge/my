#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syncinit.h>
#include <syslog.h>

enum
{
	ACTION_RESPAWN,
	ACTION_REBOOT,
	ACTION_ONCE,
};

enum
{
	METHOD_NOTIFY,
	METHOD_WAIT,
};

enum
{
	CGROUP_CPU_SYSTEM,
	CGROUP_CPU_WORK,	//限制业务冲击
	CGROUP_CPU_STACK,	//限制堆叠应用，比如文件同步
};

#define SYS_TASK_PRI2RT(priority)	(((255 - priority) * 99)/255)
#define RT_HIGH		SYS_TASK_PRI2RT(60)		/*高优先级报文及协议处理任务，有任务处理到一半，需要尽快完成*/
#define RT_NORMAL	SYS_TASK_PRI2RT(128)	/*中优先级报文及协议处理任务，有任务处理到一半，需要尽快完成*/
#define RT_LOW		SYS_TASK_PRI2RT(180)	/*HAL收包任务，QOS+限速和cgroup匹配，防止硬件收包缓存溢出*/

typedef struct{
	int action;		//监控不到进程后的处理
	int pdeathsig;	//父进程退出后的处理
	int method;		//首次启动同步方式
	int timeout;	//second，METHOD_NOTIFY表示超时时间，METHOD_WAIT表示等待时间
	int cpu_cgroup;
	int poilcy;		//调度算法，SCHED_FIFO、SCHED_RR、SCHED_OTHER
	int priority;	//poilcy为实时调度算法表示rt_priority（0到99值越大优先级越高），为非实时调度算法表示nice（-20到+19，nice值越低比重越高）
	char *name;
	char *file;
	char *args;
	pid_t pid;
}subprocess_t;

static subprocess_t subprocesses[] = {
	{ACTION_RESPAWN, SIGKILL, METHOD_WAIT, 0, CGROUP_CPU_SYSTEM, SCHED_RR, RT_LOW, "CLISH", "clish", "-l -w init-view", -1},
};

static int subprocess_process(char *file, char *args)
{
	int rv = 0;
	char *p, **argv = NULL, *args_copy = NULL;
	int argc;

	if(!args){
		argv = malloc(sizeof(char *)*2);
		argv[0] = file;
		argv[1] = NULL;
	}else{
		args_copy = malloc(strlen(args) + 1);
		if(!args_copy){
			rv = -1;
			goto exit;
		}
		
		strcpy(args_copy, args);
		argc = 1;	//第一个参数是file
	    p = strtok(args_copy, " ");
	    while(p != NULL){
	        argc++;
	        p = strtok(NULL, " ");
	    }
		argv = malloc(sizeof(char *)*(argc + 1));	//多一个存NULL
		if(!argv){
			rv = -2;
			goto exit;
		}

		strcpy(args_copy, args);
		argc = 0;	
		argv[argc] = file;	//第一个参数是file
		argc++;
	    p = strtok(args_copy, " ");
	    while(p != NULL){
			argv[argc] = p;
	        argc++;
	        p = strtok(NULL, " ");
	    }
		argv[argc] = NULL;
	}	

	if(execvp(file, argv) == -1){
		rv = -3;
		goto exit;
	}
	return 0;

exit:
	if(args_copy)
		free(args_copy);
	if(argv)
		free(argv);
	return rv;
}

static int subprocess_fork(subprocess_t *subprocess)
{
	int rv;

	subprocess->pid = fork();
	if(subprocess->pid == -1){
		//父进程创建子进程失败
		syslog(LOG_EMERG, "fork %s failed\n", subprocess->name);
		return -1;
	}else if(subprocess->pid == 0){
		pid_t pid;
		//子进程上下文
		pid = getpid();

		//设置父进程退出后的处理
		if(subprocess->pdeathsig != 0)
			prctl(PR_SET_PDEATHSIG, subprocess->pdeathsig);

		//设置进程名称
		prctl(PR_SET_NAME, subprocess->name);

		//设置进程CPU亲和性
#if 0
		cpu_set_t mask;
		CPU_ZERO(&mask);
		CPU_SET(7, &mask);
		sched_setaffinity(0, sizeof(mask), &mask);
#endif

		//设置进程调度算法及优先级
		struct sched_param param;
		switch(subprocess->poilcy){
			case SCHED_FIFO:
			case SCHED_RR:
				param.sched_priority = subprocess->priority;
				rv = sched_setscheduler(0, subprocess->poilcy, &param);
				if(rv < 0){
					syslog(LOG_EMERG, "%s sched_setscheduler failed, %d\n", subprocess->name, rv);
					exit(0);
				}
				break;
			case SCHED_OTHER:
				rv = nice(subprocess->priority);
				if(rv < 0){
					syslog(LOG_EMERG, "%s nice failed, %d\n", subprocess->name, rv);
					exit(0);
				}
				break;
			default:
				syslog(LOG_EMERG, "%s illage poilcy, %d\n", subprocess->name, subprocess->poilcy);
				exit(0);
		}					

		/*设置CPU所属cgroup*/
		char cmd[128];
		switch(subprocess->cpu_cgroup){
			case CGROUP_CPU_SYSTEM:
				break;
			case CGROUP_CPU_WORK:
				sprintf(cmd, "echo %d >> /cgroup/cpu/work/cgroup.procs", pid);
				system(cmd);
				break;
			case CGROUP_CPU_STACK:
				sprintf(cmd, "echo %d >> /cgroup/cpu/stack/cgroup.procs", pid);
				system(cmd);
				break;
			default:
				syslog(LOG_EMERG, "%s illage cpu group, %d\n", subprocess->name, subprocess->cpu_cgroup);
				exit(0);
		}

		/*设置MEM所属cgroup*/
#if 0
		sprintf(cmd, "echo %d >> /cgroup/mem/switch/cgroup.procs", pid);
		system(cmd);
#endif

		rv = subprocess_process(subprocess->file, subprocess->args);
		if(rv < 0){
			syslog(LOG_EMERG, "%s subprocess_process failed, %d\n", subprocess->name, rv);
			exit(0);
		}
		exit(0);	//never return
	}else{
		//父进程上下文
		switch(subprocess->method){
			case METHOD_NOTIFY:
				rv = syncinit3_wait(subprocess->pid, subprocess->timeout);
				if(rv < 0){
					syslog(LOG_EMERG, "%s init failed, %d\n", subprocess->name, rv);
					return -1;
				}
				break;
			case METHOD_WAIT:
				sleep(subprocess->timeout);
				break;
			default:
				syslog(LOG_EMERG, "%s illage method, %d\n", subprocess->name, subprocess->method);
				return -1;
		}
	}

	return 0;
}

static void func_waitpid(int signo) {
    pid_t pid;
    int stat;
    while( (pid = waitpid(-1, &stat, WNOHANG)) > 0 ) {
        //printf( "child %d exit\n", pid );
    }
    return;
}

int my16(){
	int rv, i;

	//回收子进程，避免产生僵尸进程
	signal(SIGCHLD, &func_waitpid);

	//屏蔽CTRL+C
	sigset_t intmask;
    sigemptyset(&intmask);
    sigaddset(&intmask,SIGINT);
    sigprocmask(SIG_BLOCK,&intmask,NULL);
	
	/*启动命令行*/
	for(i=0;i<sizeof(subprocesses)/sizeof(subprocess_t);i++){
		rv = subprocess_fork(&subprocesses[i]);
		if(rv != 0)
			goto exit;
	}

	//设置进程调度算法及优先级
	struct sched_param param;
	param.sched_priority = RT_HIGH;
	rv = sched_setscheduler(0, SCHED_RR, &param);
	if(rv < 0){
		syslog(LOG_EMERG, "%s sched_setscheduler failed, %d\n", "init-switch", rv);
		goto exit;
	}

	//监控子进程
	while(1){
		for(i=0;i<sizeof(subprocesses)/sizeof(subprocess_t);i++){
			if(subprocesses[i].pid == -1)
				continue;
			if(subprocesses[i].pid == 0)
				continue;
			rv = kill(subprocesses[i].pid, 0);
			if(rv != 0)
			{
				switch(subprocesses[i].action){
					case ACTION_RESPAWN:
					case ACTION_REBOOT:
						syslog(LOG_ERR, "%s(%d) crashed\n", subprocesses[i].name, subprocesses[i].pid);
						//是否会存在coredump文件生成到一半的情况？need fixed
						//压缩并持久化coredump文件，策略是增加递增的索引编号，仅保存最近n个异常文件，need fixed
						if(subprocesses[i].action == ACTION_RESPAWN){
							rv = subprocess_fork(&subprocesses[i]);
							if(rv != 0)
								goto exit;
						}else if(subprocesses[i].action == ACTION_REBOOT){
							goto exit;
						}
						break;
					case ACTION_ONCE:
						break;
					default:
						syslog(LOG_EMERG, "%s illage method, %d\n", subprocesses[i].name, subprocesses[i].method);
						goto exit;
				}
			}
		}
		sleep(1);
	}

exit:
	//重启系统，处理coredump，need fixed
	syslog(LOG_EMERG, "system reboot...\n");
	return -1;
}

