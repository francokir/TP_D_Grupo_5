CC     = gcc
CFLAGS = -Wall -Wextra

all: cliente test_md5 test_autenticacion

cliente: src/cliente.c src/ip_publica.c src/ip_publica.h src/autenticacion.c src/autenticacion.h src/md5.c src/md5.h
	$(CC) $(CFLAGS) -o cliente src/cliente.c src/ip_publica.c src/autenticacion.c src/md5.c

test_md5: src/test_md5.c src/md5.c src/md5.h
	$(CC) $(CFLAGS) -o test_md5 src/test_md5.c src/md5.c

test_autenticacion: src/test_autenticacion.c src/autenticacion.c src/autenticacion.h src/md5.c src/md5.h
	$(CC) $(CFLAGS) -o test_autenticacion src/test_autenticacion.c src/autenticacion.c src/md5.c

run: cliente
	./cliente $(ARGS)

test: test_md5 test_autenticacion
	./test_md5
	./test_autenticacion

clean:
	rm -f cliente test_md5 test_autenticacion

.PHONY: all run test clean
