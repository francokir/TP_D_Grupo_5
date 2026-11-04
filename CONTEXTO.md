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
- **Sin warnings** con `-Wall -Wextra` (la consigna exige `-Wall`). Por ahora quedan solo
  avisos de "unused parameter" que desaparecen en pasos siguientes.
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
| `src/md5.h` / `.c` | `md5_hex`, `MD5_HEX_LEN` (por ahora devuelve 32 ceros) |
| `src/test_md5.c` | Prueba `md5_hex` con vectores del RFC 1321 y del enunciado |
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
| 1 | Pedir la IP pública por TCP/HTTP (`consulta_ip`) | ✅ |
| 2 | Acumular la respuesta y sacar la IP (`obtener_ip`) | ✅ |
| 3a | Esqueleto de MD5 + `test_md5.c` + `make test` | ✅ (0/5 pruebas, es lo esperado) |
| 3b | MD5: padding (`0x80`, ceros hasta 56 mod 64, largo en bits en 8 bytes LE) | ⏳ **siguiente** |
| 3c | MD5: bloque de 64 bytes → 16 palabras de 32 bits little-endian | ⏳ |
| 3d | MD5: 64 pasos de mezcla (4 rondas, tablas K y S del RFC) | ⏳ |
| 3e | MD5: salida a hex → `make test` 5/5 | ⏳ |
| 4 | Leer usuario / secreto / IP servidor desde `argv` y armar `usuario\|ip\|md5(usuario\|ip\|secreto)` | ⏳ |
| 5 | Mandarlo por UDP al puerto 20262 (`sendto`) | ⏳ |
| 6 | Esperar la respuesta como máximo 10 s (`select` + `recvfrom`) | ⏳ |
| 7 | Separar `CODE\|ACTION\|TEXT\|SIGNATURE`; si es 200, verificar la firma (si no coincide, descartar en silencio); si es 400/401, informar | ⏳ |
| 8 | Rehacer `README.md` (el actual es de una versión vieja) | ⏳ |
| 9 | Rehacer la captura de Wireshark con el código nuevo | ⏳ |

Filtro de Wireshark: `ip.addr == <IP servidor> || tcp.port == 80`

### Desafío 2 (entrega final 20/11)

Conectarse por TCP al puerto recibido, delimitar las PDUs por `\r\n` (pueden llegar cortadas
o varias juntas), validarlas (códigos 200, 400, 405–410) y reenviar cada una firmada por UDP.
Todavía no empezado.

## Notas

- El enunciado dice Checkpoint #1 el **3/11**. Confirmar la fecha con la cátedra.
- Preguntas a responder en el PDF: ver la última página del enunciado.
