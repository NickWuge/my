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

static void temperature_alert_subscribe_cb(struct ubus_context *ctx, struct ubus_object *obj);
static int temperature_create(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg);
static int temperature_destroy(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg);
static int temperature_config_set(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg);
static int temperature_status_get(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg);

enum process_e{
	P_TEMD,
	P_FAND,
	P_TEM,
	P_FAN,
	PROCESS_MAX
};

typedef struct{
	char *name;
	int priority;
	int(*func)();
}subprocess_t;


enum  
{
	TEMPERATURE_INDEX,
	TEMPERATURE_NAME,
    TEMPERATURE_HIGH_ALERT,
	TEMPERATURE_LOW_ALERT,
	TEMPERATURE_CURRENT,
	TEMPERATURE_DEFAULT_HIGH_ALERT,
	TEMPERATURE_DEFAULT_LOW_ALERT,
	TEMPERATURE_RET,
    TEMPERATURE_POLICY_MAX,  
};  

enum
{
	TEMPERATURE_ALERT_NORMAL,
	TEMPERATURE_ALERT_HIGH,
	TEMPERATURE_ALERT_LOW,
};

struct temperature_s{
	char name[16];
	int high_alert;
	int low_alert;
	int last_current;
	int last_alert;
};

static const struct blobmsg_policy temperature_policy[TEMPERATURE_POLICY_MAX] = {  
	[TEMPERATURE_INDEX] = {.name = "index", .type = BLOBMSG_TYPE_STRING},
	[TEMPERATURE_NAME] = {.name = "name", .type = BLOBMSG_TYPE_STRING},
	[TEMPERATURE_HIGH_ALERT] = {.name = "high_alert", .type = BLOBMSG_TYPE_INT32},
	[TEMPERATURE_LOW_ALERT] = {.name = "low_alert", .type = BLOBMSG_TYPE_INT32},
	[TEMPERATURE_CURRENT] = {.name = "current", .type = BLOBMSG_TYPE_INT32},
	[TEMPERATURE_RET] = {.name = "ret", .type = BLOBMSG_TYPE_INT32},
	[TEMPERATURE_DEFAULT_HIGH_ALERT] = {.name = "default_high_alert", .type = BLOBMSG_TYPE_INT32},
	[TEMPERATURE_DEFAULT_LOW_ALERT] = {.name = "default_low_alert", .type = BLOBMSG_TYPE_INT32},
};

#define TEMPERATURE_OBJECT "L.L.CHASSIS.SLOT.TEMPERATURE"
static const struct ubus_method temperature_methods[] = {
	{ .name = "create", .handler = temperature_create },
	{ .name = "destroy", .handler = temperature_destroy },
    { .name = "config_set", .handler = temperature_config_set },
    { .name = "status_get", .handler = temperature_status_get },
};
static struct ubus_object_type temperature_object_type =
    UBUS_OBJECT_TYPE(TEMPERATURE_OBJECT, temperature_methods);
static struct ubus_object temperature_object = {
    .name = TEMPERATURE_OBJECT,
    .type = &temperature_object_type,
    .methods = temperature_methods,
    .n_methods = ARRAY_SIZE(temperature_methods),
};

#define TEMPERATURE_ALERT_EVENT "L.L.CHASSIS.SLOT.TEMPERATURE.ALERT_EVENT"

static struct temperature_s temperature[2];
static struct blob_buf b;

static void temperature_alert_subscribe_cb(struct ubus_context *ctx, struct ubus_object *obj)
{
    printf("Subscribers active: %d\n", obj->has_subscribers);
}

static int stub_cpu_temperature_get(){
	static int temperature = 60;
	if(temperature < 90)
		temperature+=2;
	else
		temperature = 60;
	
	return temperature;
}

static int stub_switch_temperature_get(){
	static int temperature = 80;
	if(temperature < 110)
		temperature+=2;
	else
		temperature = 80;
	
	return temperature;
}

static int temperature_create(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
	int rv = 0;
	int chassis, slot, index;
	struct blob_attr *tb[TEMPERATURE_POLICY_MAX];  

	blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[TEMPERATURE_INDEX])
        return UBUS_STATUS_INVALID_ARGUMENT;

	sscanf(blobmsg_get_string(tb[TEMPERATURE_INDEX]), "%d.%d.%d", &chassis, &slot, &index);
	if(index > 2)
		rv = -1;
	else{
		char *name = blobmsg_get_string(tb[TEMPERATURE_NAME]);
		int high_alert = blobmsg_get_u32(tb[TEMPERATURE_HIGH_ALERT]);
		int low_alert = blobmsg_get_u32(tb[TEMPERATURE_LOW_ALERT]);
		int current = blobmsg_get_u32(tb[TEMPERATURE_CURRENT]);
		strncpy(temperature[index].name, name, 15);
		temperature[index].high_alert = high_alert;
		temperature[index].low_alert = low_alert;
		temperature[index].last_current = current;
		temperature[index].last_alert = TEMPERATURE_ALERT_NORMAL;
	}

	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "ret", rv);
	ubus_send_reply(ctx, req, b.head);

    return 0;
}

static int temperature_destroy(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
	int rv = 0;
	int chassis, slot, index;
	struct blob_attr *tb[TEMPERATURE_POLICY_MAX];  

	blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[TEMPERATURE_INDEX])
        return UBUS_STATUS_INVALID_ARGUMENT;

	sscanf(blobmsg_get_string(tb[TEMPERATURE_INDEX]), "%d.%d.%d", &chassis, &slot, &index);
	if(index > 2)
		rv = -1;
	else{
		memset(&temperature[index], 0, sizeof(struct temperature_s));
	}

	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "ret", rv);
	ubus_send_reply(ctx, req, b.head);

    return 0;
}

static int temperature_config_set(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
	int rv = 0;
	int chassis, slot, index;
	struct blob_attr *tb[TEMPERATURE_POLICY_MAX];  

	blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[TEMPERATURE_INDEX])
        return UBUS_STATUS_INVALID_ARGUMENT;

	sscanf(blobmsg_get_string(tb[TEMPERATURE_INDEX]), "%d.%d.%d", &chassis, &slot, &index);
	if(index > 2)
		rv = -1;
	else{
		if(tb[TEMPERATURE_NAME]){
			char *name = blobmsg_get_string(tb[TEMPERATURE_NAME]);
			strncpy(temperature[index].name, name, 15);
		}
		if(tb[TEMPERATURE_HIGH_ALERT]){
			int high_alert = blobmsg_get_u32(tb[TEMPERATURE_HIGH_ALERT]);
			temperature[index].high_alert = high_alert;
		}
		if(tb[TEMPERATURE_LOW_ALERT]){
			int low_alert = blobmsg_get_u32(tb[TEMPERATURE_LOW_ALERT]);
			temperature[index].low_alert = low_alert;
		}
	}

	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "ret", rv);
	ubus_send_reply(ctx, req, b.head);

    return 0;
}

static int temperature_status_get(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
	int rv;
	int chassis, slot, index;
	struct blob_attr *tb[TEMPERATURE_POLICY_MAX];

	blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[TEMPERATURE_INDEX])
        return UBUS_STATUS_INVALID_ARGUMENT;

	sscanf(blobmsg_get_string(tb[TEMPERATURE_INDEX]), "%d.%d.%d", &chassis, &slot, &index);
	blob_buf_init(&b, 0);
	if(tb[TEMPERATURE_CURRENT]){
		int current = 0;
		rv = 0;
		if(index == 0)
			current = stub_cpu_temperature_get();
		else if(index == 1)
			current = stub_switch_temperature_get();
		else
			rv = -1;
		temperature[index].last_current = current;
		blobmsg_add_u32(&b, "current", current);
	}
	blobmsg_add_u32(&b, "ret", rv);
	ubus_send_reply(ctx, req, b.head);

    return 0;
}

static void *temperature_poll_thread(void *arg){
	static struct blob_buf b;
	struct ubus_context *ctx = arg;
	int current;
	int alert;
	struct blob_attr *ret;

	while(1){
		//轮询CPU温度
		if(temperature[0].name){
			current = stub_cpu_temperature_get();
			if(temperature[0].last_current != current){
				temperature[0].last_current = current;
				blob_buf_init(&b, 0);
				blobmsg_add_string(&b, "index", "0.0.0");
				blobmsg_add_u32(&b, "current", current);
				local_entity_status_change(ctx, TEMPERATURE_OBJECT, b.head, ret);
				if(temperature[0].last_current > temperature[0].high_alert){
					alert = TEMPERATURE_ALERT_HIGH;
				}else if(temperature[0].last_current < temperature[0].low_alert){
					alert = TEMPERATURE_ALERT_LOW;
				}else{
					alert = TEMPERATURE_ALERT_NORMAL;
				}
				//事件通知
				if(alert != temperature[0].last_alert){
					temperature[0].last_alert = alert;
					blobmsg_add_u32(&b, "high_alert", temperature[0].high_alert);
					blobmsg_add_u32(&b, "low_alert", temperature[0].low_alert);
					local_entity_event_notify(ctx, TEMPERATURE_OBJECT, TEMPERATURE_ALERT_EVENT, b.head);
				}
			}
		}

		//轮询SWITCH温度
		if(temperature[1].name){
			current = stub_switch_temperature_get();
			if(temperature[1].last_current != current){
				temperature[1].last_current = current;
				blob_buf_init(&b, 0);
				blobmsg_add_string(&b, "index", "0.0.1");
				blobmsg_add_u32(&b, "current", current);
				local_entity_status_change(ctx, TEMPERATURE_OBJECT, b.head, ret);
				if(temperature[1].last_current > temperature[1].high_alert){
					alert = TEMPERATURE_ALERT_HIGH;
				}else if(temperature[1].last_current < temperature[1].low_alert){
					alert = TEMPERATURE_ALERT_LOW;
				}else{
					alert = TEMPERATURE_ALERT_NORMAL;
				}
				//事件通知
				if(alert != temperature[1].last_alert){
					temperature[1].last_alert = alert;
					blob_buf_init(&b, 0);
					blobmsg_add_u32(&b, "high_alert", temperature[1].high_alert);
					blobmsg_add_u32(&b, "low_alert", temperature[1].low_alert);
					local_entity_event_notify(ctx, TEMPERATURE_OBJECT, TEMPERATURE_ALERT_EVENT, b.head);
				}
			}
		}
		sleep(2);
	}

	return NULL;
}

static int temperature_daemon_process(){
	int rv;
	struct ubus_context *ctx;
	pthread_t tid;

	uloop_init();
	ctx = ubus_connect(PATH);
	if (!ctx) {
		printf("Failed to connect to ubus\n");
		return -1;
	}
	ubus_add_uloop(ctx);

	//注册温度对象方法
	rv = ubus_add_object(ctx, &temperature_object);
    if (rv)
        printf("Failed to add zboard object: %s\n", ubus_strerror(rv));
	
	//创建温度查询线程
	pthread_create(&tid, NULL, temperature_poll_thread, ctx);
	
	uloop_run();
	ubus_free(ctx);  
	uloop_done();
	pthread_join(tid, NULL);
}

static void temperature_alert_event(struct ubus_context *ctx, struct ubus_event_handler *ev,  
              const char *type, struct blob_attr *msg)  
{
	char *str;
	if (!msg)
		return;

	str = blobmsg_format_json(msg, true);
	printf("{ \"%s\": %s }\n", type, str);
	free(str);

	//温度告警
}

static int temperature_process(){
	int rv;
	struct ubus_context *ctx;

	sleep(5);	//wait for temperature_daemon

	uloop_init();
	ctx = ubus_connect(PATH);
	if (!ctx) {
		printf("Failed to connect to ubus\n");
		return -1;
	}
	ubus_add_uloop(ctx);

	//注册监控CPU的温度对象实体
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "index", "0.0.0");
	blobmsg_add_string(&b, "name", "CPU");
	blobmsg_add_u32(&b, "high_alert", 80);
	blobmsg_add_u32(&b, "low_alert", -10);
	blobmsg_add_u32(&b, "current", 40);
	blobmsg_add_u32(&b, "default_high_alert", 80);
	blobmsg_add_u32(&b, "default_low_alert", -10);
	rv = local_entity_create(ctx, TEMPERATURE_OBJECT, b.head);
	if(rv != 0)
		printf("local_entity_create failed:%d\r\n", rv);
	
	//注册监控交换芯片的温度对象实体
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "index", "0.0.1");
	blobmsg_add_string(&b, "name", "SWITCH");
	blobmsg_add_u32(&b, "high_alert", 100);
	blobmsg_add_u32(&b, "low_alert", -10);
	blobmsg_add_u32(&b, "current", 50);
	blobmsg_add_u32(&b, "default_high_alert", 100);
	blobmsg_add_u32(&b, "default_low_alert", -10);
	rv = local_entity_create(ctx, TEMPERATURE_OBJECT, b.head);
	if(rv != 0)
		printf("local_entity_create failed:%d\r\n", rv);

	//监控温度报警事件
	struct ubus_event_handler listener;
    memset(&listener, 0, sizeof(listener));
    listener.cb = temperature_alert_event;
    rv = ubus_register_event_handler(ctx, &listener, TEMPERATURE_ALERT_EVENT);
	if (rv)
        printf("ubus_register_event_handler failed: %s\n", ubus_strerror(rv));

	uloop_run();
	ubus_free(ctx);
	uloop_done();

	return 0;
}

static int fan_create(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg);
static int fan_destroy(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg);
static int fan_config_set(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg);

enum
{
	FAN_INDEX,
	FAN_NAME,
	FAN_GEAR,
	FAN_RET,
	FAN_POLICY_MAX,
};

enum
{
	FAN_GEAR_LOW,
	FAN_GEAR_HIGH,
};

struct fan_s{
	char name[16];
	int gear;
};

static const struct blobmsg_policy fan_policy[FAN_POLICY_MAX] = {  
	[FAN_INDEX] = {.name = "index", .type = BLOBMSG_TYPE_STRING},
	[FAN_NAME] = {.name = "name", .type = BLOBMSG_TYPE_STRING},
	[FAN_GEAR] = {.name = "gear", .type = BLOBMSG_TYPE_INT32},
	[FAN_RET] = {.name = "ret", .type = BLOBMSG_TYPE_INT32},
};

#define FAN_OBJECT "L.C.CHASSIS.FAN"
static const struct ubus_method fan_methods[] = {
	{ .name = "create", .handler = fan_create },
	{ .name = "destroy", .handler = fan_destroy },
    { .name = "config_set", .handler = fan_config_set },
};
static struct ubus_object_type fan_object_type =
    UBUS_OBJECT_TYPE(FAN_OBJECT, fan_methods);
static struct ubus_object fan_object = {
    .name = FAN_OBJECT,
    .type = &fan_object_type,
    .methods = fan_methods,
    .n_methods = ARRAY_SIZE(fan_methods),
};

struct fan_s fan;

static int stub_fan_gear_set(int gear){
	return 0;
}

static int fan_create(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
	int rv = 0;
	int chassis, index;
	struct blob_attr *tb[FAN_POLICY_MAX];  

	blobmsg_parse(fan_policy, FAN_POLICY_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[FAN_INDEX])
        return UBUS_STATUS_INVALID_ARGUMENT;

	sscanf(blobmsg_get_string(tb[FAN_INDEX]), "%d.%d", &chassis, &index);
	if(index > 1)
		rv = -1;
	else{
		char *name = blobmsg_get_string(tb[FAN_NAME]);
		int gear = blobmsg_get_u32(tb[FAN_GEAR]);
		strncpy(fan.name, name, 15);
		stub_fan_gear_set(gear);
		fan.gear = gear;
	}

	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "ret", rv);
	ubus_send_reply(ctx, req, b.head);

    return 0;
}

static int fan_destroy(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
	int rv = 0;
	int chassis, index;
	struct blob_attr *tb[FAN_POLICY_MAX];  

	blobmsg_parse(fan_policy, FAN_POLICY_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[FAN_INDEX])
        return UBUS_STATUS_INVALID_ARGUMENT;

	sscanf(blobmsg_get_string(tb[FAN_INDEX]), "%d.%d", &chassis, &index);
	if(index > 1)
		rv = -1;
	else{
		memset(&fan, 0, sizeof(struct fan_s));
	}

	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "ret", rv);
	ubus_send_reply(ctx, req, b.head);

    return 0;
}

static int fan_config_set(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
	int rv = 0;
	int chassis, index;
	struct blob_attr *tb[FAN_POLICY_MAX];  

	blobmsg_parse(fan_policy, FAN_POLICY_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[FAN_INDEX])
        return UBUS_STATUS_INVALID_ARGUMENT;

	sscanf(blobmsg_get_string(tb[FAN_INDEX]), "%d.%d", &chassis, &index);
	if(index > 1)
		rv = -1;
	else{
		if(tb[FAN_NAME]){
			char *name = blobmsg_get_string(tb[FAN_NAME]);
			strncpy(fan.name, name, 15);
		}
		if(tb[FAN_GEAR]){
			int gear = blobmsg_get_u32(tb[FAN_GEAR]);
			stub_fan_gear_set(gear);
			fan.gear = gear;
		}
	}

	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "ret", rv);
	ubus_send_reply(ctx, req, b.head);

    return 0;
}

static int fan_daemon_process(){
	int rv;
	struct ubus_context *ctx;

	uloop_init();
	ctx = ubus_connect(PATH);
	if (!ctx) {
		printf("Failed to connect to ubus\n");
		return -1;
	}
	ubus_add_uloop(ctx);

	//注册风扇对象方法
	rv = ubus_add_object(ctx, &fan_object);
    if (rv)
        printf("Failed to add zboard object: %s\n", ubus_strerror(rv));

	uloop_run();
	ubus_free(ctx);  
	uloop_done();

	return 0;
}

static int fan_process(){
	int rv;
	struct ubus_context *ctx;
	struct blob_attr *msgOut;
	struct blob_attr *tb[TEMPERATURE_POLICY_MAX];
	int current;
	int last_gear, cur_gear;

	sleep(5);	//wait for fan_daemon

	ctx = ubus_connect(PATH);
	if (!ctx) {
		printf("Failed to connect to ubus\n");
		return -1;
	}

	//注册风扇管理对象实体
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "index", "0.0");
	blobmsg_add_string(&b, "name", "FAN");
	last_gear = 0;
	blobmsg_add_u32(&b, "gear", last_gear);
	rv = local_entity_create(ctx, FAN_OBJECT, b.head);
	if(rv != 0)
		printf("local_entity_create failed:%d\r\n", rv);

	//轮询SWITCH的温度设置风扇转速
	while(1){
		//轮询SWITCH的温度
		blob_buf_init(&b, 0);
		blobmsg_add_string(&b, "index", "0.0.1");
		blobmsg_add_u32(&b, "current", 0);
		rv = local_entity_status_cache_get(ctx, TEMPERATURE_OBJECT, b.head, &msgOut);
		if(rv != 0){
			printf("local_entity_status_cache_get failed:%d\r\n", rv);
			sleep(1);
			continue;
		}
		blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msgOut), blob_len(msgOut));
		if(!tb[TEMPERATURE_CURRENT]){
			printf("missing msgOut\r\n");
			sleep(1);
			continue;
		}
		current = blobmsg_get_u32(tb[TEMPERATURE_CURRENT]);

		//设置风扇转速
		cur_gear = current/10;
		if(last_gear != cur_gear){
			blob_buf_init(&b, 0);
			blobmsg_add_string(&b, "index", "0.0");
			blobmsg_add_u32(&b, "gear", cur_gear);
			rv = local_entity_config_set(ctx, FAN_OBJECT, b.head, &msgOut);
			if(rv != 0){
				printf("local_entity_config_set failed:%d\r\n", rv);
				sleep(1);
				continue;
			}
			last_gear = cur_gear;
		}
		sleep(1);
	}

	ubus_free(ctx);
	return 0;
}

static subprocess_t subprocesses[PROCESS_MAX] = {
	{"TEMD", 60, temperature_daemon_process},
	{"FAND", 60, fan_daemon_process},
	{"TEM", 60, temperature_process},
	{"FAN", 60, fan_process},
};

int my14(){
	int i;
	pid_t pid[PROCESS_MAX] = {0};

	/*创建业务进程*/
	for(i=0;i<PROCESS_MAX;i++){
		pid[i] = fork();
		if(pid[i] == -1){
			//父进程创建子进程失败
			printf("fork %s failed\r\n", subprocesses[i].name);
			return -1;
		}else if(pid[i] == 0){
			//子进程上下文
			if(!subprocesses[i].func)
				return 0;
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
			subprocesses[i].func();
			return 0;
		}
	}

	struct ubus_context *ctx;
	struct blob_attr *msgOut;
	struct blob_attr *tb[TEMPERATURE_POLICY_MAX];	//>FAN_POLICY_MAX
	char cmd[100];
	int rv;
	
	ctx = ubus_connect(PATH);
    if (!ctx) {
        printf("Failed to connect to ubus\n");
        return -1;
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
		fgets(cmd, 100, stdin);
		printf("exec %s\r\n", cmd);
		if(strcmp(cmd, "config_get\n") == 0){
			blob_buf_init(&b, 0);
			blobmsg_add_string(&b, "index", "0.0.0");
			blobmsg_add_u32(&b, "high_alert", 0);
			blobmsg_add_u32(&b, "low_alert", 0);
			rv = local_entity_config_get(ctx, TEMPERATURE_OBJECT, b.head, &msgOut);
			if(rv != 0)
				printf("local_entity_config_get failed:%d\r\n", rv);
			blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msgOut), blob_len(msgOut));
			if(!tb[TEMPERATURE_HIGH_ALERT] || !tb[TEMPERATURE_LOW_ALERT]){
				printf("missing msgOut\r\n");
				continue;
			}else{
				printf("local_entity_config_get %s %s\r\n", TEMPERATURE_OBJECT, "0.0.0");
				printf("TEMPERATURE_HIGH_ALERT:%d\r\n", blobmsg_get_u32(tb[TEMPERATURE_HIGH_ALERT]));
				printf("TEMPERATURE_LOW_ALERT:%d\r\n", blobmsg_get_u32(tb[TEMPERATURE_LOW_ALERT]));
			}
			blob_buf_init(&b, 0);
			blobmsg_add_string(&b, "index", "0.0.1");
			blobmsg_add_u32(&b, "high_alert", 0);
			blobmsg_add_u32(&b, "low_alert", 0);
			rv = local_entity_config_get(ctx, TEMPERATURE_OBJECT, b.head, &msgOut);
			if(rv != 0)
				printf("local_entity_config_get failed:%d\r\n", rv);
			blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msgOut), blob_len(msgOut));
			if(!tb[TEMPERATURE_HIGH_ALERT] || !tb[TEMPERATURE_LOW_ALERT]){
				printf("missing msgOut\r\n");
				continue;
			}else{
				printf("local_entity_config_get %s %s\r\n", TEMPERATURE_OBJECT, "0.0.1");
				printf("TEMPERATURE_HIGH_ALERT:%d\r\n", blobmsg_get_u32(tb[TEMPERATURE_HIGH_ALERT]));
				printf("TEMPERATURE_LOW_ALERT:%d\r\n", blobmsg_get_u32(tb[TEMPERATURE_LOW_ALERT]));
			}
			blob_buf_init(&b, 0);
			blobmsg_add_string(&b, "index", "0.0");
			blobmsg_add_u32(&b, "gear", 0);
			rv = local_entity_config_get(ctx, FAN_OBJECT, b.head, &msgOut);
			if(rv != 0)
				printf("local_entity_config_get failed:%d\r\n", rv);
			blobmsg_parse(fan_policy, FAN_POLICY_MAX, tb, blob_data(msgOut), blob_len(msgOut));
			if(!tb[FAN_GEAR]){
				printf("missing msgOut\r\n");
				continue;
			}else{
				printf("local_entity_config_get %s %s\r\n", FAN_OBJECT, "0.0");
				printf("FAN_GEAR:%d\r\n", blobmsg_get_u32(tb[FAN_GEAR]));
			}
		}else if(strcmp(cmd, "status_get\n") == 0){
			blob_buf_init(&b, 0);
			blobmsg_add_string(&b, "index", "0.0.0");
			blobmsg_add_u32(&b, "current", 0);
			rv = local_entity_status_get(ctx, TEMPERATURE_OBJECT, b.head, &msgOut);
			if(rv != 0)
				printf("local_entity_status_get failed:%d\r\n", rv);
			blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msgOut), blob_len(msgOut));
			if(!tb[TEMPERATURE_CURRENT]){
				printf("missing msgOut\r\n");
				continue;
			}else{
				printf("local_entity_status_get %s %s\r\n", TEMPERATURE_OBJECT, "0.0.0");
				printf("TEMPERATURE_CURRENT:%d\r\n", blobmsg_get_u32(tb[TEMPERATURE_CURRENT]));
			}
			blob_buf_init(&b, 0);
			blobmsg_add_string(&b, "index", "0.0.1");
			blobmsg_add_u32(&b, "current", 0);;
			rv = local_entity_status_get(ctx, TEMPERATURE_OBJECT, b.head, &msgOut);
			if(rv != 0)
				printf("local_entity_status_get failed:%d\r\n", rv);
			blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msgOut), blob_len(msgOut));
			if(!tb[TEMPERATURE_CURRENT]){
				printf("missing msgOut\r\n");
				continue;
			}else{
				printf("local_entity_status_get %s %s\r\n", TEMPERATURE_OBJECT, "0.0.1");
				printf("TEMPERATURE_CURRENT:%d\r\n", blobmsg_get_u32(tb[TEMPERATURE_CURRENT]));
			}
		}else if(strcmp(cmd, "status_cache_get\n") == 0){
			blob_buf_init(&b, 0);
			blobmsg_add_string(&b, "index", "0.0.0");
			blobmsg_add_u32(&b, "current", 0);
			rv = local_entity_status_cache_get(ctx, TEMPERATURE_OBJECT, b.head, &msgOut);
			if(rv != 0)
				printf("local_entity_status_cache_get failed:%d\r\n", rv);
			blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msgOut), blob_len(msgOut));
			if(!tb[TEMPERATURE_CURRENT]){
				printf("missing msgOut\r\n");
				continue;
			}else{
				printf("local_entity_status_cache_get %s %s\r\n", TEMPERATURE_OBJECT, "0.0.0");
				printf("TEMPERATURE_CURRENT:%d\r\n", blobmsg_get_u32(tb[TEMPERATURE_CURRENT]));
			}
			blob_buf_init(&b, 0);
			blobmsg_add_string(&b, "index", "0.0.1");
			blobmsg_add_u32(&b, "current", 0);;
			rv = local_entity_status_cache_get(ctx, TEMPERATURE_OBJECT, b.head, &msgOut);
			if(rv != 0)
				printf("local_entity_status_cache_get failed:%d\r\n", rv);
			blobmsg_parse(temperature_policy, TEMPERATURE_POLICY_MAX, tb, blob_data(msgOut), blob_len(msgOut));
			if(!tb[TEMPERATURE_CURRENT]){
				printf("missing msgOut\r\n");
				continue;
			}else{
				printf("local_entity_status_cache_get %s %s\r\n", TEMPERATURE_OBJECT, "0.0.1");
				printf("TEMPERATURE_CURRENT:%d\r\n", blobmsg_get_u32(tb[TEMPERATURE_CURRENT]));
			}
		}else if(strcmp(cmd, "config_set\n") == 0){
			//to be added
		}else if(strcmp(cmd, "exit\n") == 0){
			break;
		}
	}
	printf("cmd loop stop\r\n");

	for(i=0;i<PROCESS_MAX;i++){
		kill(pid[i], SIGTERM);
		waitpid(pid[i], NULL, 0);
	}
	
	return 0;
}

