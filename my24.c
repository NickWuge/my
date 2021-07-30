#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		/*sleep*/
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#include <semaphore.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <libres/resource.h>
#include <entity/entity.h>
#include <libsys/vos/vos_types.h>

static char *hostname_str = "IES200";
RESOURCE_ID hostname_id;


static char *mystrdup(const char *str1)
{
	char *dupstr = NULL;
	if (str1) {
		dupstr = (char *)malloc(strlen(str1) + 1);
		if (dupstr) {
			strcpy(dupstr, str1);
		}
	}
	return dupstr;
}

static int __RESOURCE_TYPE_HOSTNAME_encode(RESOURCE_TYPE type, RESOURCE_INDEX_TYPE index_type, UINT32 param1, UINT32 param2, UINT32 param3, UINT32 param4, INT32 flag, void **data, UINT32 *data_len)
{
	if(data == NULL || data_len == NULL)
		return -1;

	switch (index_type) 
	{
		case RESOURCE_INDEX_TYPE_NONE:
			if(type == RESOURCE_TYPE_HOSTNAME)
			{
				if(flag & RESOURCE_USER_READ_FLAG == RESOURCE_USER_READ_FLAG)
				{
					*data_len = strlen(*(char **)param1);
					printf("%s %d %d, data_len %d.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),*data_len);sleep(1);
					*data = malloc(*data_len + 1);
					memcpy(*data, *(void **)param1, *data_len+1);
					printf("%s %d %d, hostname_str %s %s.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),*(char **)param1, *(char **)data);
				}
				else
				{
					*data_len = strlen((char *)param1);
					*data = malloc(*data_len + 1);
					memset(*data, 0, *data_len+1);
					memcpy(*data, (void *)param1, *data_len);
					printf("%s %d %d, hostname_str %s %s.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),(char *)param1, *(char **)data);
				}
			}
			break;
        case RESOURCE_INDEX_TYPE_STRING:
        case RESOURCE_INDEX_TYPE_DIID:
        case RESOURCE_INDEX_TYPE_ETERNALID:
        case RESOURCE_INDEX_TYPE_UINT32:
		default:
			return -2;
	}

	return 0;
}

static int __RESOURCE_TYPE_HOSTNAME_decode(RESOURCE_TYPE type, RESOURCE_INDEX_TYPE index_type, UINT32 *param1, UINT32 *param2, UINT32 *param3, UINT32 *param4, INT32 flag, void *data, UINT32 data_len)
{
	if(data == NULL || data_len == 0)
		return -1;

	printf("%s %d %d, type is %d %d %d.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),index_type, type,data_len);
	switch (index_type) 
	{
		case RESOURCE_INDEX_TYPE_NONE:
			if(type == RESOURCE_TYPE_HOSTNAME)
			{
				if(flag & RESOURCE_USER_READ_FLAG == RESOURCE_USER_READ_FLAG)
				{
					*(char **)param1 = (char *)data;
					printf("%s %d %d, hostname_str %s %s.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),*(char **)param1, (char *)data);
				}
				else
				{
					*(char **)param1 = (char *)data;
					printf("%s %d %d, hostname_str %s %s.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),*(char **)param1, (char *)data);
				}
			}
			break;
        case RESOURCE_INDEX_TYPE_STRING:
        case RESOURCE_INDEX_TYPE_DIID:
        case RESOURCE_INDEX_TYPE_ETERNALID:
        case RESOURCE_INDEX_TYPE_UINT32:
		default:
			return -2;
	}

	return 0;
}

static INT32 hostname_write (RESOURCE_TYPE type,RESOURCE_INDEX_TYPE index_type,void *index,UINT32 param1,UINT32 param2,UINT32 param3,UINT32 param4)
{
	char *name;
	char *old;
	int  rc = 0;

	assert (type == RESOURCE_TYPE_HOSTNAME
		&& index_type == RESOURCE_INDEX_TYPE_NONE
		&& param1);

	name = (char *)param1;
	printf("%s %d %d, name is %s.\n",__FUNCTION__,__LINE__, syscall(SYS_gettid),name);
	if (!name) {
		vty_output("Can't allocate memory for hostname in hostname_write.\n");
		return -2;
	}
	old = mystrdup(hostname_str);
	hostname_str = mystrdup(name);
	if (old)
	{
		sys_mem_free (old);
	}
	printf("%s %d %d, hostname_str %s %s %s.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),old, (char *)param1, hostname_str);
	rc = resource_synchronize (hostname_id, (UINT32)hostname_str, 0);
	printf("%s %d %d, %d.\n",__FUNCTION__,__LINE__, syscall(SYS_gettid), rc);

	return 0;
}

static INT32 hostname_read (RESOURCE_TYPE type,RESOURCE_INDEX_TYPE index_type,void *index,UINT32 param1,UINT32 param2,UINT32 param3,UINT32 param4)
{
	assert (type == RESOURCE_TYPE_HOSTNAME
	&& index_type == RESOURCE_INDEX_TYPE_NONE
	&& param1);

	*(char **)param1 = hostname_str;
	printf("%s %d %d, hostname_str %s %s.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),*(char **)param1, hostname_str);
	return 0;
}

static void resource_hostname_callback1(RESOURCE_ID id, unsigned int arg1, unsigned int arg2, unsigned int param1, unsigned int param2)
{
	hostname_str = (char *)param1;
	printf("%s %d %d, hostname_str %s.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),hostname_str);
	return;
}

static void resource_hostname_callback2(RESOURCE_ID id, unsigned int arg1, unsigned int arg2, unsigned int param1, unsigned int param2)
{
	hostname_str = (char *)param1;
	printf("%s %d %d, hostname_str %s.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),hostname_str);
	return;
}
#if 0
static int ubus_server()
{
	if(execlp("ubusd", "ubusd", "-s", "/run/ubus.socket", NULL) == -1)
	{
		printf("execlp ubusd failed %s!\n", strerror(errno));
		return -1;
	}
	return 0;
}
#endif

static MSG_Q_ID resource_msg = 0;

static void resource_test()
{
	int rc = 0;
	uint32 msg[4];		

	while(sys_msgq_receive(resource_msg, msg, (uint32)SYS_WAIT_FOREVER) == 0)
	{
		printf("%s %d %d,0x%x.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),resource_msg);		
	}		
}

int my24()
{
	pid_t pid;
	char *name = "resource_test";
	int rc;
	RESOURCE_ID hostname_id1, hostname_id2;
	uint32 msg[4];	

#if 0	
	pid = fork();
	if(pid == 0) {
		prctl(PR_SET_PDEATHSIG,SIGKILL);	/*父进程退出时，会收到SIGKILL信号*/
		ubus_server();
	}
	sleep(5);
#endif
#if 1
	pid = fork();
	if(pid == 0){
		//子进程上下文
		prctl(PR_SET_PDEATHSIG,SIGKILL);	/*父进程退出时，会收到SIGKILL信号*/
		//resource_init(res_param_encode, res_param_decode, NULL, res_ubus_event_register);
		sleep(5);
#if 1
		rc = resource_register_synchronizer_callback(&hostname_id1, RESOURCE_TYPE_HOSTNAME,
			RESOURCE_INDEX_TYPE_NONE, NULL, resource_hostname_callback1, 0, 0);
		printf("%s %d %d, rc %d 0x%x.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),rc,hostname_id1);
#if 0
		rc = resource_register_synchronizer_callback(&hostname_id1, RESOURCE_TYPE_HOSTNAME,
			RESOURCE_INDEX_TYPE_NONE, NULL, resource_hostname_callback2, 0, 0);
		printf("%s %d %d, rc %d 0x%x.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),rc,hostname_id1);
#endif
#endif

#if 1
		sleep(20);
		rc = resource_read(RESOURCE_TYPE_HOSTNAME, RESOURCE_INDEX_TYPE_NONE,
			NULL,(UINT32)&hostname_str, 0, 0, 0);
		printf("%s %d, rc %d.\n",__FUNCTION__,__LINE__,rc);
#endif
		while(1){
			sleep(1);
		}
	}
#endif
	pid = fork();
	if(pid == 0) {
		pthread_t tid;
		prctl(PR_SET_PDEATHSIG,SIGKILL);	/*父进程退出时，会收到SIGKILL信号*/
		sleep(5);		
		resource_msg = sys_msgq_create(10, 0);
		rc = resource_register_synchronizer_message(&hostname_id1, RESOURCE_TYPE_HOSTNAME,
			RESOURCE_INDEX_TYPE_NONE, NULL, resource_msg, 0, 0);
		printf("%s %d %d, rc %d 0x%x.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),rc,resource_msg); 
		pthread_create(&tid, NULL, (void *)resource_test,NULL);
		sleep(15);
		rc = resource_write (RESOURCE_TYPE_HOSTNAME, RESOURCE_INDEX_TYPE_NONE, 
			NULL, (UINT32)name, 0, 0, 0);
		printf("%s %d %d, rc %d.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),rc); 
		while(1){
			sleep(1);
		}
	}
	//resource_init(res_param_encode, res_param_decode, res_ubus_obj_add, NULL);
	sleep(5);
	rc = resource_register_owner (&hostname_id, RESOURCE_TYPE_HOSTNAME, RESOURCE_INDEX_TYPE_NONE, NULL, hostname_write, hostname_read);
	printf("%s %d %d, rc %d,0x%x 0x%x.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),rc, hostname_id, hostname_str);
#if 1 
	sleep(30);
	rc = resource_synchronize (hostname_id, (UINT32)hostname_str, 0);
	printf("%s %d %d, rc %d,0x%x 0x%x.\n",__FUNCTION__,__LINE__,syscall(SYS_gettid),rc, hostname_id2, hostname_str);
#endif
	while(1){
		sleep(1);
	}
	return 0;
}

