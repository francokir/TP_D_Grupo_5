# Guion de la defensa: 15 minutos por grupo

Los 15 minutos **incluyen las preguntas** que nos van a hacer. Por eso apuntamos a presentar en
**~9 minutos** (3 partes de ~3 minutos) y dejar **~6 minutos para preguntas**. Si nos cortan
antes para preguntar, mejor: cada uno sabe su parte de memoria y puede saltar a lo importante.

| Parte | Quién    | Tema                                      | Tiempo | Qué tener abierto                        |
| ----- | -------- | ----------------------------------------- | ------ | ---------------------------------------- |
| 1     | Luis     | Visión general, IP pública y MD5          | ~3 min | `cliente.c`, `ip_publica.c`, `md5.c`     |
| 2     | Franco   | Autenticación por UDP                     | ~3 min | `autenticacion.c` + frames 10 y 12       |
| 3     | Alan     | Desafío 2: framing, validación y reportes | ~3 min | `conexion_tcp.c`, `pdu.c` + frames 13–49 |
| —     | los tres | Preguntas                                 | ~6 min | todo                                     |

**Antes de empezar:** el repo abierto en el editor, `TP_D_captura2.pcapng` abierta en
Wireshark con el filtro `ip.addr == 167.114.129.206 || tcp.port == 80`, y una terminal con
`make` y `make test` ya corridos para mostrar que no hay warnings y que pasan todos los tests.

---

## Parte 1 (~3 min): visión general, IP pública y MD5

**Mostrar:** la salida de `make` y `make test`, después `cliente.c`.

> "Nuestro programa es un cliente en C que resuelve los dos desafíos en una sola corrida. Se
> llama con `./cliente <usuario> <secreto> <ip_servidor>`: el usuario y el secreto van por
> línea de comandos para no dejarlos en el repositorio. Compila con `-Wall -Wextra` sin
> warnings, y tiene tests sin red que pasan todos."

> "El `main` solo pone los pasos en orden: IP pública, pedido firmado, envío por UDP, espera
> de la respuesta, conexión TCP y procesamiento de PDUs. Cada paso es una función en su propio
> archivo y, si alguno falla, el programa termina con código 1."

**Mostrar:** `ip_publica.c`.

> "Primero necesitamos nuestra IP pública, porque el servidor la valida. Nuestra PC tiene una
> IP privada y sale por NAT, así que se la preguntamos a `api.ipify.org`. Como no se puede usar
> libcurl, escribimos el pedido HTTP a mano: `getaddrinfo` resuelve el nombre, `connect` hace
> el 3-way handshake y mandamos `GET / HTTP/1.1` con `Host:` y `Connection: close`."

> "Con `Connection: close` el servidor cierra al terminar, así que leemos con `recv` en un loop,
> acumulando, hasta que devuelve 0. Después `obtener_ip` busca la línea vacía que separa
> headers y cuerpo, recorta la IP y la valida con `inet_pton`."

**Mostrar:** `md5.c`.

> "Las firmas son un MD5 que implementamos siguiendo el RFC 1321: el padding agrega `0x80`,
> ceros y el largo en bits; cada bloque de 64 bytes pasa por 4 rondas de 16 pasos, y el
> resultado son 32 caracteres hexa. Lo verificamos con los vectores del RFC y los ejemplos del
> enunciado."

**Pase:** "Con la IP y el MD5, [nombre] cuenta cómo nos autenticamos."

---

## Parte 2 (~3 min): autenticación por UDP

**Mostrar:** `autenticacion.c`, función `armar_pedido`.

> "Firmamos `grupo05|<IP>|<secreto>` y mandamos `grupo05|<IP>|<firma>`. El secreto nunca viaja:
> el servidor también lo conoce, recalcula el MD5 y compara."

**Mostrar:** `enviar_pedido` y el frame 10 en Wireshark.

> "Lo mandamos con `sendto` al puerto 20262 por un socket UDP. La función devuelve el socket
> abierto, porque la respuesta vuelve al mismo puerto de origen. En la captura sale del 56872."

**Mostrar:** `esperar_respuesta` y `recibir_puerto`.

> "UDP no garantiza que llegue la respuesta, así que no podemos quedarnos bloqueados en
> `recvfrom`. Usamos `select` con un timeout. La consigna pide esperar 10 segundos y descartar
> en silencio lo que tenga la firma mal; para no reiniciar el reloj cada vez que descartamos
> algo, `recibir_puerto` mide con `gettimeofday` y en cada vuelta le pasa a `select` solo el
> tiempo que queda."

**Mostrar:** `verificar_respuesta` y el frame 12.

> "De cada datagrama chequeamos que venga de la IP del servidor, que tenga 4 campos y que la
> firma `md5(200|Port|<puerto>|<secreto>)` coincida. Así sabemos que la mandó el servidor y que
> nadie cambió el puerto. Si llega 400 o 401, mostramos el error y salimos. En la captura, la
> respuesta `200|Port|30029` vuelve al puerto 56872."

**Pase:** "Con el puerto, [nombre] cuenta el desafío 2."

---

## Parte 3 (~3 min): framing, validación y reportes

**Mostrar:** `conexion_tcp.c`, función `procesar_pdus`.

> "Nos conectamos enseguida al puerto, porque el servidor lo deja abierto 10 segundos. El
> problema central es que TCP es un flujo de bytes: no respeta dónde termina cada mensaje. Un
> `recv` puede traer media PDU, varias o una y media."

**Mostrar:** `pdu.c`, `extraer_pdu`, y en Wireshark los frames 19 y 23–27.

> "Por eso usamos un acumulador: cada `recv` se pega al final, y después sacamos todas las PDUs
> completas que haya buscando el `\r\n` byte por byte. Lo que no está completo queda esperando
> el siguiente `recv`. En la captura se ven los dos casos: el frame 19 trae dos PDUs juntas y
> salen dos reportes; los frames 23, 25 y 27 traen una PDU partida en tres y el reporte sale
> recién al final."

**Mostrar:** `validar_pdu`.

> "Cada PDU se valida: sin `&` es 400; timestamp con algo que no sea dígito, 405; más de 11
> caracteres, 407; más de 300 segundos, 406; mensaje vacío, 409; caracteres fuera de `0x20` a
> `0x7D`, 408; más de 100 caracteres, 410. Si no, 200."

**Mostrar:** `armar_reporte` y el frame 30.

> "Cada una se reenvía firmada por el mismo socket UDP. Las que tienen `|` no se reenvían,
> porque romperían los campos del reporte: es el frame 30, que no tiene UDP después.
> Terminamos cuando el servidor cierra: mandamos 9 reportes y en la web de la cátedra aparecen
> todos en verde."

**Cierre:** "Eso es todo. ¿Preguntas?"

---

## Si nos apuran: una frase por parte

1. "La IP pública la sacamos con HTTP escrito a mano sobre TCP, y la firma es un MD5 propio
   verificado con los vectores del RFC."
2. "Nos autenticamos por UDP con un MD5 del secreto, esperamos como mucho 10 segundos en total
   con `select` y solo aceptamos un 200 firmado por el servidor."
3. "Como TCP no respeta los límites de los mensajes, acumulamos los bytes y cortamos por
   `\r\n`; cada PDU se valida, se firma y se reporta por UDP."
