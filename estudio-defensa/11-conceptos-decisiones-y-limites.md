# Conceptos de la materia, decisiones y límites del programa

## Conceptos que aparecen en el código

| Concepto | Dónde aparece | En pocas palabras |
|---|---|---|
| Socket | todos los `socket()` | Un descriptor (un número) para comunicarse por la red, como un archivo abierto |
| TCP: flujo de bytes | `procesar_pdus`, `extraer_pdu` | Llega todo y en orden, pero sin límites de mensaje: el framing lo pone la aplicación |
| UDP: datagramas | `enviar_pedido`, `esperar_respuesta` | Cada `sendto` es un mensaje completo, sin confirmación, sin retransmisión, sin orden garantizado |
| 3-way handshake | `connect` | SYN → SYN-ACK → ACK antes de mandar datos |
| Cierre y EOF | loops de `recv` | El FIN del otro lado hace que `recv` devuelva 0 |
| Puerto efímero | primer `sendto` sin `bind` | El sistema elige el puerto origen (56872 en la captura) |
| NAT / NAPT | IP pública, socket UDP reutilizado | El router traduce IP y puerto privados a públicos; la entrada se crea con el tráfico de salida |
| DNS | `getaddrinfo("api.ipify.org", …)` | Traduce un nombre a una IP |
| HTTP/1.1 | pedido a ipify | Línea de pedido, `Host:` obligatorio, línea vacía al final, `Connection: close` |
| Hash / firma con secreto compartido | `md5_hex`, `armar_pedido`, `verificar_respuesta`, `armar_reporte` | Se manda la huella y no el secreto; la firma prueba quién escribió el mensaje y que no se modificó |
| Multiplexación de E/S | `select` | Esperar hasta que un descriptor tenga datos o venza un tiempo |
| Delimitador | `\r\n`, `\|`, `&` | Bytes reservados para marcar dónde termina algo; no pueden aparecer como datos (por eso la PDU con `\|` no se reenvía) |

## Las funciones de sockets que usamos

| Función | Para qué | Qué devuelve |
|---|---|---|
| `getaddrinfo` | Resolver nombre/IP + puerto a una dirección lista para usar | 0 si OK; si no, un código para `gai_strerror` |
| `socket` | Crear el descriptor (`SOCK_STREAM` = TCP, `SOCK_DGRAM` = UDP) | el descriptor, o -1 |
| `connect` | TCP: el 3-way handshake | 0 o -1 |
| `send` / `recv` | TCP: mandar / recibir bytes | bytes, 0 (EOF, solo `recv`) o -1 |
| `sendto` / `recvfrom` | UDP: mandar a / recibir de una dirección | bytes o -1 |
| `select` | Esperar con un tiempo máximo | cantidad de descriptores listos, 0 (timeout) o -1 |
| `close` | Cerrar el socket (en TCP manda el FIN) | 0 o -1 |
| `freeaddrinfo` | Liberar la lista de `getaddrinfo` | — |
| `inet_pton` / `inet_ntop` | IP texto → binario / binario → texto | — |

Los errores se muestran con `fprintf(stderr, "Error en X %i: %s\n", errno, strerror(errno))`,
salvo `getaddrinfo`, que usa `gai_strerror(status)` porque no pone el error en `errno`.

## Decisiones que tomamos donde la consigna era ambigua

- **El reporte lleva la PDU sin `\r\n`.** El `\r\n` es el delimitador, no parte del mensaje.
  Confirmado: en la web de la cátedra los 9 reportes aparecen en verde.
- **Si una PDU tiene varios errores, se reporta el primero que encontramos**, en este orden:
  estructura, timestamp, mensaje. Cada ejemplo del enunciado tiene un solo error, así que la
  consigna no fija el orden.
- **Timestamp vacío (`&Hola`) = 400**, porque no es ninguno de los errores tipificados.
- **Timestamp del futuro = 200:** la consigna solo rechaza los que tienen más de 300 s de
  antigüedad.
- **Los reportes salen por el mismo socket UDP de la autenticación**, para usar el puerto que
  el NAT ya asoció con el servidor.
- **10 segundos en total** para esperar la respuesta, aunque se descarten datagramas en el
  medio.
- **Filtrar por IP de origen** los datagramas UDP: lo que no viene del servidor se ignora.

## Lo que el programa no hace (y cómo se podría mejorar)

Es mejor saberlo antes de que lo pregunten:

| Limitación | Qué pasa | Mejora posible |
|---|---|---|
| No reintenta el pedido UDP | Si se pierde el pedido o la respuesta, espera 10 s y termina con error | Reenviar con backoff (1, 2, 4 s) dentro de los 10 s |
| No sabe si los reportes llegaron | El protocolo no tiene confirmación | Número de secuencia + ACK del servidor, o mandar los reportes por TCP |
| `recv` del desafío 2 sin timeout | Si el servidor se colgara sin cerrar, el programa esperaría para siempre | `select` con timeout también en ese loop |
| No corta una PDU a los 122 bytes | El acumulador acepta hasta 4096 sin `\r\n` y recién ahí da error | Cortar a los 122 bytes y reportar 400 |
| Solo IPv4 | `AF_INET` en todos los `getaddrinfo` | El servidor espera una IPv4 en el pedido, así que es lo correcto acá |
| `consulta_ip` no chequea el retorno de `send`, y si falla `connect` no libera `servinfo` | Detalles menores; no afectan una corrida normal | Chequear `send` y llamar a `freeaddrinfo` también en ese camino |
| MD5 | Hoy se considera roto para firmas | HMAC-SHA256; lo usamos porque lo pide la consigna |
