#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libubus.h>
#include <entity/entity.h>
#include <hashmap.h>
#include <resource.h>

#define OBJ_COUNT			256
#define OBJ_NAME_LENGTH		32

#define RESID_OBJMAP_SIZE	hashmap_size(OBJ_COUNT*2, OBJ_NAME_LENGTH+sizeof(int))

#define TEMPERATURE_OBJECT "L.L.CHASSIS.SLOT.TEMPERATURE"
#define FAN_OBJECT "L.C.CHASSIS.FAN"

static void _enumerator(const char *key, uint32_t keySize, const void *data, uint32_t length, void *context)
{
	*(unsigned int *)context = *(unsigned int *)data;
}

int my19(){
	int pid = fork();
	if(pid == -1){
		//父进程创建子进程失败
		printf("fork failed\r\n");
		return -1;
	}else if(pid == 0){
		sleep(5);
		//子进程上下文
		int rv;
		hashmap_t *hashmap;
		unsigned int data1 = 0, data2 = 0;
		hashmap = resource_open("OBJMAP", RESMODE_RO);
		if(!hashmap){
			printf("resource_open OBJMAP failed\r\n");
			return -1;
		}
		rv = hashmap_find(hashmap, TEMPERATURE_OBJECT, sizeof(TEMPERATURE_OBJECT), (enumerator_t)_enumerator, &data1);
		if(rv != E_OK){
			resource_close(hashmap);
			printf("hashmap_find TEMPERATURE_OBJECT failed:%d\r\n", rv);
			return -1;
		}
		rv = hashmap_find(hashmap, FAN_OBJECT, sizeof(FAN_OBJECT), (enumerator_t)_enumerator, &data2);
		if(rv != E_OK){
			resource_close(hashmap);
			printf("hashmap_find FAN_OBJECT failed:%d\r\n", rv);
			return -1;
		}
		resource_close(hashmap);
		printf("TEMPERATURE_OBJECT:0x%x\r\n", data1);
		printf("FAN_OBJECT:0x%x\r\n", data2);
		
		return 0;
	}else{
		//父进程上下文
		int rv;
		hashmap_t *hashmap;
		unsigned int data1 = 0x01234567, data2 = 0x89abcdef;
		rv = resource_create("OBJMAP", NULL, RESID_OBJMAP_SIZE);
		if(rv < 0){
			printf("resource_create OBJMAP failed\r\n");
			return -1;
		}
		hashmap = resource_open("OBJMAP", RESMODE_RW);
		if(!hashmap){
			printf("resource_open OBJMAP failed\r\n");
			return -1;
		}
		hashmap_init(hashmap, OBJ_COUNT*2, OBJ_NAME_LENGTH+sizeof(int));
		rv = hashmap_insert(hashmap, TEMPERATURE_OBJECT, sizeof(TEMPERATURE_OBJECT), &data1, sizeof(unsigned int));
		if(rv != E_OK){
			resource_close(hashmap);
			printf("hashmap_insert TEMPERATURE_OBJECT failed\r\n");
			return -1;
		}
		rv = hashmap_insert(hashmap, FAN_OBJECT, sizeof(FAN_OBJECT), &data2, sizeof(unsigned int));
		if(rv != E_OK){
			resource_close(hashmap);
			printf("hashmap_insert FAN_OBJECT failed\r\n");
			return -1;
		}
		resource_close(hashmap);
	}
	
	return 0;
}

