.PHONY: all server client clean sub

all: sub clean server client

server: server.c http-parser/http_parser.c http-parser/http_parser.h
	@gcc  -Ihttp_parser/ http-parser/http_parser.c server.c -g -o server -lpthread

client: client.c http-parser/http_parser.c http-parser/http_parser.h
	@gcc  -Ihttp_parser/ http-parser/http_parser.c client.c -g -o client

sub:
	@git submodule update --init --recursive

clean:
	@rm -f server client
