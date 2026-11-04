CC     = gcc
CFLAGS = -Wall -Wextra

all: cliente test_md5

cliente: src/cliente.c src/ip_publica.c src/ip_publica.h
	$(CC) $(CFLAGS) -o cliente src/cliente.c src/ip_publica.c

test_md5: src/test_md5.c src/md5.c src/md5.h
	$(CC) $(CFLAGS) -o test_md5 src/test_md5.c src/md5.c

run: cliente
	./cliente

test: test_md5
	./test_md5

clean:
	rm -f cliente test_md5

.PHONY: all run test clean
