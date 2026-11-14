CC     = gcc
CFLAGS = -Wall -Wextra

all: cliente test_md5 test_autenticacion test_pdu

cliente: src/cliente.c src/ip_publica.c src/ip_publica.h src/autenticacion.c src/autenticacion.h src/md5.c src/md5.h src/conexion_tcp.c src/conexion_tcp.h src/pdu.c src/pdu.h
	$(CC) $(CFLAGS) -o cliente src/cliente.c src/ip_publica.c src/autenticacion.c src/md5.c src/conexion_tcp.c src/pdu.c

test_md5: src/test_md5.c src/md5.c src/md5.h
	$(CC) $(CFLAGS) -o test_md5 src/test_md5.c src/md5.c

test_autenticacion: src/test_autenticacion.c src/autenticacion.c src/autenticacion.h src/md5.c src/md5.h
	$(CC) $(CFLAGS) -o test_autenticacion src/test_autenticacion.c src/autenticacion.c src/md5.c

test_pdu: src/test_pdu.c src/pdu.c src/pdu.h src/md5.c src/md5.h
	$(CC) $(CFLAGS) -o test_pdu src/test_pdu.c src/pdu.c src/md5.c

run: cliente
	./cliente $(ARGS)

test: test_md5 test_autenticacion test_pdu
	./test_md5
	./test_autenticacion
	./test_pdu

clean:
	rm -f cliente test_md5 test_autenticacion test_pdu

.PHONY: all run test clean
