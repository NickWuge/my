#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
#define container_of(ptr, type, member)					\
		({								\
			const __typeof__(((type *) NULL)->member) *__mptr = (ptr);	\
			(type *) ((char *) __mptr - offsetof(type, member));	\
		})
#endif

enum resource_e{
	RES_1 = 1,
	RES_MAX
};

enum mode_e{
	MODE_RO,
	MODE_RW,
};

struct resource{
	pthread_rwlock_t rwlock;
	char data[0];
};

struct user_data{
	int status;
};

static int _create(enum resource_e resource, void *data, int size)
{
	int id;
	key_t key;
	pthread_rwlockattr_t     attr;
	struct resource *p;
	
	key = ftok("/res", resource);
	if(key < 0)
	{
		printf("get key error\n");
		return -1;
	}
	id = shmget(key, sizeof(struct resource) + size, IPC_CREAT|0666);	/*IPC_EXCL*/
	if(id < 0)
	{
		printf("get id error\n");
		return -1;
	}

	pthread_rwlockattr_init(&attr);
	pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	p = shmat(id, NULL, 0);
	pthread_rwlock_init(&p->rwlock, &attr);
	if(data){
		pthread_rwlock_wrlock(&p->rwlock);
		memcpy(p->data, data, size);
		pthread_rwlock_unlock(&p->rwlock);
	}
	shmdt(p);

	return 0;
}

static void * _open(enum resource_e resource, int size, enum mode_e mode)
{
	int id;
	key_t key;
	struct resource *p;

	key = ftok("/res", resource);
	if(key < 0)
	{
		printf("get key error\n");
		return NULL;
	}
	id = shmget(key, sizeof(struct resource) + size, 0666);
	if(id < 0)
	{
		printf("get id error\n");
		return NULL;
	}

	if(mode == MODE_RO){
		p = shmat(id, NULL, 0);	/*SHM_RDONLY*/
		pthread_rwlock_rdlock(&p->rwlock);
	}else{
		p = shmat(id, NULL, 0);
		pthread_rwlock_wrlock(&p->rwlock);
	}

	return p->data;
}

static void _close(void *data)
{
	struct resource *p;
	p = container_of(data, struct resource, data);
	pthread_rwlock_unlock(&p->rwlock);
	shmdt(p);
}

int my18(){
	int pid = fork();
	if(pid == -1){
		//父进程创建子进程失败
		printf("fork failed\r\n");
		return -1;
	}else if(pid == 0){
		sleep(5);
		//子进程上下文
		int rv;
		struct user_data *p;
		int status;

		while(1){
			p = _open(RES_1, sizeof(struct user_data), MODE_RO);
			status = p->status;
			_close(p);
			printf("%d\r\n", status);
			sleep(1);
		}
		return 0;
	}else{
		//父进程上下文
		int rv;
		struct user_data *p;

		p->status = 0;
		rv = _create(RES_1, p, sizeof(struct user_data));
		if(rv < 0)
		{
			printf("_create error\n");
			return -1;
		}

		while(1){
			p = _open(RES_1, sizeof(struct user_data), MODE_RW);
			p->status ++;
			_close(p);
			sleep(1);
		}
	}
	
	return 0;
}

