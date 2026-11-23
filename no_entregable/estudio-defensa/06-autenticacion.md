# `autenticacion.c`: el desafío 1 por UDP

## Qué resuelve

Todo lo que pasa por UDP:

1. armar el pedido firmado (`armar_pedido`);
2. mandarlo al puerto 20262 (`enviar_pedido`);
3. esperar la respuesta como mucho 10 segundos (`recibir_puerto`, que usa `esperar_respuesta`);
4. verificar que sea auténtica y sacar el puerto TCP (`verificar_respuesta`, que usa
   `separar_campos`).

También tiene `enviar_al_servidor`, que se usa en el desafío 2 para mandar los reportes por el
mismo socket.

El protocolo, según la consigna:

| Mensaje | Formato |
|---|---|
| Pedido (nosotros → servidor) | `<USUARIO>\|<IP_PUBLICA>\|md5(<USUARIO>\|<IP_PUBLICA>\|<SECRETO>)` |
| Respuesta OK (servidor → nosotros) | `200\|Port\|<puerto>\|md5(200\|Port\|<puerto>\|<SECRETO>)` |
| Respuestas de error | `400\|Message\|Bad format\|000…0` y `401\|Message\|Unauthorized\|000…0` |

Abajo está el archivo completo, en orden. Conviene leerlo con `src/autenticacion.c` abierto al
lado.

---

## El header: `autenticacion.h`

```c
#ifndef AUTENTICACION_H
#define AUTENTICACION_H
```

- La guarda de inclusión, como en todos los headers.

```c
#define PEDIDO_LEN 256
#define RESPUESTA_TIMEOUT_MS 10000
#define RESPUESTA_OK 0
#define RESPUESTA_ERROR 1
#define RESPUESTA_INVALIDA 2
```

- **`PEDIDO_LEN 256`**: el tamaño de los buffers del pedido, de la respuesta y de los textos a
  firmar. Nuestros mensajes miden unos 50 bytes, así que sobra.
- **`RESPUESTA_TIMEOUT_MS 10000`**: los **10 segundos** que pide la consigna, en milisegundos.
- **Los tres resultados de `verificar_respuesta`**, con nombre para que el código se lea solo:
  - **`RESPUESTA_OK`**: un `200` con firma válida.
  - **`RESPUESTA_ERROR`**: el servidor rechazó el pedido (`400` o `401`).
  - **`RESPUESTA_INVALIDA`**: cualquier otra cosa; se **descarta en silencio**.

Después vienen los siete prototipos, cada uno con un comentario que explica qué recibe y qué
devuelve:

```c
/* arma el pedido de autenticacion <USUARIO>|<IP>|<FIRMA>, donde
   FIRMA = md5(<USUARIO>|<IP>|<SECRETO>). Lo deja en 'pedido'.
   Devuelve 0 si salio bien, 1 si hubo error. */
int armar_pedido(char *usuario, char *ip, char *secreto, char *pedido, int pedido_len);

/* manda 'pedido' por UDP a ip_servidor, puerto 20262.
   Devuelve el socket (para despues recibir la respuesta por el mismo),
   o -1 si hubo error. */
int enviar_pedido(char *ip_servidor, char *pedido);

/* espera hasta 10 segundos a que llegue un datagrama por el socket s.
   Si llega, lo deja en 'respuesta' (terminado en '\0') y la IP de
   quien lo mando en 'ip_origen'.
   Devuelve 0 si llego, 1 si se vencio el tiempo, -1 si hubo error. */
int esperar_respuesta(int s, int timeout_ms, char *respuesta, int respuesta_len, char *ip_origen);

/* separa "A|B|C|D" en sus 4 campos. MODIFICA 'mensaje': reemplaza cada '|'
   por '\0' y deja en campos[0..3] punteros al principio de cada campo.
   Devuelve 0 si hay exactamente 4 campos, 1 si hay mas o menos. */
int separar_campos(char *mensaje, char **campos);

/* analiza la respuesta del servidor <CODE>|<ACTION>|<TEXT>|<SIGNATURE>.
   - 200 con firma md5(<CODE>|<ACTION>|<TEXT>|<SECRETO>) valida: deja el
     puerto en *puerto y devuelve RESPUESTA_OK.
   - 400 o 401: muestra el error y devuelve RESPUESTA_ERROR.
   - Cualquier otra cosa (formato raro, firma que no coincide, puerto
     invalido): devuelve RESPUESTA_INVALIDA sin mostrar nada.
   No modifica 'respuesta'. */
int verificar_respuesta(char *respuesta, char *secreto, int *puerto);

/* espera la respuesta del servidor, como maximo RESPUESTA_TIMEOUT_MS EN TOTAL.
   Descarta en silencio lo que no venga de ip_servidor o sea invalido, y sigue
   esperando el tiempo que queda.
   Devuelve 0 y deja el puerto en *puerto si llego un 200 valido, 1 si no. */
int recibir_puerto(int s, char *ip_servidor, char *secreto, int *puerto);

/* manda 'mensaje' en un datagrama UDP a ip_servidor, puerto 20262, usando
   el socket 's' que ya existe (el de la autenticacion).
   Devuelve 0 si salio bien, 1 si hubo error. */
int enviar_al_servidor(int s, char *ip_servidor, char *mensaje);

#endif
```

- **Los comentarios son el "contrato"** de cada función: qué recibe, qué deja en los buffers y qué
  devuelve. Quien la usa no necesita leer el código.
- **Tres convenciones de retorno distintas**, según lo que devuelve la función:
  - las que pueden salir bien o mal devuelven **0 o 1** (`armar_pedido`, `recibir_puerto`,
    `enviar_al_servidor`, `separar_campos`);
  - las que devuelven un **socket** usan **-1** para el error (`enviar_pedido`);
  - `esperar_respuesta` tiene **tres** resultados (0 llegó, 1 timeout, -1 error), y
    `verificar_respuesta` usa las tres constantes `RESPUESTA_*`.
- **`int *puerto`**: un puntero, para que la función escriba el puerto en la variable del que
  llama (es como "devolver" un segundo valor).
- **`char **campos`**: un arreglo de punteros a strings (`char *campos[4]` en quien llama).
- `separar_campos` está en el header aunque solo la use `verificar_respuesta`, para poder
  probarla desde `test_autenticacion.c`.

Los vemos uno por uno abajo, con su código.

---

## Líneas 1 a 17: includes y constante

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>  // necesario para memset(), strlen()
#include <errno.h>   // necesario para codigos de errores
#include <netinet/in.h>
#include <netdb.h>   // necesario para getaddrinfo()
#include <unistd.h>  // necesario para close()
#include <sys/select.h> // necesario para select()
#include <arpa/inet.h>  // necesario para inet_ntop()
#include "autenticacion.h"
#include "md5.h"
#include "ip_publica.h" // por INET4_ADDRSTRLEN

#define SERVIDOR_PUERTO_UDP "20262"
```

| Include | Qué nos da |
|---|---|
| `<stdio.h>` | `fprintf`, `snprintf` |
| `<stdlib.h>` | `strtol` (texto → número, para el puerto) |
| `<sys/time.h>` | `gettimeofday` y `struct timeval` |
| `<sys/types.h>`, `<sys/socket.h>` | `socket`, `sendto`, `recvfrom`, `socklen_t`, `AF_INET`, `SOCK_DGRAM` |
| `<string.h>` | `memset`, `strlen`, `strcmp`, `strcpy`, `strchr`, `strerror` |
| `<errno.h>` | `errno` |
| `<netinet/in.h>` | `struct sockaddr_in` (la dirección IPv4 con `sin_addr` y `sin_port`) |
| `<netdb.h>` | `getaddrinfo`, `freeaddrinfo`, `gai_strerror` |
| `<unistd.h>` | `close` |
| `<sys/select.h>` | `select`, `fd_set`, `FD_ZERO`, `FD_SET` |
| `<arpa/inet.h>` | `inet_ntop` (IP binaria → texto) |
| `"autenticacion.h"` | Nuestros prototipos y constantes |
| `"md5.h"` | `md5_hex` y `MD5_HEX_LEN`, para firmar |
| `"ip_publica.h"` | Solo por `INET4_ADDRSTRLEN`, el tamaño de una IP en texto |

- **`SERVIDOR_PUERTO_UDP "20262"`**: el puerto UDP del servidor de la cátedra, como **texto**
  porque se lo pasamos a `getaddrinfo`. Va en el `.c` y no en el `.h` porque ningún otro archivo
  lo necesita.

---

## `armar_pedido`: líneas 19 a 39

Arma `<USUARIO>|<IP>|<FIRMA>`. Devuelve 0 si salió bien y 1 si algo no entra en los buffers.

```c
int armar_pedido(char *usuario, char *ip, char *secreto, char *pedido, int pedido_len) {
    char a_firmar[PEDIDO_LEN];
    char firma[MD5_HEX_LEN];
    int n;
```

- **Parámetros:** el usuario y el secreto (vienen de la línea de comandos), la IP pública (de
  `obtener_ip`) y **dónde dejar el pedido** con su tamaño (el `main` le pasa un arreglo de
  `PEDIDO_LEN`).
- **`a_firmar`**: el texto que se hashea, que **sí** incluye el secreto. Es local: nunca sale de
  esta función.
- **`firma`**: los 32 caracteres hexa del MD5 + el `'\0'`.
- **`n`**: lo que devuelve `snprintf`.

```c
    // texto a firmar: <USUARIO>|<IP>|<SECRETO>
    n = snprintf(a_firmar, sizeof(a_firmar), "%s|%s|%s", usuario, ip, secreto);
    if (n < 0 || n >= (int) sizeof(a_firmar)) {
        fprintf(stderr, "Error: el texto a firmar no entra en el buffer\n");
        return 1;
    }
    md5_hex(a_firmar, n, firma);
```

- **`snprintf(destino, tamaño, formato, ...)`** escribe texto con formato, como `printf`, pero en
  un buffer. Los tres `%s` se reemplazan por el usuario, la IP y el secreto, separados por `|`:
  `grupo05|190.225.36.143|<secreto>`.
- **`snprintf` nunca escribe más de `tamaño` bytes** (incluido el `'\0'`), así que no puede
  desbordar el buffer. Y **devuelve cuántos caracteres habría escrito** si hubiera tenido lugar.
  Por eso el chequeo:
  - **`n >= sizeof(a_firmar)`**: el texto no entraba y quedó **cortado**. Firmar un texto cortado
    daría una firma equivocada, así que es un error.
  - **`n < 0`**: hubo un error de formato.
  - **`(int) sizeof(...)`**: `sizeof` devuelve un número sin signo; lo convertimos a `int` para
    compararlo con `n` sin que el compilador avise (con `-Wextra` avisaría).
- **`md5_hex(a_firmar, n, firma)`**: calcula el MD5 de los `n` caracteres y deja los 32 hexa en
  `firma`. Usamos `n` como largo, que ya lo tenemos, en vez de llamar a `strlen`.

```c
    // pedido: <USUARIO>|<IP>|<FIRMA>
    n = snprintf(pedido, pedido_len, "%s|%s|%s", usuario, ip, firma);
    if (n < 0 || n >= pedido_len) {
        fprintf(stderr, "Error: el pedido no entra en el buffer\n");
        return 1;
    }
    return 0;
}
```

- El mismo formato, pero con la **firma en lugar del secreto**:
  `grupo05|190.225.36.143|1baf07b1…`. Esto es lo que viaja por la red.
- **Por qué funciona como autenticación:** el servidor también conoce nuestro secreto. Recalcula
  `md5(grupo05|IP|secreto)` y lo compara con la firma que mandamos. Si alguien no conoce el
  secreto, no puede fabricar una firma válida. Y el secreto **nunca viaja** por la red.
- Acá no hace falta el cast `(int)` porque `pedido_len` ya es un `int`.

---

## `enviar_pedido`: líneas 41 a 77

Crea el socket UDP, manda el pedido y **devuelve el socket abierto** (o -1 si hubo error).

### Líneas 41 a 50: variables y `hints`

```c
int enviar_pedido(char *ip_servidor, char *pedido) {
    int s; // file descriptor
    struct addrinfo hints;
    struct addrinfo *servinfo; // es donde van los resultados
    int status;
    int enviados;

    memset(&hints,0,sizeof(hints));     // es necesario inicializar con ceros
    hints.ai_family = AF_INET;          // address family
    hints.ai_socktype = SOCK_DGRAM;     // socket type: UDP
```

- Las mismas variables que en `consulta_ip`: el socket, la plantilla `hints`, el resultado
  `servinfo` y el `status` de `getaddrinfo`. **`enviados`** guarda lo que devuelve `sendto`.
- **La diferencia con TCP:** `ai_socktype = SOCK_DGRAM`, o sea **UDP**.

### Líneas 52 a 57: armar la dirección del servidor

```c
    // armamos la direccion del servidor (IP + puerto)
    if ( (status = getaddrinfo(ip_servidor, SERVIDOR_PUERTO_UDP, &hints, &servinfo))!=0)
    {
      fprintf(stderr, "Error en getaddrinfo: %s\n",gai_strerror(status));
      return -1;
    }
```

- Le pasamos la **IP del servidor como texto** (`"167.114.129.206"`). `getaddrinfo` acepta un
  nombre o una IP: con una IP no hace consulta DNS, solo la convierte y arma la estructura con el
  puerto 20262.
- Si falla, devuelve **-1** (y no 1): esta función devuelve un socket, y como un socket válido
  puede ser 0, 1 o cualquier número positivo, el error tiene que ser un valor que nunca sea un
  socket. Es la misma convención de `socket()`.

### Líneas 58 a 65: crear el socket UDP

```c
    // creamos el socket UDP
    s = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (s == -1)
    {
      fprintf(stderr,"Error en socket %i: %s\n",errno,strerror(errno));
      freeaddrinfo(servinfo);
      return -1;
    }
```

- Igual que en TCP, con los datos del resultado de `getaddrinfo`.
- En este camino de error sí se libera `servinfo` antes de salir.

### Líneas 67 a 77: mandar el pedido

```c
    // mandamos el pedido en un datagrama (sin connect: le decimos el destino a sendto)
    enviados = sendto(s, pedido, strlen(pedido), 0, servinfo->ai_addr, servinfo->ai_addrlen);
    freeaddrinfo(servinfo);
    if (enviados == -1)
    {
      fprintf(stderr,"Error en sendto %i: %s\n",errno,strerror(errno));
      close(s);
      return -1;
    }
    return s;
}
```

- **`sendto(socket, datos, largo, flags, destino, largo_destino)`** manda **un datagrama**. A
  diferencia de `send`, recibe la **dirección de destino** como parámetro, porque en UDP no hay
  conexión que la recuerde.
- **No hay `connect` ni handshake:** UDP es no orientado a conexión. El datagrama sale
  directamente.
- **No hacemos `bind`**: el sistema operativo le asigna al socket un **puerto efímero** en este
  primer `sendto` (en la captura fue el 56872).
- **`strlen(pedido)`**: mandamos solo los caracteres, sin el `'\0'`.
- **`freeaddrinfo`** va antes del chequeo para liberar la lista en los dos caminos, el bueno y el
  de error.
- **`return s`, sin cerrarlo.** Es una decisión clave del TP:
  - el servidor contesta al **puerto de origen** del pedido; si cerráramos este socket y
    abriéramos otro, el nuevo tendría otro puerto y la respuesta no le llegaría;
  - el **NAT** del router creó una entrada para ese puerto cuando salió el pedido, y es esa
    entrada la que deja pasar la respuesta;
  - por eso los reportes del desafío 2 también salen por este mismo socket.

---

## `esperar_respuesta`: líneas 79 a 119

Espera **un** datagrama con un tiempo máximo. Devuelve **0** si llegó, **1** si se venció el
tiempo y **-1** si hubo error.

### Por qué hace falta `select`

UDP no garantiza nada: el pedido o la respuesta se pueden perder. Y `recvfrom` es
**bloqueante**: si lo llamáramos directamente y no llegara nada, el programa se quedaría
esperando para siempre (es lo que muestra el `ex05-01` de la clase). `select` espera hasta que
pase **una de dos cosas**: que el socket tenga algo para leer, o que se venza el tiempo.

### Líneas 79 a 87: variables

```c
int esperar_respuesta(int s, int timeout_ms, char *respuesta, int respuesta_len, char *ip_origen) {
    // variables del select
    struct timeval tv;
    fd_set rfds;
    int retval;
    // variables para saber quien mando el datagrama
    struct sockaddr_in their_addr;
    socklen_t addrsize;
    int inbytes;
```

- **Parámetros:** el socket, cuánto esperar como máximo, dónde dejar la respuesta con su tamaño y
  dónde dejar la **IP de quien la mandó**.
- **`tv`** (`struct timeval`): el tiempo máximo de espera, en dos campos: segundos (`tv_sec`) y
  microsegundos (`tv_usec`).
- **`rfds`** (`fd_set`): el **conjunto de descriptores** que `select` vigila para lectura (*read
  file descriptors*).
- **`retval`**: lo que devuelve `select`.
- **`their_addr`** (`struct sockaddr_in`): la dirección IPv4 de **quien mandó** el datagrama, con
  sus campos `sin_addr` (la IP) y `sin_port` (el puerto). Los nombres son los de los ejemplos de
  clase.
- **`addrsize`** (`socklen_t`): el tamaño de `their_addr`.
- **`inbytes`**: los bytes que trajo `recvfrom`.

### Líneas 89 a 94: preparar el `select`

```c
    // pre-select: configuramos el timeout
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    // pre-select: borramos el SET de file descriptors y agregamos el socket
    FD_ZERO(&rfds);
    FD_SET(s, &rfds);
```

- **De milisegundos a `timeval`:** por ejemplo, 7350 ms →
  `tv_sec = 7350 / 1000 = 7` y `tv_usec = (7350 % 1000) * 1000 = 350 * 1000 = 350000`
  microsegundos.
- **`FD_ZERO(&rfds)`** vacía el conjunto.
- **`FD_SET(s, &rfds)`** agrega nuestro socket. Es el único que vigilamos.
- **Se arman en cada llamada**, porque `select` los modifica al volver (es el "pre-select" del
  `ex08` de la clase). Como `recibir_puerto` llama a esta función en un loop, cada vuelta arranca
  con valores frescos.

### Líneas 96 a 104: esperar

```c
    // esperamos hasta que el socket tenga algo para leer o se venza el tiempo
    retval = select(s + 1, &rfds, NULL, NULL, &tv);
    if (retval == -1)
    {
      fprintf(stderr,"Error en select %i: %s\n",errno,strerror(errno));
      return -1;
    }
    if (retval == 0)
      return 1; // timeout: no llego nada
```

- **`select(nfds, lectura, escritura, excepciones, timeout)`**:
  - **`s + 1`**: el primer argumento es **el descriptor más alto + 1**. Como solo vigilamos `s`,
    es `s + 1`.
  - **`&rfds`**: el conjunto para **lectura**, el único que nos interesa.
  - **`NULL, NULL`**: no vigilamos escritura ni excepciones.
  - **`&tv`**: el tiempo máximo.
- **Lo que devuelve:**
  - **`-1`**: error. Se muestra y se devuelve -1.
  - **`0`**: se venció el tiempo sin que llegara nada. Se devuelve **1** ("timeout").
  - **`> 0`**: cuántos descriptores están listos. Como vigilamos uno solo, es 1: nuestro socket
    tiene un datagrama esperando, y el `recvfrom` de abajo **no va a bloquear**.

### Líneas 106 a 114: leer el datagrama

```c
    // hay un datagrama esperando: lo leemos
    addrsize = sizeof(their_addr); // very important: inicializarlo antes de recvfrom
    memset(respuesta, 0, respuesta_len);
    inbytes = recvfrom(s, respuesta, respuesta_len - 1, 0, (struct sockaddr *) &their_addr, &addrsize);
    if (inbytes == -1)
    {
      fprintf(stderr,"Error en recvfrom %i: %s\n",errno,strerror(errno));
      return -1;
    }
```

- **`addrsize = sizeof(their_addr)`**: `addrsize` es de **entrada y salida**. Entra el tamaño del
  espacio que tenemos para la dirección y sale el tamaño que realmente usó el sistema. Si no se
  inicializa, el comportamiento es indefinido. El comentario "very important" es el mismo del
  cliente UDP del workshop de la clase (el server de ese workshop se olvida de hacerlo).
- **`memset(respuesta, 0, ...)`**: la respuesta en ceros, para que lo que leamos quede terminado
  en `'\0'` y se pueda usar como string.
- **`recvfrom(s, buffer, tamaño, flags, &dirección, &tamaño_dirección)`**:
  - lee **un datagrama entero** (en UDP no hay mensajes "partidos": llega todo o nada);
  - **`respuesta_len - 1`**: deja lugar para el `'\0'`;
  - llena **`their_addr` con quién lo mandó** (IP y puerto de origen). Es lo que diferencia a
    `recvfrom` de `recv`.
- **`(struct sockaddr *) &their_addr`**: `recvfrom` recibe un `struct sockaddr *` genérico (sirve
  para IPv4 y para IPv6). Le pasamos nuestra `sockaddr_in` con un *cast*; es el "único propósito
  de `struct sockaddr`" del que habla la clase.

### Líneas 116 a 119: la IP de origen como texto

```c
    // pasamos la IP de quien lo mando a texto
    inet_ntop(AF_INET, &(their_addr.sin_addr), ip_origen, INET4_ADDRSTRLEN);
    return 0;
}
```

- **`inet_ntop`** (*network to presentation*) convierte la IP de **binario a texto**
  (`"167.114.129.206"`) y la deja en `ip_origen`. Es la inversa de la `inet_pton` de
  `ip_publica.c`.
- **`their_addr.sin_addr`**: el campo con la IP. Se usa `.` (y no `->`) porque `their_addr` es
  una estructura, no un puntero.
- **`INET4_ADDRSTRLEN`**: el tamaño del buffer de destino (16).
- Devuelve 0: llegó un datagrama. **Todavía no sabemos si es válido**: eso lo decide
  `recibir_puerto`.

---

## `separar_campos`: líneas 121 a 140

Corta `"A|B|C|D"` en sus 4 campos. Devuelve 0 si hay **exactamente** 4 y 1 si hay más o menos.

```c
int separar_campos(char *mensaje, char **campos) {
    char *pipe;
    int i;
```

- **`mensaje`**: el texto a separar. **Se modifica** (ver abajo).
- **`campos`**: un arreglo de **4 punteros** (`char *campos[4]` en quien llama). La función deja
  en cada uno la dirección donde empieza un campo. `char **` = "puntero a puntero a char", que es
  como se pasa un arreglo de strings.
- **`pipe`**: apunta al próximo `|` encontrado.

```c
    // los primeros 3 campos terminan cada uno en un '|'
    for (i = 0; i < 3; i++) {
        campos[i] = mensaje;
        pipe = strchr(mensaje, '|');
        if (pipe == NULL)
            return 1;          // faltan campos
        *pipe = '\0';          // cortamos el campo ahi
        mensaje = pipe + 1;    // el siguiente empieza despues del '|'
    }
```

En cada vuelta:

1. **`campos[i] = mensaje`**: el campo `i` empieza donde estamos parados.
2. **`strchr(mensaje, '|')`** busca el próximo `|` y devuelve un puntero a él, o `NULL` si no hay.
3. Si no hay, **faltan campos** (había menos de 4): devuelve 1.
4. **`*pipe = '\0'`**: reemplaza el `|` por un `'\0'`. Así, el campo `i` queda **terminado** y es
   un string independiente.
5. **`mensaje = pipe + 1`**: el siguiente campo empieza justo después.

```c
    // el cuarto campo es todo lo que queda, y no puede tener mas '|'
    campos[3] = mensaje;
    if (strchr(mensaje, '|') != NULL)
        return 1;              // sobran campos
    return 0;
}
```

- El **cuarto campo** es todo lo que queda hasta el `'\0'` final.
- Si en ese resto hay **otro `|`**, había **más de 4 campos**: devuelve 1.

Ejemplo: `"200|Port|30029|4238…"` queda en memoria como `200␀Port␀30029␀4238…` (cada `␀` es
un `'\0'`), con
`campos = {"200", "Port", "30029", "4238…"}` apuntando adentro del mismo buffer. **No se copia
nada.**

**¿Por qué no `strtok`?** `strtok` saltea separadores seguidos (a `"200||17189|abc"` le
encontraría 3 campos en vez de 4 con uno vacío) y guarda estado en una variable global. Con
`strchr` controlamos exactamente cuántos campos hay.

---

## `verificar_respuesta`: líneas 142 a 183

Clasifica una respuesta en `RESPUESTA_OK`, `RESPUESTA_ERROR` o `RESPUESTA_INVALIDA`.

### Líneas 142 a 149: variables

```c
int verificar_respuesta(char *respuesta, char *secreto, int *puerto) {
    char copia[PEDIDO_LEN];
    char *campos[4];
    char a_firmar[PEDIDO_LEN];
    char firma[MD5_HEX_LEN];
    char *fin;
    long numero;
    int n;
```

- **`puerto`** es un **puntero**: así la función puede "devolver" el puerto además del resultado
  (escribe en la variable del que llama con `*puerto = ...`).
- **`copia`**: donde separamos los campos sin tocar la respuesta original.
- **`campos`**: los 4 punteros que llena `separar_campos`.
- **`a_firmar`** y **`firma`**: para recalcular la firma.
- **`fin`** y **`numero`**: para convertir el puerto de texto a número con `strtol`.

### Líneas 151 a 156: copiar y separar

```c
    // trabajamos sobre una copia porque separar_campos modifica el mensaje
    if (strlen(respuesta) >= sizeof(copia))
        return RESPUESTA_INVALIDA;
    strcpy(copia, respuesta);
    if (separar_campos(copia, campos) != 0)
        return RESPUESTA_INVALIDA;
```

- **La copia:** `separar_campos` reemplaza los `|` por `'\0'`, y la función promete no modificar
  la respuesta (lo dice el header).
- **Antes de copiar, chequea que entre**: si la respuesta tuviera 256 caracteres o más, `strcpy`
  escribiría fuera de `copia`. En ese caso es una respuesta rara y se descarta.
- **`strcpy(destino, origen)`** copia el string con su `'\0'`.
- Si no tiene **exactamente 4 campos**, no es una respuesta válida: se descarta.

### Líneas 158 a 162: los errores del servidor

```c
    // 400 o 401: el servidor rechazo el pedido (la firma son 32 ceros, no se verifica)
    if (strcmp(campos[0], "400") == 0 || strcmp(campos[0], "401") == 0) {
        fprintf(stderr, "Error del servidor %s: %s\n", campos[0], campos[2]);
        return RESPUESTA_ERROR;
    }
```

- **`strcmp(a, b)`** devuelve 0 si los dos strings son iguales.
- Si el código es **400** (formato incorrecto) o **401** (no autorizado: la firma no le dio
  bien al servidor), mostramos el código y el texto del error (`campos[2]`, por ejemplo
  `Unauthorized`) y devolvemos `RESPUESTA_ERROR`.
- **No se verifica la firma**: estas respuestas vienen con 32 ceros, porque si el servidor no nos
  reconoció no sabe qué secreto usar para firmar.

### Líneas 164 a 166: solo aceptamos `200|Port`

```c
    // lo unico que aceptamos ademas es 200|Port|...
    if (strcmp(campos[0], "200") != 0 || strcmp(campos[1], "Port") != 0)
        return RESPUESTA_INVALIDA;
```

- Cualquier otro código o acción no es algo que esperemos: se descarta en silencio.

### Líneas 168 a 174: verificar la firma

```c
    // verificamos la firma: md5(<CODE>|<ACTION>|<TEXT>|<SECRETO>)
    n = snprintf(a_firmar, sizeof(a_firmar), "%s|%s|%s|%s", campos[0], campos[1], campos[2], secreto);
    if (n < 0 || n >= (int) sizeof(a_firmar))
        return RESPUESTA_INVALIDA;
    md5_hex(a_firmar, n, firma);
    if (strcmp(firma, campos[3]) != 0)
        return RESPUESTA_INVALIDA;  // firma invalida: descartar en silencio
```

- **Recalculamos** la firma que *tendría que* haber mandado el servidor:
  `md5(200|Port|30029|<secreto>)`.
- La comparamos con la que vino (`campos[3]`). Si coinciden, sabemos dos cosas:
  1. la respuesta la mandó **alguien que conoce el secreto** (el servidor);
  2. **nadie cambió** el puerto en el camino (si cambiara un solo carácter, el MD5 sería
     totalmente distinto).
- Si no coinciden, **se descarta en silencio**, como pide la consigna: no se muestra nada y se
  sigue esperando.
- Este chequeo tiene un test propio: el puerto alterado (17190 con la firma del 17189) da
  `RESPUESTA_INVALIDA`.

### Líneas 176 a 183: validar el puerto

```c
    // el TEXT tiene que ser un numero de puerto valido (1 a 65535)
    numero = strtol(campos[2], &fin, 10);
    if (fin == campos[2] || *fin != '\0' || numero < 1 || numero > 65535)
        return RESPUESTA_INVALIDA;

    *puerto = (int) numero;
    return RESPUESTA_OK;
}
```

- **`strtol(texto, &fin, 10)`** convierte texto a número en base 10, y deja **`fin` apuntando al
  primer carácter que no pudo convertir**. Usamos `strtol` y no `atoi` porque `atoi` no avisa si
  el texto no era un número.
- Las cuatro condiciones de rechazo:
  - **`fin == campos[2]`**: no convirtió nada; el campo no empieza con un número (`"abc"`).
  - **`*fin != '\0'`**: después del número había algo más (`"30029x"`).
  - **`numero < 1 || numero > 65535`**: fuera del rango de puertos, que son números de **16
    bits** (`"70000"`).
- **`*puerto = (int) numero`**: escribe el puerto en la variable del que llamó. El `*` adelante
  del puntero significa "lo apuntado".

---

## `recibir_puerto`: líneas 185 a 225

El loop del desafío 1. Devuelve 0 con el puerto en `*puerto`, o 1 si no hubo una respuesta
válida en 10 segundos.

### El problema que resuelve

La consigna pide dos cosas que se cruzan:

- esperar la respuesta **10 segundos**;
- **descartar en silencio** lo que sea inválido y seguir esperando.

Si llega un datagrama inválido a los 3 segundos, lo descartamos y hay que seguir esperando, pero
solo los **7 segundos que quedan**, no 10 de nuevo. Si no, una seguidilla de datagramas
inválidos nos dejaría esperando indefinidamente.

### Líneas 185 a 192: variables y la hora de inicio

```c
int recibir_puerto(int s, char *ip_servidor, char *secreto, int *puerto) {
    struct timeval inicio, ahora;
    int transcurrido_ms, restante_ms;
    char respuesta[PEDIDO_LEN];
    char ip_origen[INET4_ADDRSTRLEN];
    int resultado;

    gettimeofday(&inicio, NULL); // anotamos cuando empezamos a esperar
```

- **`inicio` y `ahora`**: dos marcas de tiempo (`struct timeval`, con segundos y microsegundos).
- **`transcurrido_ms` / `restante_ms`**: cuánto pasó y cuánto queda de los 10 segundos.
- **`respuesta`** e **`ip_origen`**: dónde `esperar_respuesta` deja el datagrama y quién lo mandó.
- **`gettimeofday(&inicio, NULL)`** guarda la hora actual con precisión de microsegundos (en
  segundos desde el 1/1/1970, el *epoch*). El segundo parámetro es la zona horaria, que no se usa
  y va en `NULL`. Es la función que la clase usa en el workshop para medir el tiempo de
  respuesta.

### Líneas 194 a 202: cuánto queda

```c
    while (1)
    {
        // cuanto tiempo queda de los 10 segundos
        gettimeofday(&ahora, NULL);
        transcurrido_ms = (ahora.tv_sec - inicio.tv_sec) * 1000
                        + (ahora.tv_usec - inicio.tv_usec) / 1000;
        restante_ms = RESPUESTA_TIMEOUT_MS - transcurrido_ms;
        if (restante_ms <= 0)
            break;
```

- En cada vuelta tomamos la hora de nuevo y calculamos **cuántos milisegundos pasaron**:
  - la diferencia de segundos × 1000;
  - más la diferencia de microsegundos / 1000.
  La parte de microsegundos puede dar negativa (por ejemplo, de 3,9 s a 4,1 s:
  `(4 − 3) × 1000 + (100000 − 900000) / 1000 = 1000 − 800 = 200` ms), y la suma igual da bien.
- **`restante_ms = 10000 − transcurrido_ms`**. Si ya no queda tiempo, `break` sale del loop.

### Líneas 204 a 209: esperar lo que queda

```c
        // esperamos un datagrama, como mucho el tiempo que queda
        resultado = esperar_respuesta(s, restante_ms, respuesta, sizeof(respuesta), ip_origen);
        if (resultado == -1)
            return 1;            // error (ya se mostro)
        if (resultado == 1)
            break;               // timeout
```

- Le pasamos a `esperar_respuesta` **solo el tiempo que queda**, no 10 segundos.
- **`-1`**: hubo un error de sistema (ya se mostró adentro): devolvemos 1.
- **`1`**: se venció el tiempo sin que llegara nada: salimos del loop.
- **`0`**: llegó un datagrama, y seguimos abajo.

### Líneas 211 a 213: ¿viene del servidor?

```c
        // si no viene del servidor, lo ignoramos
        if (strcmp(ip_origen, ip_servidor) != 0)
            continue;
```

- Si el datagrama no viene de la IP del servidor, no es la respuesta: **`continue`** vuelve al
  principio del loop (recalcula el tiempo que queda y sigue esperando).
- Compara **textos**, así que funciona porque al programa le pasamos la IP del servidor y no un
  nombre: si le pasáramos un nombre, nunca coincidiría con la IP que da `inet_ntop`.

### Líneas 215 a 221: verificar

```c
        resultado = verificar_respuesta(respuesta, secreto, puerto);
        if (resultado == RESPUESTA_OK)
            return 0;
        if (resultado == RESPUESTA_ERROR)
            return 1;            // 400/401 (ya se mostro)
        // RESPUESTA_INVALIDA: descartar en silencio y seguir esperando
    }
```

- **`RESPUESTA_OK`**: el puerto ya quedó en `*puerto` (lo escribió `verificar_respuesta`, porque
  le pasamos el mismo puntero): devolvemos 0.
- **`RESPUESTA_ERROR`**: el servidor nos rechazó con 400 o 401. No tiene sentido seguir
  esperando: devolvemos 1.
- **`RESPUESTA_INVALIDA`**: no hay ningún `if`, así que llega al final del `while` y **da otra
  vuelta**. Eso es "descartar en silencio y seguir esperando".

### Líneas 223 a 225: no llegó nada válido

```c
    fprintf(stderr, "Error: no hubo respuesta valida en %d segundos\n", RESPUESTA_TIMEOUT_MS / 1000);
    return 1;
}
```

- A esta línea solo se llega con un `break`, o sea cuando se acabaron los 10 segundos.
- `RESPUESTA_TIMEOUT_MS / 1000` = 10, para que el mensaje diga "10 segundos".

---

## `enviar_al_servidor`: líneas 227 a 252

La usa el desafío 2: manda un reporte al puerto 20262 **por el socket que ya existe**.

```c
int enviar_al_servidor(int s, char *ip_servidor, char *mensaje) {
    struct addrinfo hints;
    struct addrinfo *servinfo; // es donde van los resultados
    int status;
    int enviados;

    memset(&hints,0,sizeof(hints));     // es necesario inicializar con ceros
    hints.ai_family = AF_INET;          // address family
    hints.ai_socktype = SOCK_DGRAM;     // socket type: UDP

    // armamos la direccion del servidor (IP + puerto 20262)
    if ( (status = getaddrinfo(ip_servidor, SERVIDOR_PUERTO_UDP, &hints, &servinfo))!=0)
    {
      fprintf(stderr, "Error en getaddrinfo: %s\n",gai_strerror(status));
      return 1;
    }

    enviados = sendto(s, mensaje, strlen(mensaje), 0, servinfo->ai_addr, servinfo->ai_addrlen);
    freeaddrinfo(servinfo);
    if (enviados == -1)
    {
      fprintf(stderr,"Error en sendto %i: %s\n",errno,strerror(errno));
      return 1;
    }
    return 0;
}
```

- Es `enviar_pedido` **sin crear el socket**: recibe `s` (el de la autenticación), arma la
  dirección del servidor con `getaddrinfo` y manda un datagrama con `sendto`.
- **No cierra el socket**: lo va a seguir usando para los siguientes reportes, y lo cierra el
  `main` al final.
- Devuelve 0 o 1 (no un socket), así que el error es 1.
- Arma la dirección en **cada** reporte. Es un poco de trabajo repetido, pero con una IP no hay
  consulta DNS, así que es instantáneo, y deja la función independiente.

---

## Cómo se ve en la captura

| Frame | Qué es | Qué línea lo produce |
|---|---|---|
| 10 | `192.168.1.25:56872 → 167.114.129.206:20262`, 55 bytes de datos: `grupo05\|190.225.36.143\|1baf07b1…` | `sendto` de `enviar_pedido` |
| 12 | La respuesta vuelve al puerto **56872**, 47 bytes: `200\|Port\|30029\|4238aec9…` (pasaron 180 ms) | la lee `recvfrom` después de que `select` avisó |
| 18, 21, 22, … | Los reportes, todos desde el **mismo puerto 56872** | `sendto` de `enviar_al_servidor` |

## Preguntas que pueden hacer sobre este archivo

| Pregunta | Respuesta corta |
|---|---|
| ¿Por qué se manda la firma y no el secreto? | El servidor conoce el secreto y recalcula; así el secreto nunca viaja por la red |
| ¿Para qué sirve el chequeo de `snprintf`? | Si `n >= tamaño` el texto quedó cortado, y firmarlo daría una firma equivocada |
| ¿Por qué `sendto` y no `send`? | No hay `connect`: en UDP cada datagrama lleva la dirección de destino |
| ¿Quién elige el puerto de origen? | El sistema (puerto efímero), en el primer `sendto`, porque no hacemos `bind` |
| ¿Por qué no cierran el socket UDP? | La respuesta vuelve a ese puerto, que es el que el NAT tiene registrado; por el mismo motivo los reportes salen por ahí |
| ¿Por qué `select` y no `recvfrom` directo? | `recvfrom` bloquea; si el datagrama se pierde, sin timeout nos quedaríamos esperando para siempre |
| ¿Qué devuelve `select`? | Cuántos descriptores están listos; 0 si se venció el tiempo, -1 si hubo error |
| ¿Por qué se inicializa `addrsize`? | Es de entrada y salida: tiene que valer el tamaño del espacio antes de `recvfrom` |
| ¿Qué diferencia a `recvfrom` de `recv`? | `recvfrom` además dice quién mandó el datagrama |
| ¿Por qué no `strtok`? | Saltea separadores seguidos y usa estado global; con `strchr` contamos exactamente 4 campos |
| ¿Por qué no verifican la firma de un 401? | Viene con 32 ceros: si el servidor no nos reconoció, no sabe con qué secreto firmar |
| ¿Por qué recalculan el tiempo en cada vuelta? | Para esperar solo lo que resta de los 10 s después de descartar un datagrama inválido |
| ¿Por qué `strtol` y no `atoi`? | `strtol` dice dónde dejó de convertir, y así detectamos basura después del número |
