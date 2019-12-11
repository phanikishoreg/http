#include <assert.h>
#include <sys/socket.h>
#include <sys/types.h> 
#include <sys/wait.h>  
#include <arpa/inet.h> 
#include <fcntl.h>
#include <unistd.h>    
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "http-parser/http_parser.h"

#define SERVER_PORT 8080
#define BACKLOG     10
#define RCV_MAX     1024
#define HEADERS_MAX 5
#define HEADER_KV_SIZE 32
#define BODY_SZ     1024*1024
#define URL_SZ      32

#define GET_RESPOSNE "HTTP/1.1 200 OK\r\n"

http_parser_settings settings;

struct http_header {
	char header[HEADER_KV_SIZE];
	char value[HEADER_KV_SIZE];
};

struct http_request {
	struct http_header headers[HEADERS_MAX];
	char url[URL_SZ];
	char body[BODY_SZ];
	int bodylen;
	int nheaders;
	int last_was_value;
	int header_end;
	int message_begin, message_end;
};

int
http_on_msg_begin(http_parser *parser)
{
	printf("%s:%d\n", __func__, __LINE__);
	struct http_request *r = parser->data;

	r->message_begin = 1;
	r->last_was_value = 1; //should always start with a header..
	return 0;
}

int
http_on_msg_end(http_parser *parser)
{
	printf("%s:%d\n", __func__, __LINE__);
	struct http_request *r = parser->data;

	r->message_end = 1;
	return 0;
}

int
http_on_header_end(http_parser *parser)
{
	printf("%s:%d\n", __func__, __LINE__);
	struct http_request *r = parser->data;

	r->header_end = 1;

	return 0;
}

int
http_on_url(http_parser* parser, const char *at, size_t length)
{
	printf("%s:%d\n", __func__, __LINE__);
	struct http_request *r = parser->data;

	assert(length <= URL_SZ);
	strncpy(r->url, at, length);

	return 0;
}

int
http_on_header_field(http_parser* parser, const char *at, size_t length)
{
	printf("%s:%d\n", __func__, __LINE__);
	struct http_request *r = parser->data;

	if (r->last_was_value) r->nheaders ++;
	assert(r->nheaders <= HEADERS_MAX);

	r->last_was_value = 0;
	strncpy(r->headers[r->nheaders - 1].header, at, length);

	return 0;
}

int
http_on_header_value(http_parser* parser, const char *at, size_t length)
{
	printf("%s:%d\n", __func__, __LINE__);
	struct http_request *r = parser->data;

	r->last_was_value = 1;
	strncpy(r->headers[r->nheaders - 1].value, at, length);

	return 0;
}

int
http_on_body(http_parser* parser, const char *at, size_t length)
{
	printf("%s:%d\n", __func__, __LINE__);
	struct http_request *r = parser->data;

	assert(length <= BODY_SZ);
	memcpy(r->body, at, length);
	printf("%s:%d %d\n", __func__, __LINE__, length);
	r->bodylen = length;

	return 0;
}

void *
http_server_fn(void *d)
{
	struct http_request *hreq = (struct http_request *)malloc(sizeof(struct http_request));
	memset(hreq, 0, sizeof(hreq));
	int sock = *((int *)d);
	char *buf = (char *)malloc(RCV_MAX);
	int rcvd = 0, r = 0;

	http_parser *parser = malloc(sizeof(http_parser));
	http_parser_init(parser, HTTP_REQUEST);
	parser->data = hreq;

	printf("Receiving from [%d]\n", sock);
	memset(buf, 0, RCV_MAX);
	while ((r = recv(sock, (buf + rcvd), RCV_MAX, 0)) > 0) {
		rcvd += r;
		printf("%s:%d\n", __func__, __LINE__);
		if (r < RCV_MAX) break;
		printf("%s:%d\n", __func__, __LINE__);
		buf = (char *)realloc(buf, rcvd + RCV_MAX);
		memset(buf + rcvd, 0, RCV_MAX);
	}

	int nparsed = http_parser_execute(parser, &settings, buf, rcvd);

	int i;
	for (i = 0; i < hreq->nheaders; i++) {
		printf("[%s]:[%s]\n", hreq->headers[i].header, hreq->headers[i].value);
	}
	printf("Body: %s, %d\n", hreq->body, strlen(hreq->body));
	int fd = open("tmp.png", O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXO | S_IRWXG);
	if (fd < 0) {
		perror("open");
		goto skip;
	}

	int sz = write(fd, hreq->body, hreq->bodylen);
	if (sz < 0) {
		perror("write");
	}
	close(fd);


skip:
	printf("Rcvd: [[%s]]\n", buf);
	if (send(sock, GET_RESPOSNE, strlen(GET_RESPOSNE), 0) < 0) {
		perror("send");
	}

	free(hreq);
	free(parser);
	free(buf);
	free((int *)d);
	close(sock);
}

int
main(int argc, char *argv[])
{
	int    sock;
	struct sockaddr_in servaddr;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(-1);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(SERVER_PORT);

	int tr=1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &tr, sizeof(int)) < 0) {
		perror("setsockopt");
		exit(-1);
	}

	if (bind(sock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		perror("bind");
		exit(-1);
	}

	if (listen(sock, BACKLOG) < 0) {
		perror("listen");
		exit(-1);
	}

	http_parser_settings_init(&settings);
	settings.on_url          = http_on_url;
	settings.on_header_field = http_on_header_field;
	settings.on_header_value = http_on_header_value;
	settings.on_body         = http_on_body;
	settings.on_headers_complete = http_on_header_end;
	settings.on_message_begin    = http_on_msg_begin;
	settings.on_message_complete = http_on_msg_end;

	while (1) {
		struct sockaddr_in clientaddr;
		int addr_len = sizeof(clientaddr);
		int conn = -1;

		printf("Accepting..\n");
		if ((conn = accept(sock, (struct sockaddr *)&clientaddr, &addr_len)) < 0) {
			perror("accept");
			continue;
		}
		int *c = (int *)malloc(sizeof(int));
		*c = conn;

		printf("Accepted %d\n", conn);
		pthread_t thd;
		int r = pthread_create(&thd, NULL, http_server_fn, (void *)c);
		assert(r == 0);
	}

	return -1;
}

