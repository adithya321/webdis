#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <evhttp.h>
#include <event.h>
#include <string.h>
#include <signal.h>

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <jansson.h>

#include "conf.h"
#include "json.h"
#include "cmd.h"

void
cmdCallback(redisAsyncContext *c, void *r, void *privdata) {
	json_reply(c,r,privdata);
}

#if 0
	switch(reply->type) {
		case REDIS_REPLY_STRING:
		case REDIS_REPLY_STATUS:

			/* send reply */
			body = evbuffer_new();
			evbuffer_add(body, reply->str, strlen(reply->str));
			evhttp_send_reply(cmd->rq, 200, "OK", body);
			evbuffer_free(body);
			break;

		case REDIS_REPLY_NIL:
			evhttp_send_reply(cmd->rq, 404, "Not Found", NULL);
			break;

		default:
			evhttp_send_reply(cmd->rq, 500, "Unknown redis format", NULL);
	}
#endif

static void
connectCallback(const redisAsyncContext *c) {
	((void)c);
	printf("connected...\n");
}

static void
disconnectCallback(const redisAsyncContext *c, int status) {
	if (status != REDIS_OK) {
		printf("Error: %s\n", c->errstr);
	}
	printf("disconnected...\n");
}


void
on_request(struct evhttp_request *rq, void *ctx) {

	const char *uri = evhttp_request_uri(rq);

	/* get context */
	redisAsyncContext *c = ctx;

	switch(rq->type) {
		case EVHTTP_REQ_GET:
			cmd_run(c, rq, 1+uri, strlen(uri)-1);
			break;
		case EVHTTP_REQ_POST:
			cmd_run(c, rq,
				(const char*)EVBUFFER_DATA(rq->input_buffer),
				EVBUFFER_LENGTH(rq->input_buffer));
			break;

		default:
			evhttp_send_reply(rq, 500, "Unknown redis format", NULL);
			return;
	}
}

int
main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	struct event_base *base = event_base_new();
	struct evhttp *http = evhttp_new(base);
	
	struct conf *cfg = conf_read("turnip.conf");

	redisAsyncContext *c = redisAsyncConnect(cfg->redis_host, cfg->redis_port);
	if(c->err) {
		/* Let *c leak for now... */
		printf("Error: %s\n", c->errstr);
		return 1;
	}

	/* start http server */
	evhttp_bind_socket(http, cfg->http_host, cfg->http_port);
	evhttp_set_gencb(http, on_request, c);

	/* attach hiredis to libevent base */
	redisLibeventAttach(c, base);
	redisAsyncSetConnectCallback(c, connectCallback);
	redisAsyncSetDisconnectCallback(c, disconnectCallback);

	/* loop */
	event_base_dispatch(base);

	return EXIT_SUCCESS;
}
