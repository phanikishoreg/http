.PHONY: all server client clean

all: clean server client

server: server.c http-parser/http_parser.c http-parser/http_parser.h
	@gcc  -Ihttp_parser/ http-parser/http_parser.c server.c -g -o server -lpthread

client: client.c http-parser/http_parser.c http-parser/http_parser.h
	@gcc  -Ihttp_parser/ http-parser/http_parser.c client.c -g -o client

clean:
	@rm -f server client
