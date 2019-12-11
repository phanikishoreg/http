.PHONY: all server clean

all: clean server

server: server.c http-parser/http_parser.c http-parser/http_parser.h
	@gcc  -Ihttp_parser/ http-parser/http_parser.c server.c -g -o server -lpthread

clean:
	@rm -f server
