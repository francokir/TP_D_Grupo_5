# TP D – Aplicaciones distribuidas con BSD sockets (Grupo 5)

## Compilación

```sh
make          # compila ./cliente y ./test_md5 con -Wall -Wextra (sin warnings)
make test     # verifica la implementación propia de MD5
make clean
```

## Uso

```sh
./cliente <usuario> <secreto> <ip_servidor> [puerto_udp]
# ejemplo:
./cliente grupo05 <SECRETO> 167.114.129.206
```

Las credenciales están en `credenciales.txt` (no se versiona).

## Desafío 1 (Checkpoint #1)

1. **IP pública (cliente TCP):** se abre un socket TCP a `api.ipify.org:80` y se
   envía un `GET / HTTP/1.1` escrito a mano (sin libcurl). Se lee en loop hasta
   que el servidor cierra (`Connection: close`), se salta el header (`\r\n\r\n`)
   y se valida la IP con `inet_pton`.
2. **Autenticación (cliente UDP):** se envía al puerto 20262
   `usuario|ip|md5(usuario|ip|secreto)`.
3. **Respuesta:** se espera hasta 10 s con `select()`. Si llega un `200` cuya
   firma `md5(200|Port|<puerto>|secreto)` no coincide, se descarta en silencio y
   se sigue esperando. Los `400`/`401` traen firma de 32 ceros y se informan.

MD5 implementado desde cero según RFC 1321 (`src/md5.c`).

## Captura de Wireshark

Filtro sugerido: `ip.addr == 167.114.129.206 || tcp.port == 80`
