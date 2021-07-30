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
#include <libubox/uloop.h>
#include <libubox/ustream.h>
#include <libubox/utils.h>
#include <libubus.h>
#include <json-c/json.h>
#include <libubox/blobmsg_json.h>
#include <entity/entity.h>

enum  
{
	TEMPERATURE_CHASSIS,
	TEMPERATURE_SLOT,
	TEMPERATURE_INDEX,
    TEMPERATURE_HIGH_ALERT,
	TEMPERATURE_LOW_ALERT,
	TEMPERATURE_CURRENT,
	TEMPERATURE_RET,
    TEMPERATURE_POLICY_MAX,  
};  
  
static const struct blobmsg_policy temperature_policy[TEMPERATURE_POLICY_MAX] = {  
	[TEMPERATURE_CHASSIS] = {.name = "chassis", .type = BLOBMSG_TYPE_INT32},
	[TEMPERATURE_SLOT] = {.name = "slot", .type = BLOBMSG_TYPE_INT32},
	[TEMPERATURE_INDEX] = {.name = "index", .type = BLOBMSG_TYPE_INT32},
	[TEMPERATURE_HIGH_ALERT] = {.name = "high_alert", .type = BLOBMSG_TYPE_INT32},
	[TEMPERATURE_LOW_ALERT] = {.name = "low_alert", .type = BLOBMSG_TYPE_INT32},
	[TEMPERATURE_CURRENT] = {.name = "current", .type = BLOBMSG_TYPE_INT32},
	[TEMPERATURE_RET] = {.name = "ret", .type = BLOBMSG_TYPE_INT32},
};

static struct blob_buf b;
static int temperature_config_set(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
	struct blob_attr *tb[TEMPERATURE_POLICY_MAX];  

	blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[TEMPERATURE_CHASSIS] || !tb[TEMPERATURE_SLOT] || !tb[TEMPERATURE_INDEX])
        return UBUS_STATUS_INVALID_ARGUMENT;
	
	int chassis = blobmsg_get_u32(tb[TEMPERATURE_CHASSIS]);
	int slot = blobmsg_get_u32(tb[TEMPERATURE_SLOT]);
	int index = blobmsg_get_u32(tb[TEMPERATURE_INDEX]);
	printf("temperature_config_set, TEMPERATURE_CHASSIS %d\r\n", chassis);
	printf("temperature_config_set, TEMPERATURE_SLOT %d\r\n", slot);
	printf("temperature_config_set, TEMPERATURE_INDEX %d\r\n", index);
	if(tb[TEMPERATURE_HIGH_ALERT]){
		int high_alert = blobmsg_get_u32(tb[TEMPERATURE_HIGH_ALERT]);
		printf("temperature_config_set, TEMPERATURE_HIGH_ALERT %d\r\n", high_alert);
	}
	if(tb[TEMPERATURE_LOW_ALERT]){
		int low_alert = blobmsg_get_u32(tb[TEMPERATURE_LOW_ALERT]);
		printf("temperature_config_set, TEMPERATURE_LOW_ALERT %d\r\n", low_alert);
	}

	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "ret", 0);
	ubus_send_reply(ctx, req, b.head);

    return 0;
}

static int temperature_status_get(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
	struct blob_attr *tb[TEMPERATURE_POLICY_MAX];

	blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[TEMPERATURE_CHASSIS] || !tb[TEMPERATURE_SLOT] || !tb[TEMPERATURE_INDEX])
        return UBUS_STATUS_INVALID_ARGUMENT;
	
	int chassis = blobmsg_get_u32(tb[TEMPERATURE_CHASSIS]);
	int slot = blobmsg_get_u32(tb[TEMPERATURE_SLOT]);
	int index = blobmsg_get_u32(tb[TEMPERATURE_INDEX]);
	printf("temperature_status_get, TEMPERATURE_CHASSIS %d\r\n", chassis);
	printf("temperature_status_get, TEMPERATURE_SLOT %d\r\n", slot);
	printf("temperature_status_get, TEMPERATURE_INDEX %d\r\n", index);
	blob_buf_init(&b, 0);
	if(tb[TEMPERATURE_CURRENT]){
		int current = 60;
		blobmsg_add_u32(&b, "current", current);
	}
	blobmsg_add_u32(&b, "ret", 0);
	ubus_send_reply(ctx, req, b.head);

    return 0;
}


static const struct ubus_method temperature_methods[] = {
    { .name = "temperature_config_set", .handler = temperature_config_set },
    { .name = "temperature_status_get", .handler = temperature_status_get },
};

static struct ubus_object_type temperature_object_type =
    UBUS_OBJECT_TYPE("H.G.CHASSIS.SLOT.TEMPERATURE", temperature_methods);

static struct ubus_object temperature_object = {
    .name = "H.G.CHASSIS.SLOT.TEMPERATURE",
    .type = &temperature_object_type,
    .methods = temperature_methods,
    .n_methods = ARRAY_SIZE(temperature_methods),
};

/*
ubusd -s /usocket &
ubus -s /usocket list
ubus -s /usocket call temperature temperature_status_get '{ "chassis":0,"slot":1,"index":2,"current":0}'
ubus -s /usocket call temperature temperature_config_set '{ "chassis":0,"slot":1,"index":2,"high_alert":100}'
*/
static void my11_cb(struct ubus_request *req, int type, struct blob_attr *msg)  
{
	struct blob_attr *tb[TEMPERATURE_POLICY_MAX];
	blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msg), blob_len(msg));
	if(tb[TEMPERATURE_CURRENT]){
		int current = blobmsg_get_u32(tb[TEMPERATURE_CURRENT]);
		printf("my11_cb, TEMPERATURE_CURRENT %d\r\n", current);
	}
	if(tb[TEMPERATURE_RET]){
		int ret = blobmsg_get_u32(tb[TEMPERATURE_RET]);
		printf("my11_cb, TEMPERATURE_RET %d\r\n", ret);
	}
}

int my11()
{
	int pid = fork();
	if(pid == -1){
		//父进程创建子进程失败
		printf("fork failed\r\n");
		return -1;
	}else if(pid == 0){
		//子进程上下文
		sleep(5);
		int rv;
		struct ubus_context *ctx;
		unsigned int obj_id;
		ctx = ubus_connect(PATH);
	    if (!ctx) {
	        printf("Failed to connect to ubus\n");
	        return -1;
	    }

		/* 
	    向ubusd查询是否存在"scan_prog"这个对象， 
	    如果存在，返回其id 
	    */  
	    rv = ubus_lookup_id(ctx, "H.G.CHASSIS.SLOT.TEMPERATURE", &obj_id);  
	    if (rv != UBUS_STATUS_OK) {  
	        printf("lookup temperature failed\n");  
	        return rv;  
	    }  
	    else {  
	        printf("lookup temperature successs\n");  
	    }

		blob_buf_init(&b, 0);
		blobmsg_add_u32(&b, "chassis", 0);
		blobmsg_add_u32(&b, "slot", 1);
		blobmsg_add_u32(&b, "index", 2);
		blobmsg_add_u32(&b, "current", 0);
		ubus_invoke(ctx, obj_id, "temperature_status_get", b.head, my11_cb, NULL, 5 * 1000);	    

		blob_buf_init(&b, 0);
		blobmsg_add_u32(&b, "chassis", 0);
		blobmsg_add_u32(&b, "slot", 1);
		blobmsg_add_u32(&b, "index", 2);
		blobmsg_add_u32(&b, "high_alert", 100);		
		ubus_invoke(ctx, obj_id, "temperature_config_set", b.head, my11_cb, NULL, 5 * 1000); 	

		while(1){
			sleep(1);
		}
		return 0;
	}else{
		//父进程上下文
		int rv;
		struct ubus_context *ctx;
		
		uloop_init();

		ctx = ubus_connect(PATH);
	    if (!ctx) {
	        printf("Failed to connect to ubus\n");
	        return -1;
	    }
		ubus_add_uloop(ctx);

		rv = ubus_add_object(ctx, &temperature_object);
	    if (rv)
	        printf("Failed to add zboard object: %s\n", ubus_strerror(rv));
		
	    uloop_run();
	}

	return 0;
}

