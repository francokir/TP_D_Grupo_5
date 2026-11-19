# Preguntas probables, con la respuesta corta

Pueden preguntar a cualquiera sobre cualquier parte. Cada respuesta es la versión para decir
en voz alta; entre paréntesis, dónde está en el código o en qué md se explica más.

## Generales

- **¿Qué hace el programa, en una frase?** Se autentica por UDP con una firma MD5, recibe un
  puerto, se conecta por TCP, separa las PDUs que llegan, las valida y reporta cada una firmada
  por UDP.
- **¿Por qué TCP para las PDUs y UDP para el resto?** Lo define la consigna, y sirve para ver
  las dos cosas: UDP conserva los límites de cada mensaje pero no garantiza la entrega; TCP
  garantiza la entrega y el orden, pero no conserva los límites de los mensajes.
- **¿Cómo se compila y se corre?** `make` (con `-Wall -Wextra`, sin warnings), `make test` y
  `./cliente <usuario> <secreto> <ip_servidor>`.
- **¿Por qué el secreto se pasa por argumento?** Para que no quede en el código ni en el
  repositorio público.
- **¿Cómo manejan los errores?** Chequeamos el retorno de cada llamada; mostramos `errno` con
  `strerror`, y para `getaddrinfo`, `gai_strerror`. El `main` corta con código 1.
- **¿Qué filtro usaron en Wireshark?** `ip.addr == 167.114.129.206 || tcp.port == 80`.
- **¿Qué es un socket?** Un descriptor, un número que representa un extremo de comunicación,
  parecido a un archivo abierto. Se crea con `socket()` y se cierra con `close()`.

## IP pública y HTTP (`ip_publica.c`, md 04)

- **¿Por qué hace falta preguntar la IP?** La PC tiene IP privada (`192.168.1.25`) y sale por
  NAT con la del router. ipify ve el origen ya traducido y nos lo devuelve.
- **¿Qué hace `getaddrinfo`?** Resuelve el nombre por DNS y devuelve la dirección armada para
  `connect`. Le pedimos IPv4 y TCP con `hints`.
- **¿Por qué el puerto es `"80"` entre comillas?** El parámetro es el servicio como texto.
- **¿Qué hace `connect` en TCP?** El 3-way handshake: SYN, SYN-ACK, ACK.
- **¿Por qué `Host:`?** Es obligatorio en HTTP/1.1; permite varios sitios en una misma IP
  (virtual hosting). Sin él da 400.
- **¿Por qué `Connection: close`?** Para que el servidor cierre al terminar y saber que la
  respuesta está completa cuando `recv` devuelve 0.
- **¿Qué devuelve `recv`?** Bytes leídos, 0 si el otro cerró (EOF), -1 si hubo error.
- **¿Por qué el loop de `recv`?** Porque la respuesta puede llegar en varios pedazos.
- **¿Por qué `buffer_len - 1 - total`?** Para escribir después de lo ya leído y dejar un byte
  para el `'\0'`.
- **¿Cómo encuentran la IP en la respuesta?** Buscamos `\r\n\r\n` (fin de headers), contamos
  dígitos y puntos con `strspn` y validamos con `inet_pton`.

## MD5 (`md5.c`, md 05)

- **¿Qué es un hash?** Una función que de cualquier mensaje saca un resultado de largo fijo
  (128 bits en MD5). Mismo mensaje, mismo hash; un bit distinto, otro hash; no se puede volver
  atrás.
- **¿Cómo es el padding?** `0x80`, ceros y el largo en bits en los últimos 8 bytes, en
  little-endian, completando bloques de 64 bytes.
- **¿Cuándo hacen falta dos bloques de cola?** Cuando sobran 56 bytes o más: no entran el
  `0x80` y los 8 bytes del largo.
- **¿Qué son `K` y `S`?** Constantes del RFC: `K` sale del seno, `S` son las rotaciones de
  cada paso.
- **¿Por qué `uint32_t`?** Necesitamos 32 bits exactos sin signo para que las sumas den la
  vuelta módulo 2³² y las rotaciones no arrastren el signo.
- **¿Cómo saben que está bien?** Vectores del RFC y ejemplos del enunciado (6/6), y el
  servidor aceptó la autenticación.
- **¿Es seguro MD5?** No para firmas hoy (colisiones). Se usaría HMAC-SHA256.

## Autenticación por UDP (`autenticacion.c`, md 06)

- **¿Por qué el secreto no se manda?** Lo vería cualquiera que capture el tráfico. Mandamos su
  huella y el servidor recalcula.
- **¿Para qué verifican la firma de la respuesta?** Para saber que viene del servidor y que
  nadie cambió el puerto. Sin el secreto no se puede armar una firma válida.
- **¿Qué es `SOCK_DGRAM`?** UDP: sin conexión; cada `sendto` es un datagrama independiente.
- **¿Por qué no cierran el socket después de mandar?** La respuesta vuelve a ese puerto de
  origen; un socket nuevo tendría otro puerto. Además, el NAT ya asoció ese puerto.
- **¿Quién eligió el puerto 56872?** El sistema operativo, porque no hicimos `bind`: es un
  puerto efímero asignado en el primer `sendto`.
- **¿Por qué `select`?** `recvfrom` se bloquearía para siempre si el datagrama se pierde.
  `select` espera hasta que haya datos o venza el tiempo.
- **¿Qué es `s + 1`?** El descriptor más alto que vigilamos, más uno.
- **¿Qué hacen `FD_ZERO` y `FD_SET`?** Vacían el conjunto de descriptores y agregan el nuestro.
- **¿Cómo cumplen los 10 segundos en total?** Medimos con `gettimeofday` y en cada vuelta le
  pasamos a `select` solo lo que queda.
- **¿Por qué inicializan `addrsize`?** Es de entrada y salida: entra el tamaño disponible y
  sale el real. Sin inicializar es comportamiento indefinido.
- **¿Qué hacen si llega un datagrama de otra IP?** Lo ignoran y siguen esperando.
- **¿Qué pasa con un 400 o 401?** Mostramos el error y terminamos. Tienen firma de 32 ceros
  porque el servidor no puede firmar sin reconocernos.
- **¿Por qué `strchr` y no `strtok`?** `strtok` saltea separadores seguidos y tiene estado
  global; con `strchr` controlamos que sean exactamente 4 campos, incluso vacíos.
- **¿Por qué trabajan sobre una copia?** `separar_campos` reemplaza los `|` por `'\0'`.
- **¿Cómo validan el puerto?** `strtol` con `fin`: tiene que convertirse entero y estar entre
  1 y 65535.
- **¿Podrían haber usado `connect` en UDP?** Sí: fija el destino y filtra el origen. Usamos
  `sendto` y chequeamos la IP a mano.

## Desafío 2 (`conexion_tcp.c` y `pdu.c`, md 07 y 08)

- **¿Qué es el framing?** Reconstruir dónde empieza y termina cada mensaje. TCP entrega un
  flujo de bytes sin límites, así que lo hacemos con el delimitador `\r\n`.
- **¿Cómo funciona el acumulador?** Cada `recv` se pega al final; después sacamos todas las
  PDUs completas buscando `\r\n`; lo incompleto queda para el siguiente `recv`.
- **¿Por qué dos loops?** El de afuera lee de la red hasta EOF; el de adentro saca 0, 1 o
  varias PDUs de lo acumulado.
- **¿Por qué no `strstr`?** Los bytes de `recv` no terminan en `'\0'`; buscamos solo dentro de
  `largo`.
- **¿Por qué `memmove`?** Origen y destino se superponen en el mismo arreglo.
- **¿Qué pasa si el `\r` llega en un `recv` y el `\n` en otro?** El `\r` queda último en el
  acumulador y no se toma como delimitador hasta que llega el `\n`. Está en los tests.
- **¿Qué pasa si llegan más de 4096 bytes sin `\r\n`?** Error. Se podría cortar a los 122.
- **¿En qué orden validan?** Estructura (400), timestamp (405, 407, 406), mensaje (409, 408,
  410). Reportamos el primer error.
- **¿Por qué el `unsigned char`?** Para que los bytes ≥ `0x80` no se lean como negativos.
- **¿`~` y `}`?** `~` = `0x7E`, fuera de rango → 408. `}` = `0x7D`, el último válido.
- **¿300 segundos exactos?** Válido; 301 → 406. Un timestamp del futuro → 200.
- **¿Por qué la hora es un parámetro de `validar_pdu`?** Para probarla con horas fijas.
- **¿Qué código tiene `17x0000000&Mensaje | invalido`?** 405, pero no se manda por el `|`.
- **¿El reporte lleva el `\r\n`?** No, y en la web de la cátedra salió todo en verde.
- **¿Por qué el mismo socket UDP para los reportes?** Usa el puerto que el NAT ya tiene asociado
  con el servidor.
- **¿Cómo termina?** `recv` devuelve 0 cuando llega el FIN del servidor.
- **¿Qué pasa si `connect` llega tarde?** El puerto ya está cerrado: `connect` falla y el
  programa termina con error.

## Las cuatro preguntas del informe

- **P1, PDUs con `|`:** parsear el reporte por los extremos (los demás campos tienen formato
  fijo), anteponer la longitud de la PDU, escapar el `|` (byte stuffing) o codificar en
  hexa/base64; siempre firmando la PDU original.
- **P2, ¿puede iniciar el servidor?** En la práctica no: NAT y firewall descartan un SYN que no
  pedimos. Es el problema de FTP activo; nuestro TP funciona "en pasivo". Habría que hacer
  `bind`/`listen`/`accept` y abrir puertos en el router.
- **P3, pérdidas:** lo que va por TCP solo se demora; si se pierde el pedido o la respuesta
  UDP, fallamos a los 10 s; un reporte UDP perdido no se nota (no hay ACK). El SYN tiene que
  llegar dentro de los 10 s del puerto (reintentos a 1, 3 y 7 s).
- **P4, aprendizajes:** TCP no respeta los mensajes; el socket UDP tiene que ser el mismo por el
  NAT; MD5 se depura por partes; los warnings eran bugs reales.

## Preguntas "trampa" o de mejora

- **¿Qué mejorarían?** Reintentos del pedido UDP, confirmación de reportes, timeout en el
  `recv` del desafío 2, cortar PDUs a los 122 bytes. (md 11)
- **¿Qué pasa si el servidor no cierra nunca la conexión?** Nos quedaríamos esperando en
  `recv`: no le pusimos timeout a ese loop.
- **¿Qué cambia si corren el programa desde la facultad?** La IP pública es otra y puede
  haber filtros en puertos no estándar; ipify lo resuelve y Wireshark sirve para ver si el
  tráfico sale.
- **¿Funcionaría con IPv6?** No tal como está: todo usa `AF_INET`, y el servidor espera una
  IPv4 en el pedido.
