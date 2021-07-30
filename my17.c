#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <libubox/blobmsg_json.h>

enum
{
	INDEX,
	ATTRIBUTE,
	ENTITYS,
    POLICY_MAX,
};

static const struct blobmsg_policy policy[POLICY_MAX] = {
	[INDEX] = {.name = "index", .type = BLOBMSG_TYPE_STRING},
	[ATTRIBUTE] = {.name = "attribute", .type = BLOBMSG_TYPE_TABLE},
	[ENTITYS] = {.name = "entitys", .type = BLOBMSG_TYPE_ARRAY},
};

//测试建立entity
static void my17_test1()
{
	struct blob_buf b;
	void *cookie;
	struct blob_attr *tb[POLICY_MAX];
	struct blob_attr *cur;
	int rem;
	char *str;

	printf("%s\r\n", __FUNCTION__);

	//封装entity
	memset(&b, 0, sizeof(struct blob_buf));
	blob_buf_init(&b, 0);
	blobmsg_add_string(&b, "index", "0.1.0");
	cookie = blobmsg_open_table(&b, "attribute");
	blobmsg_add_string(&b, "name", "CPU");
	blobmsg_add_u32(&b, "high_alert", 80);
	blobmsg_add_u32(&b, "low_alert", -10);
	blobmsg_add_u32(&b, "current", 40);
	blobmsg_add_u32(&b, "default_high_alert", 80);
	blobmsg_add_u32(&b, "default_low_alert", -10);
	blobmsg_close_table(&b, cookie);

	//打印entity
	str = blobmsg_format_json_indent(b.head, true, 0);
	printf("%s\n", str);
	free(str);
	
	//解封装entity
	blobmsg_parse(policy, POLICY_MAX, tb, blob_data(b.head), blob_len(b.head));
	if(!tb[INDEX]){
		printf("missing index\r\n");
	}
	printf("index:%s\r\n", blobmsg_get_string(tb[INDEX]));

	if(!tb[ATTRIBUTE]){
		printf("missing attribute\r\n");
	}
	blobmsg_for_each_attr(cur, tb[ATTRIBUTE], rem){
		switch(blobmsg_type(cur)){
			case BLOBMSG_TYPE_INT32:
				printf("%s:%d\r\n", blobmsg_name(cur), blobmsg_get_u32(cur));
				break;
			case BLOBMSG_TYPE_STRING:
				printf("%s:%s\r\n", blobmsg_name(cur), blobmsg_get_string(cur));
				break;
			default:
				break;
		}
	}

	blob_buf_free(&b);
}

//测试用已有属性来建立entity
static void my17_test2()
{
	static struct blob_buf b1, b2;
	void *cookie;
	struct blob_attr *tb[POLICY_MAX];
	struct blob_attr *cur;
	int rem;
	char *str;

	printf("%s\r\n", __FUNCTION__);

	//已有属性
	memset(&b1, 0, sizeof(struct blob_buf));
	blob_buf_init(&b1, 0);
	blobmsg_add_string(&b1, "name", "CPU");
	blobmsg_add_u32(&b1, "high_alert", 80);
	blobmsg_add_u32(&b1, "low_alert", -10);
	blobmsg_add_u32(&b1, "current", 40);
	blobmsg_add_u32(&b1, "default_high_alert", 80);
	blobmsg_add_u32(&b1, "default_low_alert", -10);

	//封装entity
	memset(&b2, 0, sizeof(struct blob_buf));
	blob_buf_init(&b2, 0);
	blobmsg_add_string(&b2, "index", "0.1.0");
	cookie = blobmsg_open_table(&b2, "attribute");
	blob_put_raw(&b2, blob_data(b1.head), blob_len(b1.head));
	blobmsg_close_table(&b2, cookie);

	//打印entity
	str = blobmsg_format_json_indent(b2.head, true, 0);
	printf("%s\n", str);
	free(str);

	//解封装entity
	blobmsg_parse(policy, POLICY_MAX, tb, blob_data(b2.head), blob_len(b2.head));
	if(!tb[INDEX]){
		printf("missing index\r\n");
	}
	printf("index:%s\r\n", blobmsg_get_string(tb[INDEX]));

	if(!tb[ATTRIBUTE]){
		printf("missing attribute\r\n");
	}
	blobmsg_for_each_attr(cur, tb[ATTRIBUTE], rem){
		switch(blobmsg_type(cur)){
			case BLOBMSG_TYPE_INT32:
				printf("%s:%d\r\n", blobmsg_name(cur), blobmsg_get_u32(cur));
				break;
			case BLOBMSG_TYPE_STRING:
				printf("%s:%s\r\n", blobmsg_name(cur), blobmsg_get_string(cur));
				break;
			default:
				break;
		}
	}

	blob_buf_free(&b1);
	blob_buf_free(&b2);
}

//测试建立1个包含2个entity的array
static void my17_test3()
{
	static struct blob_buf b, b1, b2;
	void *cookie, *array;
	struct blob_attr *tb_entitys[POLICY_MAX];
	struct blob_attr *tb_entity[POLICY_MAX];
	struct blob_attr *entity, *attribute;
	int rem;
	char *str;

	//封装第1个entity
	memset(&b1, 0, sizeof(struct blob_buf));
	blob_buf_init(&b1, 0);
	blobmsg_add_string(&b1, "index", "0.1.0");
	cookie = blobmsg_open_table(&b1, "attribute");
	blobmsg_add_string(&b1, "name", "CPU");
	blobmsg_add_u32(&b1, "high_alert", 80);
	blobmsg_add_u32(&b1, "low_alert", -10);
	blobmsg_add_u32(&b1, "current", 40);
	blobmsg_add_u32(&b1, "default_high_alert", 80);
	blobmsg_add_u32(&b1, "default_low_alert", -10);
	blobmsg_close_table(&b1, cookie);

	//封装第2个entity
	memset(&b2, 0, sizeof(struct blob_buf));
	blob_buf_init(&b2, 0);
	blobmsg_add_string(&b2, "index", "0.1.1");
	cookie = blobmsg_open_table(&b2, "attribute");
	blobmsg_add_string(&b2, "name", "SWITCH");
	blobmsg_add_u32(&b2, "high_alert", 100);
	blobmsg_add_u32(&b2, "low_alert", -10);
	blobmsg_add_u32(&b2, "current", 50);
	blobmsg_add_u32(&b2, "default_high_alert", 100);
	blobmsg_add_u32(&b2, "default_low_alert", -10);
	blobmsg_close_table(&b2, cookie);

	//封装entity的array
	memset(&b, 0, sizeof(struct blob_buf));
	blob_buf_init(&b, 0);
	array = blobmsg_open_array(&b, "entitys");
	cookie = blobmsg_open_table(&b, NULL);
	blob_put_raw(&b, blob_data(b1.head), blob_len(b1.head));
	blobmsg_close_table(&b, cookie);
	cookie = blobmsg_open_table(&b, NULL);
	blob_put_raw(&b, blob_data(b2.head), blob_len(b2.head));
	blobmsg_close_table(&b, cookie);
	blobmsg_close_array(&b, array);

	//打印array
	str = blobmsg_format_json_indent(b.head, true, 0);
	printf("%s\n", str);
	free(str);

	//解封装array及entity
	blobmsg_parse(policy, POLICY_MAX, tb_entitys, blob_data(b.head), blob_len(b.head));
	if(!tb_entitys[ENTITYS]){
		printf("missing entitys\r\n");
	}
	blobmsg_for_each_attr(entity, tb_entitys[ENTITYS], rem){
		blobmsg_parse(policy, POLICY_MAX, tb_entity, blobmsg_data(entity), blobmsg_data_len(entity));
		if(!tb_entity[INDEX]){
			printf("missing index\r\n");
		}
		printf("index:%s\r\n", blobmsg_get_string(tb_entity[INDEX]));

		if(!tb_entity[ATTRIBUTE]){
			printf("missing attribute\r\n");
		}
		blobmsg_for_each_attr(attribute, tb_entity[ATTRIBUTE], rem){
			switch(blobmsg_type(attribute)){
				case BLOBMSG_TYPE_INT32:
					printf("%s:%d\r\n", blobmsg_name(attribute), blobmsg_get_u32(attribute));
					break;
				case BLOBMSG_TYPE_STRING:
					printf("%s:%s\r\n", blobmsg_name(attribute), blobmsg_get_string(attribute));
					break;
				default:
					break;
			}
		}
	}

	blob_buf_free(&b1);
	blob_buf_free(&b2);
	blob_buf_free(&b);
}


int my17(){
	my17_test1();
	my17_test2();
	my17_test3();
	
	return 0;
}

