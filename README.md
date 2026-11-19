# TP D – Desarrollo de aplicaciones distribuidas con BSD sockets

**Grupo 5** – Redes y Comunicación (I310), UdeSA

Cliente en C que resuelve los dos desafíos del TP D:

- **Desafío 1 (autenticación):** obtiene la IP pública por HTTP, se autentica por UDP ante
  el servidor de la cátedra con una firma MD5 y recibe un puerto TCP.
- **Desafío 2 (framing de PDUs):** se conecta por TCP a ese puerto, separa las PDUs que manda
  el servidor (aunque lleguen partidas o varias juntas), valida cada una y la reenvía
  firmada por UDP con su código de parseo.

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
Autenticado. Puerto TCP asignado: 30028
Conectado por TCP a 167.114.129.206:30028
[200] grupo05|1794669473&Primer mensaje|200|<firma>
[200] grupo05|1794669473&Second Message|200|<firma>
[200] grupo05|1794669473&3er Hello|200|<firma>
[200] grupo05|1794669473&Queres un cuarto mensaje|200|<firma>
[405] 17x0000000&Mensaje | invalido  -> no se envia (tiene '|')
[408] grupo05|1794669474&Mensaje con ~ invalido|408|<firma>
[406] grupo05|1794583074&Timestamp de ayer|406|<firma>
[407] grupo05|123456789012&Timestamp demasiado largo|407|<firma>
[409] grupo05|1794669474&|409|<firma>
[410] grupo05|1794669474&AAAA...AAAA|410|<firma>
Conexion TCP cerrada por el servidor. Reportes enviados: 9
```

Cada línea `[código]` muestra el reporte que se mandó por UDP.

### Códigos de salida

| Código | Significado |
|---|---|
| 0 | Los dos desafíos terminaron bien |
| 1 | Error: argumentos incorrectos, error de red, `400`/`401` del servidor, ninguna respuesta válida en 10 s o error en la conexión TCP |

## Tests

```sh
make test
```

No usan la red. Prueban:

- `test_md5`: la implementación de MD5 contra los vectores de prueba del RFC 1321 y los
  ejemplos del enunciado.
- `test_autenticacion`: la separación de campos y la verificación de respuestas
  (firma válida, firma inválida, mensaje alterado, puerto inválido, `400` y `401`).
- `test_pdu`:
  - **framing**: una PDU entera, varias PDUs en un `recv`, una PDU partida en varios
    `recv`, una cantidad no entera de PDUs, el `\r\n` partido entre dos `recv`, y todo
    llegando byte por byte;
  - **validación**: los 7 ejemplos del enunciado, las 10 PDUs que manda el servidor y los
    casos borde (100/101 caracteres, 300/301 segundos, `}`/`~`);
  - **reportes**: firma correcta (comparada con `md5sum`) y descarte de PDUs con `|`.

## Funcionamiento

```
cliente                     api.ipify.org          servidor (UDP 20262 / TCP dinámico)
   │── TCP: GET / HTTP/1.1 ──────►│                                │
   │◄────── 200 OK + IP pública ──│                                │
   │                                                               │
   │── UDP: <usuario>|<ip>|<firma> ───────────────────────────────►│ 20262
   │◄──────────────────────────── UDP: 200|Port|<puerto>|<firma> ──│
   │                                                               │
   │── TCP: SYN / SYN-ACK / ACK ──────────────────────────────────►│ <puerto>
   │◄────────────────────────────────────── PDUs (en segmentos) ───│
   │── UDP: <usuario>|<PDU>|<código>|<firma> ─────────────────────►│ 20262  (una por PDU)
   │◄─────────────────────────────────────────────────────── FIN ──│
```

### Desafío 1: autenticación

1. **IP pública (TCP):** se conecta a `api.ipify.org:80`, manda un pedido HTTP escrito a
   mano y lee la respuesta en un loop hasta EOF (`Connection: close`). Después busca el
   fin de los headers (`\r\n\r\n`) y valida la IP con `inet_pton`.
2. **Pedido firmado:** arma `<usuario>|<ip>|<firma>`, donde la firma es el MD5 de
   `<usuario>|<ip>|<secreto>`. El secreto nunca viaja por la red.
3. **Envío (UDP):** manda el pedido con `sendto` al puerto 20262 del servidor.
4. **Respuesta:** espera como máximo **10 segundos en total** usando `select`.
   - Si llega `200|Port|<puerto>|<firma>` y la firma coincide con
     `md5(200|Port|<puerto>|<secreto>)`, sigue con el desafío 2.
   - Si la firma no coincide, el formato es inválido o el datagrama no viene de la IP
     del servidor, lo **descarta en silencio** y sigue esperando el tiempo que queda.
   - Si llega `400` o `401`, muestra el error y termina.

### Desafío 2: framing y validación de PDUs

1. **Conexión (TCP):** se conecta enseguida al puerto recibido (el servidor lo mantiene
   abierto solo 10 segundos).
2. **Framing:** cada `recv` se agrega a un **acumulador**. Después de cada `recv` se sacan
   **todas** las PDUs completas (terminadas en `\r\n`) que haya, y lo que sobra queda en el
   acumulador para el `recv` siguiente. Así funciona igual si una PDU llega partida, si
   llegan varias juntas o cualquier combinación. El delimitador se busca byte por byte
   dentro de los bytes válidos (no con `strstr`), para no depender de un `'\0'`.
3. **Validación:** cada PDU `<TIMESTAMP>&<MESSAGE>` recibe un código. Si tiene varios
   errores se informa el primero que se encuentra, en este orden:

   | Orden | Chequeo | Código |
   |---|---|---|
   | 1 | No hay `&` (no hay 2 campos), o el timestamp está vacío | 400 |
   | 2 | El timestamp tiene caracteres fuera de `0x30`–`0x39` | 405 |
   | 3 | El timestamp tiene más de 11 caracteres | 407 |
   | 4 | El timestamp tiene más de 300 s de antigüedad respecto de la hora actual | 406 |
   | 5 | El mensaje está vacío | 409 |
   | 6 | El mensaje tiene caracteres fuera de `0x20`–`0x7D`, o `&`, o `\|` | 408 |
   | 7 | El mensaje tiene más de 100 caracteres | 410 |
   | — | Ninguno de los anteriores | 200 |

4. **Reporte (UDP):** por cada PDU se manda `<usuario>|<PDU>|<código>|<firma>` al puerto
   20262 por el mismo socket UDP de la autenticación, con
   `firma = md5(<usuario>|<PDU>|<código>|<secreto>)`. La PDU va sin el `\r\n`.
   Las PDUs que contienen `|` **no se reenvían**, porque romperían el formato del reporte.
5. Termina cuando el servidor cierra la conexión TCP (`recv` devuelve 0).

## Estructura

```
src/
├── cliente.c              main: el orden de los pasos
├── ip_publica.h/.c        consulta_ip, obtener_ip: IP pública por HTTP sobre TCP
├── md5.h/.c               md5_hex: MD5 implementado según el RFC 1321
├── autenticacion.h/.c     armar_pedido, enviar_pedido, esperar_respuesta, separar_campos,
│                          verificar_respuesta, recibir_puerto, enviar_al_servidor
├── conexion_tcp.h/.c      conectar_tcp, procesar_pdus (loop del desafío 2)
├── pdu.h/.c               acumulador y extraer_pdu (framing), validar_pdu, armar_reporte
├── test_md5.c             tests de MD5
├── test_autenticacion.c   tests de separación y verificación de respuestas
└── test_pdu.c             tests de framing, validación y reportes
```

## Capturas de tráfico

| Archivo | Contenido |
|---|---|
| `TP_D_captura.pcapng` | Desafío 1 |
| `TP_D_captura2.pcapng` | Desafíos 1 y 2 completos |

Filtro de Wireshark usado:

```
ip.addr == 167.114.129.206 || tcp.port == 80
```

## Respuestas a las preguntas

Las respuestas a las cuatro preguntas del enunciado están en `TP_D_Respuestas.pdf`.
