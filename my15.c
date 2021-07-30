#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sys/times.h>
#include <hiredis/hiredis.h>

#define NGIOSDB_OK		0
#define NGIOSDB_ERROR	-1

#define NGIOSDB_ERR_MAX_LENGTH	128
typedef struct ngiosdb_conn_s{
	redisContext *c;
	unsigned flag;
	int curr_db;
	char errstr[NGIOSDB_ERR_MAX_LENGTH];
}ngiosdb_conn_t;

/*
git clone https://github.com/redis/hiredis
cmake -D CMAKE_INSTALL_PREFIX=/usr ./
make
sudo make install
*/
void *ngiosdb_connect(unsigned flag)
{
	ngiosdb_conn_t *ngiosdb_conn = NULL;
	redisContext *c = NULL;

	ngiosdb_conn = (ngiosdb_conn_t *)malloc(sizeof(ngiosdb_conn_t));
	if(!ngiosdb_conn)
		return NULL;

	memset(ngiosdb_conn, 0, sizeof(ngiosdb_conn_t));

	c = redisConnect("127.0.0.1", 6379);
	if (c == NULL) {
		free(ngiosdb_conn);
		//printf("Connection error: can't allocate redis context\n");
		return NULL;
    } else if (c->err) {
		free(ngiosdb_conn);
		//printf("Connection error: %s\n", c->errstr);
		redisFree(c);
		return NULL;
    }

	ngiosdb_conn->c = c;
	ngiosdb_conn->flag = flag;
	ngiosdb_conn->curr_db = 0;

	return ngiosdb_conn;
}

void ngiosdb_disconnect(void *conn)
{
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;

	if(ngiosdb_conn != NULL){
		if(ngiosdb_conn->c != NULL)
			redisFree(ngiosdb_conn->c);
		free(ngiosdb_conn);
	}
}

static int _ngiosdb_select(ngiosdb_conn_t *ngiosdb_conn, int db)
{
	redisReply *reply = NULL;
	int ret = NGIOSDB_OK;
	
	if(ngiosdb_conn->curr_db != db){
		reply = redisCommand(ngiosdb_conn->c, "SELECT %d", db);
		switch(reply->type){
			case REDIS_REPLY_STATUS:	/*5*/
				/*OK*/
				ngiosdb_conn->curr_db = db;
				break;
			case REDIS_REPLY_ERROR:		/*6*/
				/*ERROR*/
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
				goto exit;
			case REDIS_REPLY_STRING:	/*1*/
			case REDIS_REPLY_INTEGER:	/*3*/
			case REDIS_REPLY_ARRAY:		/*2*/
			case REDIS_REPLY_NIL:		/*4*/
			default:
				/*ERROR, unreachable*/
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
				goto exit;
		}
		freeReplyObject(reply);
		reply = NULL;
	}

exit:
	if(reply)
		freeReplyObject(reply);
	return ret;
}

int ngiosdb_set(void *conn, int db, char *key, void *value, int len)
{
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;
	redisContext *c;
	redisReply *reply = NULL;
	int ret = NGIOSDB_OK;
	
	if(ngiosdb_conn == NULL || ngiosdb_conn->c == NULL){
		snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "parameter error");
		return NGIOSDB_ERROR;
	}else
		c = ngiosdb_conn->c;

	/*select db*/
	ret = _ngiosdb_select(ngiosdb_conn, db);
	if(ret != NGIOSDB_OK)
		goto exit;

	/*start SET*/
	reply = redisCommand(c, "SET %s %b", key, value, len);
	switch(reply->type){
		case REDIS_REPLY_STATUS:	/*5*/
			/*OK*/
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			/*ERROR*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_INTEGER:	/*3*/
		case REDIS_REPLY_ARRAY:		/*2*/
		case REDIS_REPLY_NIL:		/*4*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

exit:
	if(reply)
		freeReplyObject(reply);
	return ret;
}

int ngiosdb_get(void *conn, int db, char *key, void *value, int *len)
{
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;
	redisContext *c;
	redisReply *reply = NULL;
	int ret = NGIOSDB_OK;
	
	if(ngiosdb_conn == NULL || ngiosdb_conn->c == NULL){
		snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "parameter error");
		return NGIOSDB_ERROR;
	}else
		c = ngiosdb_conn->c;

	/*select db*/
	ret = _ngiosdb_select(ngiosdb_conn, db);
	if(ret != NGIOSDB_OK)
		goto exit;

	/*start GET*/
	reply = redisCommand(c, "GET %s", key);
	switch(reply->type){
		case REDIS_REPLY_STRING:	/*1*/
			if(reply->len <= *len){
				memcpy(value, reply->str, reply->len);
				*len = reply->len;
			}else{
				memcpy(value, reply->str, *len);
			}
			break;		
		case REDIS_REPLY_NIL:		/*4*/
			/*NULL*/
			*len = 0;
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			/*ERROR*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			goto exit;
		case REDIS_REPLY_INTEGER:	/*3*/
		case REDIS_REPLY_ARRAY:		/*2*/
		case REDIS_REPLY_STATUS:	/*5*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

exit:
	if(reply)
		freeReplyObject(reply);
	return ret;
}

int ngiosdb_del(void *conn, int db, char *key)
{
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;
	redisContext *c;
	redisReply *reply = NULL;
	int ret = NGIOSDB_OK;
	
	if(ngiosdb_conn == NULL || ngiosdb_conn->c == NULL){
		snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "parameter error");
		return NGIOSDB_ERROR;
	}else
		c = ngiosdb_conn->c;

	/*select db*/
	ret = _ngiosdb_select(ngiosdb_conn, db);
	if(ret != NGIOSDB_OK)
		goto exit;

	/*start DEL*/
	reply = redisCommand(c, "DEL %s", key);
	switch(reply->type){
		case REDIS_REPLY_INTEGER:	/*3*/
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			break;
			/*ERROR*/
			//ret = NGIOSDB_ERROR;
			//snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			//goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_ARRAY:		/*2*/
		case REDIS_REPLY_NIL:		/*4*/
		case REDIS_REPLY_STATUS:	/*5*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

exit:
	if(reply)
		freeReplyObject(reply);
	return ret;
}

int ngiosdb_sets(void *conn, int db, int key_num, char **keys, void **values, int *lens)
{
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;
	redisContext *c;
	redisReply *reply = NULL, *subReply = NULL;
	int ret = NGIOSDB_OK, i, j;

	if(key_num == 1)
		return ngiosdb_set(conn, db, keys[0], values[0], lens[0]);

	if(ngiosdb_conn == NULL || ngiosdb_conn->c == NULL){
		snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "parameter error");
		return NGIOSDB_ERROR;
	}else
		c = ngiosdb_conn->c;

	/*select db*/
	ret = _ngiosdb_select(ngiosdb_conn, db);
	if(ret != NGIOSDB_OK)
		goto exit;

	/*start MULTI*/
	reply = redisCommand(c, "MULTI");
	switch(reply->type){
		case REDIS_REPLY_STATUS:	/*5*/
			/*OK*/
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			/*ERROR*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_INTEGER:	/*3*/
		case REDIS_REPLY_ARRAY:		/*2*/
		case REDIS_REPLY_NIL:		/*4*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

	/*in queue*/
	for(i=0;i<key_num;i++){
		if(redisAppendCommand(c, "SET %s %b", keys[i], values[i], lens[i]) != REDIS_OK){
			for(j=0;j<i;j++){
				redisGetReply(c, (void**)&reply);
				freeReplyObject(reply);
				reply = NULL;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
				ret = NGIOSDB_ERROR;
				goto exit;
			}
		}
	}

	for(i=0;i<key_num;i++){
		redisGetReply(c, (void**)&reply);
		switch(reply->type){
			case REDIS_REPLY_STATUS:	/*5*/
				/*QUEUED*/
				break;
			case REDIS_REPLY_ERROR:		/*6*/
				/*ERROR*/
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
				continue;
			case REDIS_REPLY_STRING:	/*1*/
			case REDIS_REPLY_INTEGER:	/*3*/
			case REDIS_REPLY_ARRAY:		/*2*/
			case REDIS_REPLY_NIL:		/*4*/
			default:
				/*ERROR, unreachable*/
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
				continue;
		}
	}

	/*start EXEC*/
	reply = redisCommand(c, "EXEC");
	switch(reply->type){
		case REDIS_REPLY_ARRAY:		/*2*/
			for(i=0;i<reply->elements;i++){
				subReply = reply->element[i];
				switch(subReply->type){
					case REDIS_REPLY_STATUS:	/*5*/
						/*OK*/
						break;
					case REDIS_REPLY_ERROR:		/*6*/
						/*ERROR*/
						ret = NGIOSDB_ERROR;
						snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", subReply->str);
						goto exit;
					case REDIS_REPLY_STRING:	/*1*/
					case REDIS_REPLY_INTEGER:	/*3*/
					case REDIS_REPLY_ARRAY:		/*2*/
					case REDIS_REPLY_NIL:		/*4*/
					default:
						/*ERROR, unreachable*/
						ret = NGIOSDB_ERROR;
						snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
						goto exit;
				}
			}
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			/*ERROR*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_INTEGER:	/*3*/
		case REDIS_REPLY_NIL:		/*4*/
		case REDIS_REPLY_STATUS:	/*5*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

exit:
	if(reply)
		freeReplyObject(reply);
	return ret;
}

int ngiosdb_gets(void *conn, int db, int key_num, char **keys, void **values, int *lens)
{
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;
	redisContext *c;
	redisReply *reply = NULL, *subReply = NULL;
	int ret = NGIOSDB_OK, i, j;

	if(key_num == 1)
		return ngiosdb_get(conn, db, keys[0], values[0], &lens[0]);
	
	if(ngiosdb_conn == NULL || ngiosdb_conn->c == NULL){
		snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "parameter error");
		return NGIOSDB_ERROR;
	}else
		c = ngiosdb_conn->c;

	/*select db*/
	ret = _ngiosdb_select(ngiosdb_conn, db);
	if(ret != NGIOSDB_OK)
		goto exit;

	/*start MULTI*/
	reply = redisCommand(c, "MULTI");
	switch(reply->type){
		case REDIS_REPLY_STATUS:	/*5*/
			/*OK*/
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			/*ERROR*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_INTEGER:	/*3*/
		case REDIS_REPLY_ARRAY:		/*2*/
		case REDIS_REPLY_NIL:		/*4*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

	/*in queue*/
	for(i=0;i<key_num;i++){
		if(redisAppendCommand(c, "GET %s", keys[i]) != REDIS_OK){
			for(j=0;j<i;j++){
				redisGetReply(c, (void**)&reply);
				freeReplyObject(reply);
				reply = NULL;
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
				goto exit;
			}
		}
	}
	for(i=0;i<key_num;i++){
		redisGetReply(c, (void**)&reply);
		switch(reply->type){
			case REDIS_REPLY_STATUS:	/*5*/
				/*QUEUED*/
				break;
			case REDIS_REPLY_ERROR:		/*6*/
				/*ERROR*/
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
				continue;
			case REDIS_REPLY_STRING:	/*1*/
			case REDIS_REPLY_INTEGER:	/*3*/
			case REDIS_REPLY_ARRAY:		/*2*/
			case REDIS_REPLY_NIL:		/*4*/
			default:
				/*ERROR, unreachable*/
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
				continue;
		}
	}

	/*start EXEC*/
	reply = redisCommand(c, "EXEC");
	switch(reply->type){
		case REDIS_REPLY_ARRAY:		/*2*/
			for(i=0;i<reply->elements;i++){
				subReply = reply->element[i];
				switch(subReply->type){
					case REDIS_REPLY_STRING:	/*1*/
						if(subReply->len <= lens[i]){
							memcpy(values[i], subReply->str, subReply->len);
							lens[i] = subReply->len;
						}else{
							memcpy(values[i], subReply->str, lens[i]);
						}
						break;		
					case REDIS_REPLY_NIL:		/*4*/
						/*NULL*/
						lens[i] = 0;
						break;
					case REDIS_REPLY_ERROR:		/*6*/
						/*ERROR*/
						ret = NGIOSDB_ERROR;
						snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", subReply->str);
						goto exit;
					case REDIS_REPLY_INTEGER:	/*3*/
					case REDIS_REPLY_ARRAY:		/*2*/
					case REDIS_REPLY_STATUS:	/*5*/
					default:
						/*ERROR, unreachable*/
						ret = NGIOSDB_ERROR;
						snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
						goto exit;
				}
			}
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			/*ERROR*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_INTEGER:	/*3*/
		case REDIS_REPLY_NIL:		/*4*/
		case REDIS_REPLY_STATUS:	/*5*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

exit:
	if(reply)
		freeReplyObject(reply);
	return ret;
}

int ngiosdb_dels(void *conn, int db, int key_num, char **keys)
{
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;
	redisContext *c;
	redisReply *reply = NULL, *subReply = NULL;
	int ret = NGIOSDB_OK, i, j;

	if(key_num== 1)
		return ngiosdb_del(conn, db, keys[0]);
	
	if(ngiosdb_conn == NULL || ngiosdb_conn->c == NULL){
		snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "parameter error");
		return NGIOSDB_ERROR;
	}else
		c = ngiosdb_conn->c;

	/*select db*/
	ret = _ngiosdb_select(ngiosdb_conn, db);
	if(ret != NGIOSDB_OK)
		goto exit;

	/*start MULTI*/
	reply = redisCommand(c, "MULTI");
	switch(reply->type){
		case REDIS_REPLY_STATUS:	/*5*/
			/*OK*/
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			/*ERROR*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_INTEGER:	/*3*/
		case REDIS_REPLY_ARRAY:		/*2*/
		case REDIS_REPLY_NIL:		/*4*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

	/*in queue*/
	for(i=0;i<key_num;i++){
		ret = redisAppendCommand(c, "DEL %s", keys[i]);
		if(ret != REDIS_OK){
			for(j=0;j<i;j++){
				redisGetReply(c, (void**)&reply);
				freeReplyObject(reply);
				reply = NULL;
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
				goto exit;
			}
		}
	}
	for(i=0;i<key_num;i++){
		redisGetReply(c, (void**)&reply);
		switch(reply->type){
			case REDIS_REPLY_STATUS:	/*5*/
				/*QUEUED*/
				break;
			case REDIS_REPLY_ERROR:		/*6*/
				/*ERROR*/
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
				continue;
			case REDIS_REPLY_STRING:	/*1*/
			case REDIS_REPLY_INTEGER:	/*3*/
			case REDIS_REPLY_ARRAY:		/*2*/
			case REDIS_REPLY_NIL:		/*4*/
			default:
				/*ERROR, unreachable*/
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
				continue;
		}
	}

	/*start EXEC*/
	reply = redisCommand(c, "EXEC");
	switch(reply->type){
		case REDIS_REPLY_ARRAY:		/*2*/
			for(i=0;i<reply->elements;i++){
				subReply = reply->element[i];
				switch(subReply->type){
					case REDIS_REPLY_INTEGER:	/*3*/
						/*OK，返回被删除 key 的数�?/
						break;
					case REDIS_REPLY_ERROR:		/*6*/
						break;
						/*ERROR*/
						//ret = NGIOSDB_ERROR;
						//snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", subReply->str);
						//goto exit;
					case REDIS_REPLY_STRING:	/*1*/
					case REDIS_REPLY_ARRAY:		/*2*/
					case REDIS_REPLY_NIL:		/*4*/
					case REDIS_REPLY_STATUS:	/*5*/
					default:
						/*ERROR, unreachable*/
						ret = NGIOSDB_ERROR;
						snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
						goto exit;
				}
			}
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			/*ERROR*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_INTEGER:	/*3*/
		case REDIS_REPLY_NIL:		/*4*/
		case REDIS_REPLY_STATUS:	/*5*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

exit:
	if(reply)
		freeReplyObject(reply);
	return ret;
}

int ngiosdb_publish(void *conn, char *channel, void *message, int len)
{
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;
	redisContext *c;
	redisReply *reply = NULL;
	int ret = NGIOSDB_OK;
	
	if(ngiosdb_conn == NULL || ngiosdb_conn->c == NULL){
		snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "parameter error");
		return NGIOSDB_ERROR;
	}else
		c = ngiosdb_conn->c;

	reply = redisCommand(c, "PUBLISH %s %b", channel, message, len);
	switch(reply->type){
		case REDIS_REPLY_INTEGER:	/*3*/
			/*OK，返回接收到信息的订阅者数�?/
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			break;
			/*ERROR*/
			//ret = NGIOSDB_ERROR;
			//snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			//goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_ARRAY:		/*2*/
		case REDIS_REPLY_NIL:		/*4*/
		case REDIS_REPLY_STATUS:	/*5*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

exit:
	if(reply)
		freeReplyObject(reply);
	return ret;
}

int ngiosdb_subscribe(void *conn, char *channels)
{
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;
	redisContext *c;
	redisReply *reply = NULL;
	int ret = NGIOSDB_OK;

	if(ngiosdb_conn == NULL || ngiosdb_conn->c == NULL){
		snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "parameter error");
		return NGIOSDB_ERROR;
	}else
		c = ngiosdb_conn->c;

	reply = redisCommand(c,"SUBSCRIBE %s", channels);
	switch(reply->type){
		case REDIS_REPLY_ARRAY:		/*2*/
			/*OK*/
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			/*ERROR*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_INTEGER:	/*3*/
		case REDIS_REPLY_NIL:		/*4*/
		case REDIS_REPLY_STATUS:	/*5*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

exit:
	if(reply)
		freeReplyObject(reply);
	return ret;
}

int ngiosdb_unsubscribe(void *conn, char *channels)
{
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;
	redisContext *c;
	redisReply *reply = NULL;
	int ret = NGIOSDB_OK;

	if(ngiosdb_conn == NULL || ngiosdb_conn->c == NULL){
		snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "parameter error");
		return NGIOSDB_ERROR;
	}else
		c = ngiosdb_conn->c;

	reply = redisCommand(c,"UNSUBSCRIBE %s", channels);
	switch(reply->type){
		case REDIS_REPLY_ARRAY:		/*2*/
			/*OK*/
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			/*ERROR*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_INTEGER:	/*3*/
		case REDIS_REPLY_NIL:		/*4*/
		case REDIS_REPLY_STATUS:	/*5*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}
	freeReplyObject(reply);
	reply = NULL;

exit:
	if(reply)
		freeReplyObject(reply);
	return ret;
}

int ngiosdb_message(void *conn, char *channel, int clen, void *message, int *mlen)
{
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;
	redisContext *c;
	redisReply *reply = NULL, *subReply;
	int ret = NGIOSDB_OK;

	if(ngiosdb_conn == NULL || ngiosdb_conn->c == NULL){
		snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "parameter error");
		return NGIOSDB_ERROR;
	}else
		c = ngiosdb_conn->c;

	redisGetReply(c, (void**)&reply);
	switch(reply->type){
		case REDIS_REPLY_ARRAY:		/*2*/
			if(reply->elements != 3){
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
				goto exit;
			}
			subReply = reply->element[0];
			if(subReply->type != REDIS_REPLY_STRING || strcmp(subReply->str, "message") != 0){
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
				goto exit;
			}
			subReply = reply->element[1];
			if(subReply->type != REDIS_REPLY_STRING){
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
				goto exit;
			}else if(subReply->len >= clen){
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "parameter error2");
				goto exit;
			}else{
				strcpy(channel, subReply->str);
			}
			subReply = reply->element[2];
			if(subReply->type == REDIS_REPLY_ERROR){
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", subReply->str);
				goto exit;
			}else if(subReply->type != REDIS_REPLY_STRING){
				ret = NGIOSDB_ERROR;
				snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
				goto exit;
			}else{
				if(subReply->len <= *mlen){
					memcpy(message, subReply->str, subReply->len);
					*mlen = subReply->len;
				}else{
					memcpy(message, subReply->str, *mlen);
				}
			}
			break;
		case REDIS_REPLY_ERROR:		/*6*/
			/*ERROR*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "reply error, %s", reply->str);
			goto exit;
		case REDIS_REPLY_STRING:	/*1*/
		case REDIS_REPLY_INTEGER:	/*3*/
		case REDIS_REPLY_NIL:		/*4*/
		case REDIS_REPLY_STATUS:	/*5*/
		default:
			/*ERROR, unreachable*/
			ret = NGIOSDB_ERROR;
			snprintf(ngiosdb_conn->errstr, NGIOSDB_ERR_MAX_LENGTH, "internal error, %s %d", __FUNCTION__, __LINE__);
			goto exit;
	}

exit:
	if(reply)
		freeReplyObject(reply);
	return ret;
}

char* ngiosdb_error(void *conn){
	ngiosdb_conn_t *ngiosdb_conn = (ngiosdb_conn_t *)conn;

	if(ngiosdb_conn == NULL || ngiosdb_conn->c == NULL)
		return "";

	return ngiosdb_conn->errstr;
}

void my15(void)
{
	void *conn = NULL, *conn_subscribe = NULL, *conn_publish[2] = {NULL, NULL};
	int ret, i;
	int config_min_db = 0;
	int config_max_db = 9;

	conn = ngiosdb_connect(0);
	if(!conn){
		printf("Connection error\n");
		return;
	}

	conn_subscribe = ngiosdb_connect(0);
	if(!conn_subscribe){
		printf("Connection error\n");
		goto exit;
	}

	for(i=0;i<2;i++){
		conn_publish[i] = ngiosdb_connect(0);
		if(!conn_publish[i]){
			printf("Connection error\n");
			goto exit;
		}
	}

	/*test db&key*/
	{
		char strval[4][16];
		int len;

		ret = ngiosdb_set(conn, config_min_db, "db&key0", "db0&key0", sizeof("db0&key0"));
		ret |= ngiosdb_set(conn, config_min_db, "db&key1", "db0&key1", sizeof("db0&key1"));
		ret |= ngiosdb_set(conn, config_max_db, "db&key0", "db1&key0", sizeof("db1&key0"));
		ret |= ngiosdb_set(conn, config_max_db, "db&key1", "db1&key1", sizeof("db1&key1"));
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn));
			goto exit;
		}

		memset(strval, 0, sizeof(char)*4*16);
		len = 16;
		ret = ngiosdb_get(conn, config_min_db, "db&key0", strval[0], &len);
		len = 16;
		ret = ngiosdb_get(conn, config_min_db, "db&key1", strval[1], &len);
		len = 16;
		ret = ngiosdb_get(conn, config_max_db, "db&key0", strval[2], &len);
		len = 16;
		ret = ngiosdb_get(conn, config_max_db, "db&key1", strval[3], &len);
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn));
			goto exit;
		}

		if((0 != strcmp(strval[0], "db0&key0")) || \
			(0 != strcmp(strval[1], "db0&key1")) || \
			(0 != strcmp(strval[2], "db1&key0")) || \
			(0 != strcmp(strval[3], "db1&key1"))){
			printf("test db&key error\n");
			printf("%s, %s\n", "db0&key0", strval[0]);
			printf("%s, %s\n", "db0&key1", strval[1]);
			printf("%s, %s\n", "db1&key0", strval[2]);
			printf("%s, %s\n", "db1&key1", strval[3]);
			goto exit;
		}else{
			printf("test db&key ok\n");
		}
	}

	/*test SET&GET&DEL*/
	{
		unsigned char buf[1024];
		int len;

		for(i=0;i<1024;i++)
			buf[i] = (unsigned char)i;

		ret = ngiosdb_set(conn, config_min_db, "SET&GET&DEL", buf, 1024);
		memset(buf, 0, 1024);
		len = 1024;
		ret |= ngiosdb_get(conn, config_min_db, "SET&GET&DEL", buf, &len);
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn));
			goto exit;
		}else{
			if(len != 1024){
				printf("test SET&GET&DEL error\n");
				printf("GET length error, %d %d\n", 1024, len);
				goto exit;
			}
			for(i=0;i<1024;i++){
				if(buf[i] != (unsigned char)i){
					printf("test SET&GET&DEL error\n");
					printf("Get value error, %d %d\n", i, buf[i]);
					goto exit;
				}
			}
		}

		ret = ngiosdb_del(conn, config_min_db, "SET&GET&DEL");
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn));
			goto exit;
		}
		len = 1024;
		ret = ngiosdb_get(conn, config_min_db, "SET&GET&DEL", buf, &len);
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn));
			goto exit;
		}
		if(len != 0){
			printf("test SET&GET&DEL error\n");
			printf("GET length error, %d %d\n", 0, len);
			goto exit;
		}else{
			printf("test SET&GET&DEL ok\n");
		}
	}

	/*test SETS&GETS&DELS*/
	{
		char keys_buffer[1024][32];
		unsigned char vals_buffer[1024][1];
		char *keys[1024];
		unsigned char *vals[1024];
		int lens[1024];

		for(i=0;i<1024;i++){
			keys[i] = keys_buffer[i];
			vals[i] = vals_buffer[i];
			snprintf(keys[i], 32, "SETS&GETS&DELS%d", i);
			vals[i][0] = (unsigned char)i;
			lens[i] = 1;
		}

		ret = ngiosdb_sets(conn, config_min_db, 1024, keys, (void**)vals, lens);
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn));
			goto exit;
		}
		memset(vals_buffer, 0, sizeof(unsigned char)*1024*1);
		ret = ngiosdb_gets(conn, config_min_db, 1024, keys, (void**)vals, lens);
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn));
			goto exit;
		}
		for(i=0;i<1024;i++){
			if(lens[i] != 1){
				printf("test SETS&GETS&DELS error\n");
				printf("GET length error, %d %d\n", 1, lens[i]);
				goto exit;
			}else if(vals[i][0] != (unsigned char)i){
				printf("test SETS&GETS&DELS error\n");
				printf("GET value error, %d %d\n", i, vals[i][0]);
				goto exit;
			}
			
		}

		ret = ngiosdb_dels(conn, config_min_db, 1024, keys);
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn));
			goto exit;
		}
		ret = ngiosdb_gets(conn, config_min_db, 1024, keys, (void**)vals, lens);
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn));
			goto exit;
		}
		for(i=0;i<1024;i++){
			if(lens[i] != 0){
				printf("test SETS&GETS&DELS error\n");
				printf("GET length error, %d %d\n", 0, lens[i]);
				goto exit;
			}
		}
		printf("test SETS&GETS&DELS ok\n");
	}

	/*test PUBLISH&SUBSCRIBE&UNSUBSCRIBE*/
	{
		char *channels[2];
		char channel[16];
		int message, mlen;
		
		channels[0] = "channel0";
		channels[1] = "channel1";

		ret = ngiosdb_subscribe(conn_subscribe, "channel0");
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn_subscribe));
			goto exit;
		}
		ret = ngiosdb_subscribe(conn_subscribe, "channel1");
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn_subscribe));
			goto exit;
		}
		for(i=0;i<2;i++){
			message = i;
			ret = ngiosdb_publish(conn_publish[i], channels[i], &message, sizeof(message));
			if(ret != 0){
				printf("%s\n", ngiosdb_error(conn_publish[i]));
				goto exit;
			}
		}

		for(i=0;i<2;i++){
			mlen = sizeof(message);
			ret = ngiosdb_message(conn_subscribe, channel, 16, &message, &mlen);
			if(ret != 0){
				printf("%s\n", ngiosdb_error(conn_subscribe));
				goto exit;
			}else{
				if(0 != strcmp(channel, channels[i])){
					printf("test PUBLISH&SUBSCRIBE&UNSUBSCRIBE error\n");
					printf("message channel error, %s %s\n", channels[i], channel);
					goto exit;
				}else if(message != i){
					printf("test PUBLISH&SUBSCRIBE&UNSUBSCRIBE error\n");
					printf("message value error, %d %d\n", i, message);
					goto exit;
				}
			}
		}

		ret = ngiosdb_unsubscribe(conn_subscribe, "channel0");
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn_subscribe));
			goto exit;
		}
		for(i=0;i<2;i++){
			message = i;
			ret |= ngiosdb_publish(conn_publish[i], channels[i], &message, sizeof(message));
			if(ret != 0){
				printf("%s\n", ngiosdb_error(conn_publish[i]));
				goto exit;
			}
		}

		mlen = sizeof(message);
		ret = ngiosdb_message(conn_subscribe, channel, 16, &message, &mlen);
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn_subscribe));
			goto exit;
		}else{
			if(0 != strcmp(channel, channels[1])){
				printf("test PUBLISH&SUBSCRIBE&UNSUBSCRIBE error\n");
				printf("message channel error, %s %s\n", channels[1], channel);
				goto exit;
			}else if(message != 1){
				printf("test PUBLISH&SUBSCRIBE&UNSUBSCRIBE error\n");
				printf("message value error, %d %d\n", 1, message);
				goto exit;
			}
		}

		ret = ngiosdb_unsubscribe(conn_subscribe, "channel1");
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn_subscribe));
			goto exit;
		}

		printf("test PUBLISH&SUBSCRIBE&UNSUBSCRIBE ok\n");
	}

	/*test SET&GET performance*/
	{
		int count = 100000;
		unsigned char buf[4] = {0};
		int len;
		unsigned int start_tick, stop_tick;

		printf("test SET&GET performance...\n");
		start_tick = times(NULL);
		for(i=0;i<count;i++){
			ret = ngiosdb_set(conn, config_min_db, "SET&GET&DEL", buf, 4);
			if(ret != 0){
				printf("%s\n", ngiosdb_error(conn));
				goto exit;
			}
		}
		stop_tick = times(NULL);
		printf("SET %d times/ps\n", count*100/(stop_tick - start_tick));

		start_tick = times(NULL);
		for(i=0;i<count;i++){
			len = 4;
			ret = ngiosdb_get(conn, config_min_db, "SET&GET&DEL", buf, &len);
			if(ret != 0){
				printf("%s\n", ngiosdb_error(conn));
				goto exit;
			}
		}
		stop_tick = times(NULL);
		printf("GET %d times/ps\n", count*100/(stop_tick - start_tick));
	}
	
	/*test SETS&GETS performance*/
	{
		int count = 100000;
		char keys_buffer[1000][32];
		unsigned char vals_buffer[1000][4] = {0};
		char *keys[1000];
		unsigned char *vals[1000];
		int lens[1000];
		unsigned int start_tick, stop_tick;

		printf("test SETS&GETS performance...\n");

		for(i=0;i<1000;i++){
			keys[i] = keys_buffer[i];
			vals[i] = vals_buffer[i];
			snprintf(keys[i], 32, "SETS&GETS&DELS%d", i);
			lens[i] = 4;
		}
		
		start_tick = times(NULL);
		for(i=0;i<count/1000;i++){
			ret = ngiosdb_sets(conn, config_min_db, 1000, keys, (void**)vals, lens);
			if(ret != 0){
				printf("%s\n", ngiosdb_error(conn));
				goto exit;
			}
		}
		stop_tick = times(NULL);
		printf("SET %d times/ps\n", count*100/(stop_tick - start_tick));

		start_tick = times(NULL);
		for(i=0;i<count/1000;i++){
			ret = ngiosdb_gets(conn, config_min_db, 1000, keys, (void**)vals, lens);
			if(ret != 0){
				printf("%s\n", ngiosdb_error(conn));
				goto exit;
			}
		}
		stop_tick = times(NULL);
		printf("GET %d times/ps\n", count*100/(stop_tick - start_tick));
	}

	/*test PUBLISH&SUBSCRIBE performance*/
	{
		int count = 100000;
		char channel[16];
		int message, mlen;
		unsigned int start_tick, stop_tick;

		printf("test PUBLISH&SUBSCRIBE performance...\n");

		ret = ngiosdb_subscribe(conn_subscribe, "channel0");
		if(ret != 0){
			printf("%s\n", ngiosdb_error(conn_subscribe));
			goto exit;
		}

		start_tick = times(NULL);
		for(i=0;i<count;i++){
			message = 0;
			ret |= ngiosdb_publish(conn_publish[0], "channel0", &message, sizeof(message));
			if(ret != 0){
				printf("%s\n", ngiosdb_error(conn_publish[0]));
				goto exit;
			}

			mlen = sizeof(message);
			ret = ngiosdb_message(conn_subscribe, channel, 16, &message, &mlen);
			if(ret != 0){
				printf("%s\n", ngiosdb_error(conn_subscribe));
				goto exit;
			}
		}
		stop_tick = times(NULL);
		printf("PUBLISH&SUBSCRIBE %d times/ps\n", count*100/(stop_tick - start_tick));
	}

exit:
	if(conn)
		ngiosdb_disconnect(conn);
	if(conn_publish[0])
		ngiosdb_disconnect(conn_publish[0]);
	if(conn_publish[1])
		ngiosdb_disconnect(conn_publish[1]);
	return;
}


