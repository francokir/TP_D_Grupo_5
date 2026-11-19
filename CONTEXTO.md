# Contexto de trabajo – TP D (Grupo 5)

Este archivo resume cómo venimos trabajando, qué hay hecho y qué falta.
Sirve para retomar el trabajo sin perder el hilo.

## Cómo trabajamos

- **De a pasos chicos.** Cada paso tiene un objetivo concreto y una forma de comprobar
  que anda (correr el programa o `make test`). No se avanza hasta entender el paso anterior.
- **El código lo escribimos nosotros.** Cada parte se explica línea por línea antes de seguir.
  Hay que poder defenderlo en el oral.
- **Mismo estilo que la clase** (`bsd_sockets_examples.tgz`, `workshop_tcp_udp.tgz`,
  diapositivas `2025s1 - Sockets`):
  - Nombres de variables: `s`, `hints`, `servinfo`, `status`, `inbytes`, `buffer`.
  - `memset(&hints, 0, sizeof(hints))` + `getaddrinfo` para armar los sockets.
  - `switch (connect(...))` con `case -1` / `case 0`.
  - Errores: `fprintf(stderr, "Error ... %i: %s\n", errno, strerror(errno))`.
    `gai_strerror` **solo** para el código que devuelve `getaddrinfo`.
  - Loop de lectura `while (1)` con `recv` hasta EOF (`inbytes == 0`).
  - UDP: `sendto` / `recvfrom` como en `udp_response_client.c` (inicializar `addrsize`).
  - Timeout: `select` con `FD_ZERO` / `FD_SET` / `struct timeval` como en `ex08-00-select-chat.c`.
- **Detalles de los ejemplos de clase que NO copiamos:** `recv` con `sizeof(buffer)` sin dejar
  lugar para el `'\0'`, `gai_strerror(errno)`, no chequear el retorno de `socket()`,
  `addrsize` sin inicializar, `inbytes` sin inicializar.
- **Sin warnings** con `-Wall -Wextra` (la consigna exige `-Wall`).
- **No conectarse al servidor de la cátedra salvo que haga falta.** Para probar se usan tests
  locales (`make test`) o respuestas armadas a mano.
- **Cada `.c` incluye su propio `.h`**, así el compilador detecta si las firmas no coinciden.
- **Patrón de buffers:** quien llama crea el array y la función lo llena
  (nunca devolver un puntero a una variable local).

## Archivos

| Archivo | Qué tiene |
|---|---|
| `src/cliente.c` | `main`: solo el orden de los pasos |
| `src/ip_publica.h` / `.c` | `consulta_ip`, `obtener_ip`, `INET4_ADDRSTRLEN` |
| `src/md5.h` / `.c` | `md5_hex`, `MD5_HEX_LEN` |
| `src/autenticacion.h` / `.c` | Pedido firmado, envío UDP, espera y verificación de la respuesta |
| `src/test_md5.c` | Prueba `md5_hex` con vectores del RFC 1321 y del enunciado |
| `src/test_autenticacion.c` | Prueba `separar_campos` y `verificar_respuesta` sin red |
| `Makefile` | `make`, `make run`, `make test`, `make clean` |
| `credenciales.txt` | Usuario, secreto e IP del servidor. **No se commitea** (está en `.gitignore`) |

## Funciones

```c
/* ip_publica.h */
int consulta_ip(char *buffer, int buffer_len);
    /* TCP a api.ipify.org:80, manda GET HTTP escrito a mano y ACUMULA toda la
       respuesta en buffer (buffer + total) hasta EOF. Deja el buffer terminado
       en '\0'. Devuelve 0 si OK, 1 si error. */

int obtener_ip(char *respuesta, char *ip);
    /* Chequea "HTTP/1.1 200", busca "\r\n\r\n" con strstr, recorta la IP con
       strspn, la copia con memcpy + '\0' y la valida con inet_pton.
       Devuelve 0 si OK, 1 si error. No toca la red. */

/* md5.h */
void md5_hex(char *mensaje, int len, char *salida);
    /* md5 de 'len' bytes de 'mensaje' -> 'salida' (32 hex minúscula + '\0').
       Una sola función pública: los mensajes son cortos, no hace falta
       init/update/final. */
```

## Estado

### Desafío 1 (Checkpoint #1)

| Paso | Qué | Estado |
|---|---|---|
| 1–2 | IP pública por TCP/HTTP (`consulta_ip`, `obtener_ip`) | ✅ |
| 3a–3e | MD5 propio (`md5_hex`) + `test_md5.c` (6/6) | ✅ |
| 4 | Argumentos por `argv` + pedido firmado (`armar_pedido`) | ✅ |
| 5 | Envío UDP al puerto 20262 (`enviar_pedido`, devuelve el socket) | ✅ |
| 6 | Esperar con `select` + `recvfrom` (`esperar_respuesta`, timeout por parámetro) | ✅ |
| 7a | `separar_campos` (strchr, no strtok) + `test_autenticacion.c` | ✅ |
| 7b | `verificar_respuesta` (OK / ERROR / INVALIDA) | ✅ (15/15 tests) |
| 7c | `recibir_puerto`: loop con tiempo restante (gettimeofday), chequeo de origen, descarte silencioso | ✅ probado con el servidor real (200) |
| 8 | `README.md` | ✅ |
| 9 | Captura de Wireshark con el código final (`TP_D_captura.pcapng`) | ✅ |
| 10 | Commit y push | ✅ |

Filtro de Wireshark: `ip.addr == <IP servidor> || tcp.port == 80`

### Desafío 2 (entrega final 20/11)

Conectarse por TCP al puerto recibido, delimitar las PDUs por `\r\n` (pueden llegar cortadas
o varias juntas), validarlas (códigos 200, 400, 405–410) y reenviar cada una firmada por UDP
(`usuario|PDU|código|md5(usuario|PDU|código|secreto)`, puerto 20262). No se reenvían las PDUs con `|`.

| Paso | Qué | Estado |
|---|---|---|
| 1 | Conectarse por TCP al puerto (`conectar_tcp`) + volcar lo que llega (debug) | ✅ probado con el servidor real |
| 2 | Framing: `acumulador` + `extraer_pdu` (sin el `\r\n`), `test_pdu.c` | ✅ |
| 3 | `validar_pdu(pdu, len, ahora)` → código; timestamp vacío = 400, futuro = 200 | ✅ |
| 4 | `armar_reporte` (sin `\r\n`, confirmado en la web), descarta PDUs con `\|` | ✅ (38/38 tests) |
| 5 | `procesar_pdus` (conexion_tcp.c) + `enviar_al_servidor` (autenticacion.c) | ✅ |
| 6 | Probado con el servidor real: 9 reportes enviados, códigos correctos | ✅ (19/11: en la web de la cátedra aparece todo en verde) |
| 7 | `TP_D_captura2.pcapng` (desafíos 1 y 2) + README actualizado | ✅ |
| 8 | PDF con las 4 preguntas (`TP_D_Respuestas.pdf`, LaTeX, a partir de `RESPUESTAS.md`) | ✅ |

**Lo que manda el servidor** (siempre las mismas 11 PDUs, solo cambia el timestamp):

| PDU (sin `\r\n`) | Código esperado |
|---|---|
| `<ts>&Primer mensaje` | 200 |
| `<ts>&Second Message` | 200 (llega en el mismo recv que la siguiente) |
| `<ts>&3er Hello` | 200 |
| `<ts>&Queres un cuarto mensaje` | 200 (llega partida en 3 recv) |
| `17x0000000&Mensaje \| invalido` | No se envía (tiene `\|`) |
| `<ts>&Mensaje con ~ invalido` | 408 (`~` = 0x7E, fuera de rango) |
| `<ts - 86400>&Timestamp de ayer` | 406 |
| `123456789012&Timestamp demasiado largo` | 407 |
| `<ts>&` | 409 |
| `<ts>&` + 101 `A` | 410 |

No aparecen: 400, 405 (solo en la PDU con `|`), timestamp vacío ni futuro, PDUs de más de 122 bytes.

**Decisiones tomadas:**
- Si la consigna es ambigua (si `<PDU>` incluye el `\r\n`, más de 122 bytes sin delimitador,
  timestamp vacío o futuro), nos guiamos por lo que manda el servidor real.
- Si una PDU tiene varios errores: se reporta el **primero que se encuentre**.
- Los reportes UDP se mandan por el **mismo socket UDP** de la autenticación.

## Notas

- El enunciado dice Checkpoint #1 el **3/11**. Confirmar la fecha con la cátedra.
- Preguntas a responder en el PDF: ver la última página del enunciado.
