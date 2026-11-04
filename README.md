# TP D – Desarrollo de aplicaciones distribuidas con BSD sockets

**Grupo 5** – Redes y Comunicación (I310), UdeSA

Cliente en C que resuelve el **desafío 1** (autenticación) del TP D: obtiene la IP pública
por HTTP, se autentica por UDP ante el servidor de la cátedra con una firma MD5 y recibe
el puerto TCP asignado.

## Requisitos

- Linux (o WSL) con `gcc` y `make`.
- Ninguna biblioteca externa: HTTP y MD5 están implementados a mano, como pide la consigna.

## Compilación

```sh
make          # compila el cliente y los tests
make clean    # borra los ejecutables
```

Se compila con `-Wall -Wextra` y **no produce warnings**.

## Uso

```sh
./cliente <usuario> <secreto> <ip_servidor>
```

o, con `make`:

```sh
make run ARGS="<usuario> <secreto> <ip_servidor>"
```

Ejemplo:

```sh
./cliente grupo05 <secreto> 167.114.129.206
```

El usuario y el secreto **no están en el código**: se pasan por línea de comandos para no
publicarlos en el repositorio.

### Salida esperada

```
IP pública: 190.225.36.143
Pedido: grupo05|190.225.36.143|<firma>
Pedido enviado a 167.114.129.206
Autenticado. Puerto TCP asignado: 30031
```

### Códigos de salida

| Código | Significado |
|---|---|
| 0 | Autenticación exitosa (200 con firma válida) |
| 1 | Error: argumentos incorrectos, error de red, `400`/`401` del servidor o ninguna respuesta válida en 10 s |

## Tests

```sh
make test
```

No usan la red. Prueban:

- `test_md5`: la implementación de MD5 contra los vectores de prueba del RFC 1321 y los
  ejemplos del enunciado.
- `test_autenticacion`: la separación de campos y la verificación de respuestas
  (firma válida, firma inválida, mensaje alterado, puerto inválido, `400` y `401`).

## Funcionamiento

```
cliente                         api.ipify.org              servidor (UDP 20262)
   │── TCP: GET / HTTP/1.1 ────────►│                              │
   │◄──────── 200 OK + IP pública ──│                              │
   │                                                               │
   │── UDP: <usuario>|<ip>|md5(<usuario>|<ip>|<secreto>) ─────────►│
   │◄──────── UDP: 200|Port|<puerto>|md5(200|Port|<puerto>|<secreto>)
```

1. **IP pública (TCP):** se conecta a `api.ipify.org:80`, manda un pedido HTTP escrito a
   mano y lee la respuesta en un loop hasta EOF (`Connection: close`). Después busca el
   fin de los headers (`\r\n\r\n`) y valida la IP con `inet_pton`.
2. **Pedido firmado:** arma `<usuario>|<ip>|<firma>`, donde la firma es el MD5 de
   `<usuario>|<ip>|<secreto>`. El secreto nunca viaja por la red.
3. **Envío (UDP):** manda el pedido con `sendto` al puerto 20262 del servidor.
4. **Respuesta:** espera como máximo **10 segundos en total** usando `select`.
   - Si llega `200|Port|<puerto>|<firma>` y la firma coincide con
     `md5(200|Port|<puerto>|<secreto>)`, muestra el puerto.
   - Si la firma no coincide, el formato es inválido o el datagrama no viene de la IP
     del servidor, lo **descarta en silencio** y sigue esperando el tiempo que queda.
   - Si llega `400` o `401`, muestra el error y termina.

## Estructura

```
src/
├── cliente.c              main: el orden de los pasos
├── ip_publica.h/.c        consulta_ip, obtener_ip: IP pública por HTTP sobre TCP
├── md5.h/.c               md5_hex: MD5 implementado según el RFC 1321
├── autenticacion.h/.c     armar_pedido, enviar_pedido, esperar_respuesta,
│                          separar_campos, verificar_respuesta, recibir_puerto
├── test_md5.c             tests de MD5
└── test_autenticacion.c   tests de separación y verificación de respuestas
```

## Captura de tráfico

Filtro de Wireshark usado:

```
ip.addr == 167.114.129.206 || tcp.port == 80
```
