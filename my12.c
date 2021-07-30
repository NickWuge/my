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

static struct ubus_method temperature_alert_methods[] = {};
  
static struct ubus_object_type temperature_alert_object_type = 
    UBUS_OBJECT_TYPE("H.G.CHASSIS.SLOT.TEMPERATURE.ALERT_EVENT", temperature_alert_methods);

static void temperature_alert_subscribe_cb(struct ubus_context *ctx, struct ubus_object *obj)
{
    printf("Subscribers active: %d\n", obj->has_subscribers);
}

static struct ubus_object temperature_alert_object = {  
    .name = "H.G.CHASSIS.SLOT.TEMPERATURE.ALERT_EVENT", /* object的名字 */  
    .type = &temperature_alert_object_type,  
    .subscribe_cb = temperature_alert_subscribe_cb,  
};

static struct ubus_subscriber alert_event;
static unsigned int obj_id;

static int my12_cb(struct ubus_context *ctx, struct ubus_object *obj,  
                  struct ubus_request_data *req,  
                  const char *method, struct blob_attr *msg)  
{
	printf("my12_cb\r\n");
	struct blob_attr *tb[TEMPERATURE_POLICY_MAX];
	blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msg), blob_len(msg));
	if(tb[TEMPERATURE_CHASSIS]){
		int chassis = blobmsg_get_u32(tb[TEMPERATURE_CHASSIS]);
		printf("my12_cb, TEMPERATURE_CHASSIS %d\r\n", chassis);
	}
	if(tb[TEMPERATURE_SLOT]){
		int slot = blobmsg_get_u32(tb[TEMPERATURE_SLOT]);
		printf("my12_cb, TEMPERATURE_SLOT %d\r\n", slot);
	}
	if(tb[TEMPERATURE_INDEX]){
		int index = blobmsg_get_u32(tb[TEMPERATURE_INDEX]);
		printf("my12_cb, TEMPERATURE_INDEX %d\r\n", index);
	}
	if(tb[TEMPERATURE_CURRENT]){
		int current = blobmsg_get_u32(tb[TEMPERATURE_CURRENT]);
		printf("my12_cb, TEMPERATURE_CURRENT %d\r\n", current);
	}
	if(tb[TEMPERATURE_HIGH_ALERT]){
		int high_alert = blobmsg_get_u32(tb[TEMPERATURE_HIGH_ALERT]);
		printf("my12_cb, TEMPERATURE_HIGH_ALERT %d\r\n", high_alert);
	}
	if(tb[TEMPERATURE_LOW_ALERT]){
		int low_alert = blobmsg_get_u32(tb[TEMPERATURE_LOW_ALERT]);
		printf("my12_cb, TEMPERATURE_LOW_ALERT %d\r\n", low_alert);
	}
	ubus_unsubscribe(ctx, &alert_event, obj_id); /* 取消订阅 */  
    return 0;  
}  

static void *my12_fn(void *arg)
{
	int rv;
	struct ubus_context *ctx = arg;
	while (1) {  
		sleep(2);
		blob_buf_init(&b, 0);
		blobmsg_add_u32(&b, "chassis", 0);
		blobmsg_add_u32(&b, "slot", 1);
		blobmsg_add_u32(&b, "index", 2);
		blobmsg_add_u32(&b, "current", 60);
		blobmsg_add_u32(&b, "high_alert", 100);
		blobmsg_add_u32(&b, "low_alert", 0);
		printf("ubus_notify\r\n");
		rv = ubus_notify(ctx, &temperature_alert_object, "say Hi!", b.head, -1);
		if (rv)  
			printf("Failed to notify: %s\n", ubus_strerror(rv));
	}
    return NULL;
}

int my12(){
	
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

		uloop_init();
		
		ctx = ubus_connect(PATH);
		if (!ctx) {
			printf("Failed to connect to ubus\n");
			return -1;
		}
		ubus_add_uloop(ctx);

		/* 通知到来时的处理函数。 */  
		alert_event.cb = my12_cb;  
		alert_event.remove_cb = NULL; //server主动发起删除该client的订阅的cb函数（如server退出的时候） 

		/* 注册test_event */  
		rv = ubus_register_subscriber(ctx, &alert_event);  
		if (rv)  
			printf("Failed to add watch handler: %s\n", ubus_strerror(rv));  

		/* 得到要订阅的object的id */  
		rv = ubus_lookup_id(ctx, "H.G.CHASSIS.SLOT.TEMPERATURE.ALERT_EVENT", &obj_id);  
		if (rv)  
			printf("Failed to lookup object: %s\n", ubus_strerror(rv));  

		/* 订阅object */  
		rv = ubus_subscribe(ctx, &alert_event, obj_id);  
		if (rv)  
			printf("Failed to subscribe: %s\n", ubus_strerror(rv));

		uloop_run();

		return 0;
	}else{
		//父进程上下文
		int rv;
		struct ubus_context *ctx;
		int counter = 10;
		pthread_t tid;

		uloop_init();

		ctx = ubus_connect(PATH);
	    if (!ctx) {
	        printf("Failed to connect to ubus\n");
	        return -1;
	    }
		ubus_add_uloop(ctx);

		rv = ubus_add_object(ctx, &temperature_alert_object);
	    if (rv)
	        printf("Failed to add zboard object: %s\n", ubus_strerror(rv));

		pthread_create(&tid, NULL, my12_fn, ctx);

	    uloop_run();
		pthread_join(tid, NULL);
	}

	return 0;
}

