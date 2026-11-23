# `ip_publica.c`: la IP pública por HTTP escrito a mano

## Para qué existe

El servidor de la cátedra valida que la IP que mandamos en el pedido sea la **pública**, o sea
la dirección con la que salimos a Internet. Nuestra PC tiene una IP privada
(`192.168.1.25`), que el router traduce por NAT a la pública (`190.225.36.143`). Desde adentro
no la podemos ver, porque la traducción la hace el router después de que el paquete sale de
nuestra máquina. Así que se la preguntamos a un servicio externo: `api.ipify.org` lee la
dirección de origen del paquete que le llega (ya traducida por el NAT) y la devuelve en el
cuerpo de la respuesta.

La consigna prohíbe libcurl y cualquier biblioteca HTTP, así que **el pedido HTTP lo escribimos
nosotros como texto** y lo mandamos por un socket TCP (HTTP corre sobre TCP).

El archivo tiene dos funciones, separadas a propósito: `consulta_ip` hace la parte de **red**
(conectarse, pedir y leer la respuesta) y `obtener_ip` hace la parte de **texto** (encontrar la
IP dentro de la respuesta), que se puede probar sin conectarse a nada.

Abajo está el archivo completo, en orden, con la explicación de cada línea. Conviene leerlo con
`src/ip_publica.c` abierto al lado.

---

## El header: `ip_publica.h`

```c
#ifndef IP_PUBLICA_H
#define IP_PUBLICA_H
```

- **Guarda de inclusión.** `#ifndef` = "si no está definido". La primera vez que un `.c` incluye
  este header, `IP_PUBLICA_H` no existe: se define y se lee el contenido. Si el mismo `.c` lo
  incluye otra vez (por ejemplo, porque lo incluye también otro header), `IP_PUBLICA_H` ya está
  definido y el contenido se saltea. Así no hay declaraciones duplicadas. Todos nuestros `.h`
  tienen este mismo esquema.

```c
#define INET4_ADDRSTRLEN 16
```

- El tamaño del buffer para guardar una IPv4 como texto: `"255.255.255.255"` son 15 caracteres,
  más el `'\0'` que termina el string, da 16. Lo usan el `main` (para declarar `ip`) y
  `autenticacion.c` (para la IP de origen de los datagramas). Es el mismo nombre que usan los
  ejemplos de clase.

```c
int consulta_ip(char *buffer, int buffer_len);   // llena el buffer con la respuesta HTTP
int obtener_ip(char *respuesta, char *ip);       // busca la IP dentro de respuesta
```

- Los **prototipos**: le dicen al compilador cómo se llaman las funciones, qué reciben y qué
  devuelven, para que `cliente.c` las pueda usar sin ver el código.
- Las dos devuelven `int`: **0 si salió bien, 1 si hubo error** (la convención de todo el TP).
- **Quien llama reserva la memoria** y la función la llena: `consulta_ip` recibe el buffer y su
  tamaño; `obtener_ip` recibe dónde dejar la IP. Nunca se devuelve un puntero a una variable
  local, porque esa memoria desaparece cuando la función termina.

```c
#endif
```

- Cierra el `#ifndef` del principio.

---

## Líneas 1 a 13: includes y constantes

```c
#include "ip_publica.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
```

Cada `#include` trae las declaraciones de las funciones que usamos. Con comillas (`"..."`) se
busca en nuestra carpeta; con `<...>`, en las del sistema.

| Include | Qué nos da |
|---|---|
| `"ip_publica.h"` | Nuestros prototipos y `INET4_ADDRSTRLEN`. Incluirlo en su propio `.c` hace que el compilador verifique que las funciones coincidan con lo declarado |
| `<stdio.h>` | `fprintf`, `stderr` (para los mensajes de error) |
| `<sys/types.h>` | Tipos básicos del sistema que usan los sockets (`ssize_t`, `socklen_t`) |
| `<sys/socket.h>` | `socket`, `connect`, `send`, `recv` y las constantes `AF_INET`, `SOCK_STREAM` |
| `<string.h>` | `memset`, `strlen`, `strerror`, `strncmp`, `strstr`, `strspn`, `memcpy` |
| `<errno.h>` | La variable `errno`, donde las syscalls dejan el código de error |
| `<netinet/in.h>` | Las estructuras de dirección de Internet (`struct in_addr`, `struct sockaddr_in`) |
| `<netdb.h>` | `getaddrinfo`, `freeaddrinfo`, `gai_strerror` y `struct addrinfo` |
| `<unistd.h>` | `close` |
| `<arpa/inet.h>` | `inet_pton` (texto → binario, para validar la IP) |

Son los mismos que usan los ejemplos de la clase de sockets (Práctica 3), más `arpa/inet.h`.

```c
#define IP_API_HOST "api.ipify.org"
#define IP_API_PORT "80"
```

- **`#define`** reemplaza el nombre por el texto antes de compilar. Tener el host y el puerto
  arriba evita repetirlos y deja claro de dónde sacamos la IP.
- **El puerto va como texto (`"80"`)** porque `getaddrinfo` recibe el *servicio* como string
  (también aceptaría `"http"`). El 80 es el puerto reservado de HTTP (un well known service).

---

## `consulta_ip(buffer, buffer_len)`: línea 15

Se conecta a ipify, manda el pedido HTTP y deja **toda la respuesta** (headers + cuerpo) en
`buffer`, terminada en `'\0'`. Devuelve 0 si salió bien y 1 si hubo error.

### Líneas 15 a 22: las variables

```c
int consulta_ip(char *buffer, int buffer_len) {
    int s; // file descriptor
    struct addrinfo hints;
    struct addrinfo *servinfo; // resultados

    int status;
    int inbytes;
    int total = 0;
```

- **`buffer` / `buffer_len`:** dónde dejar la respuesta y cuánto entra. El `main` le pasa un
  arreglo de 3000 bytes y `sizeof(respuesta)`. Se pasa el tamaño porque, dentro de la función,
  `buffer` es solo un puntero y no "sabe" cuánto mide.
- **`s`:** el socket. Es un `int` porque un socket es un **file descriptor**: un número que el
  sistema operativo usa para identificar algo abierto, igual que un archivo.
- **`hints`:** la "plantilla" donde le decimos a `getaddrinfo` qué tipo de dirección queremos.
  Es una estructura (no un puntero) porque la llenamos nosotros.
- **`servinfo`:** un **puntero**, porque la memoria del resultado la reserva `getaddrinfo` y nos
  devuelve dónde quedó. Después hay que liberarla con `freeaddrinfo`.
- **`status`:** el valor que devuelve `getaddrinfo` (0 = bien, otro = código de error).
- **`inbytes`:** cuántos bytes trajo cada `recv`.
- **`total`:** cuántos bytes llevamos acumulados en `buffer`. Arranca en 0.

### Líneas 24 a 26: qué dirección pedimos

```c
    memset(&hints,0,sizeof(hints));     // es necesario inicializar con ceros
    hints.ai_family = AF_INET;          // address family
    hints.ai_socktype = SOCK_STREAM;    // socket type: TCP
```

- **`memset(&hints, 0, sizeof(hints))`** pone todos los bytes de `hints` en cero. Es obligatorio:
  una variable local en C empieza con basura, y los campos que no tocamos tienen que valer 0
  ("sin preferencia"). `&hints` es la dirección de la estructura y `sizeof(hints)` su tamaño.
- **`ai_family = AF_INET`**: queremos solo direcciones **IPv4**. Lo pedimos porque el servidor de
  la cátedra espera una IPv4 en el pedido: si nos conectáramos a ipify por IPv6, nos devolvería
  nuestra IPv6.
- **`ai_socktype = SOCK_STREAM`**: la queremos para un socket **TCP**.

### Líneas 28 a 33: resolver el nombre

```c
    // consulta DNS con getaddrinfo. La respuesta se guarda en servinfo
    if ( (status = getaddrinfo(IP_API_HOST, IP_API_PORT, &hints, &servinfo))!=0)
    {
      fprintf(stderr, "Error en getaddrinfo: %s\n",gai_strerror(status));
      return 1;
    }
```

- **`getaddrinfo("api.ipify.org", "80", &hints, &servinfo)`** hace la **consulta DNS** del nombre
  y arma la dirección (IP + puerto) lista para pasarle a `connect`. Recibe:
  1. el nombre a resolver;
  2. el puerto como texto;
  3. la plantilla `hints`;
  4. **`&servinfo`**, la dirección de nuestro puntero, para que `getaddrinfo` pueda escribir en
     él dónde dejó los resultados.
- El resultado es una **lista enlazada** (un nombre puede tener varias IP). Usamos solo la
  primera.
- **`(status = getaddrinfo(...)) != 0`**: se guarda lo que devolvió en `status` y, en la misma
  línea, se compara con 0. Es el estilo de los ejemplos de clase.
- **`fprintf(stderr, ...)`** escribe el error en la **salida de errores** y no en la normal
  (`stdout`). Así los errores se ven aunque la salida normal se redirija a un archivo.
- **`gai_strerror(status)`** traduce el código a texto (por ejemplo, "Name or service not
  known"). Se usa esta y no `strerror(errno)` porque **`getaddrinfo` no usa `errno`**: devuelve
  su propio código de error.
- **`return 1`**: avisa al `main` que falló.

### Líneas 34 a 40: crear el socket

```c
    // armamos socket con respuesta de la consulta DNS getaddrinfo
    s = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (s == -1)
    {
      fprintf(stderr,"Error en socket %i: %s\n",errno,strerror(errno));
      return 1;
    }
```

- **`socket(familia, tipo, protocolo)`** crea el socket y devuelve su descriptor. En vez de
  escribir a mano `AF_INET, SOCK_STREAM, 0`, usamos los campos del resultado de `getaddrinfo`
  (el patrón del `ex03-01` de la clase): así el socket siempre coincide con la dirección.
- **`servinfo->ai_family`**: la flecha `->` accede a un campo **a través de un puntero** (es lo
  mismo que `(*servinfo).ai_family`).
- Crear el socket **no manda nada por la red**: solo reserva el descriptor en el sistema.
- **`s == -1`**: las syscalls devuelven **-1** cuando fallan y dejan el motivo en la variable
  global **`errno`**. `strerror(errno)` lo pasa a texto. El `%i` imprime el número del error.
- Detalle: en este camino de error no se llama a `freeaddrinfo`. No es grave porque el programa
  termina enseguida y el sistema libera toda la memoria, pero lo prolijo sería liberarla.

### Líneas 41 a 52: conectar (el 3-way handshake)

```c
    // establecemos conexión TCP con connect (3-way handshake)
    switch (connect(s, servinfo->ai_addr, servinfo->ai_addrlen))
    {
      case -1:
        fprintf(stderr,"Error en connect %i: %s\n",errno,strerror(errno));
        close(s);
        return 1;
      case 0:
        //printf("Conectado a %s\n\n", IP_API_HOST);
        break;
    }
    freeaddrinfo(servinfo);
```

- **`connect(s, dirección, largo)`** hace el **3-way handshake** de TCP: manda el SYN, recibe el
  SYN-ACK y responde con el ACK. Cuando devuelve **0**, la conexión está establecida. Mientras
  tanto, el programa queda bloqueado esperando.
- **`servinfo->ai_addr`** es la dirección del servidor (IP + puerto 80) que armó
  `getaddrinfo`, y **`ai_addrlen`** su tamaño en bytes.
- Como no hicimos `bind`, el sistema le asigna a nuestro socket un **puerto efímero** de origen.
- **El `switch`** sobre el valor de `connect` es el estilo de los ejemplos de clase; equivale a
  un `if (connect(...) == -1)`:
  - **`case -1`:** falló (por ejemplo, `Connection refused` si nadie escucha en ese puerto).
    Imprime el error, **cierra el socket** para no dejarlo abierto y devuelve 1.
  - **`case 0`:** conectó. El `printf` está comentado (lo usamos para depurar). **`break`** sale
    del `switch`; sin él, la ejecución "caería" al caso siguiente.
- **`freeaddrinfo(servinfo)`**: libera la lista que reservó `getaddrinfo`. Ya no hace falta,
  porque el socket está conectado y no se vuelve a usar la dirección.

### Líneas 54 a 60: el pedido HTTP

```c
    // envio del request HTTP
    char *request = "GET / HTTP/1.1\r\n"
                    "Host: " IP_API_HOST "\r\n"
                    "Connection: close\r\n"
                    "\r\n";

    send(s, request, strlen(request), 0);
```

- **Strings pegados:** en C, dos strings literales seguidos se juntan en uno solo al compilar.
  Como `IP_API_HOST` es `"api.ipify.org"`, el pedido completo queda:

  ```
  GET / HTTP/1.1\r\n
  Host: api.ipify.org\r\n
  Connection: close\r\n
  \r\n
  ```

- **`GET / HTTP/1.1`**: la **línea de pedido** de HTTP: el método (`GET` = traer), el recurso
  (`/` = la raíz del sitio) y la versión.
- **`\r\n`** (CR LF, retorno de carro + salto de línea): en HTTP **cada línea termina así**.
- **`Host:`** es **obligatorio en HTTP/1.1**. Le dice al servidor qué sitio queremos, porque en
  una misma IP puede haber varios (*virtual hosting*). Sin él, la respuesta es
  `400 Bad Request`.
- **`Connection: close`** le pide al servidor que **cierre la conexión** cuando termine de
  responder. HTTP/1.1, por defecto, deja la conexión abierta para más pedidos (persistente).
  Pidiendo que la cierre, sabemos que la respuesta está completa cuando `recv` devuelve 0, y no
  hace falta interpretar el header `Content-Length`.
- **La línea vacía** (`\r\n` sola) marca el **fin de los headers**. Sin ella, el servidor
  seguiría esperando más headers y no respondería.
- **`send(s, request, strlen(request), 0)`** manda los bytes por el socket. Se usa **`strlen`**
  (la cantidad de caracteres, sin el `'\0'`) y no `sizeof` (que acá daría el tamaño de un
  puntero, 8). El último 0 son las *flags*: con 0, `send` es igual a `write`.
- No chequeamos lo que devuelve `send`: si fallara, el `recv` siguiente también fallaría y ahí sí
  se reporta el error.

### Líneas 62 a 82: leer toda la respuesta

```c
    // loop de lectura (acumulamos toda la respuesta en el buffer)
    memset(buffer, 0, buffer_len);
    while (1)
    {
      if (total >= buffer_len - 1) {
        fprintf(stderr, "Error: la respuesta no entra en el buffer\n");
        close(s);
        return 1;
      }

      inbytes = recv(s, buffer + total, buffer_len - 1 - total, 0); // -1: dejo lugar para el '\0'
      if (inbytes == 0) // cuando recv devuelve 0 implica EOF
        break;
      if (inbytes == -1)
      {
        fprintf(stderr,"Error en recv %i: %s\n",errno,strerror(errno));
        close(s);
        return 1;
      }
      total += inbytes;
    }
```

- **¿Por qué un loop?** TCP es un **flujo de bytes**: la respuesta puede llegar en uno o en
  varios `recv`, y no hay forma de saber de antemano en cuántos. Es el loop de lectura del
  `ex05-00` de la clase.
- **`memset(buffer, 0, buffer_len)`**: deja el buffer en ceros. Así, todo lo que leamos queda
  automáticamente seguido de un `'\0'` y se puede tratar como string (lo necesita `obtener_ip`).
- **`while (1)`**: un loop infinito del que se sale con `break` (EOF) o con `return` (error).
- **El chequeo de espacio** (`total >= buffer_len - 1`): si ya llenamos el buffer dejando solo el
  lugar del `'\0'`, la respuesta no entra. Es un error y se corta, en vez de escribir fuera del
  arreglo.
- **`recv(s, buffer + total, buffer_len - 1 - total, 0)`**:
  - **`buffer + total`**: escribir **a continuación** de lo que ya teníamos (aritmética de
    punteros: la dirección del buffer corrida `total` bytes).
  - **`buffer_len - 1 - total`**: el espacio que queda, **menos uno para el `'\0'`**.
  - `recv` **bloquea** hasta que haya datos.
- **Los tres resultados posibles de `recv`:**
  - **`> 0`**: la cantidad de bytes que trajo. Se suman a `total` y se vuelve a leer.
  - **`0`**: **EOF**, el servidor cerró la conexión (mandó su FIN). Como pedimos
    `Connection: close`, eso significa que la respuesta está completa: `break` sale del loop.
  - **`-1`**: error. Se imprime, se cierra el socket y se devuelve 1.

### Líneas 84 a 86: cerrar

```c
    close(s);
    return 0;
}
```

- **`close(s)`** cierra el socket: libera el descriptor y TCP manda nuestro FIN (el servidor ya
  había mandado el suyo).
- **`return 0`**: salió bien. En `buffer` queda la respuesta completa, por ejemplo:

  ```
  HTTP/1.1 200 OK\r\n
  Content-Type: text/plain\r\n
  ...otros headers...\r\n
  \r\n
  190.225.36.143
  ```

---

## `obtener_ip(respuesta, ip)`: línea 88

No toca la red: recibe el texto de la respuesta y deja la IP en `ip`. Devuelve 0 si la encontró
y 1 si no.

### Líneas 88 a 91: las variables

```c
int obtener_ip(char *respuesta, char *ip) {
    char *body;
    int len;
    struct in_addr addr; // solo para validar la IP con inet_pton
```

- **`respuesta`**: lo que dejó `consulta_ip`. **`ip`**: dónde escribir la IP (el `main` le pasa
  un arreglo de `INET4_ADDRSTRLEN` = 16 bytes).
- **`body`**: un puntero que va a apuntar **adentro de `respuesta`**, al principio del cuerpo. No
  copia nada: solo marca una posición.
- **`len`**: cuántos caracteres mide la IP.
- **`addr`**: la IP en **binario** (4 bytes). Solo la usamos para que `inet_pton` tenga dónde
  escribir mientras valida; después no se usa.

### Líneas 93 a 98: ¿respondió 200?

```c
    // chequeamos que el servidor haya respondido 200 OK
    if (strncmp(respuesta, "HTTP/1.1 200", 12) != 0)
    {
      fprintf(stderr, "Error: respuesta HTTP inesperada:\n%s\n", respuesta);
      return 1;
    }
```

- La primera línea de toda respuesta HTTP es la **línea de estado**: versión, **código** y texto
  (`HTTP/1.1 200 OK`). El código 200 significa que salió bien.
- **`strncmp(a, b, 12)`** compara **solo los primeros 12 caracteres** (exactamente lo que mide
  `"HTTP/1.1 200"`) y devuelve 0 si son iguales. Comparar solo el principio es lo que queremos:
  el resto de la respuesta es otra cosa.
- Si no es un 200 (un 404, un 500, una página de error), se muestra la respuesta entera para ver
  qué pasó y se devuelve 1.

### Líneas 100 a 107: encontrar el cuerpo

```c
    // buscamos la linea vacia que separa los headers del body
    body = strstr(respuesta, "\r\n\r\n");
    if (body == NULL)
    {
      fprintf(stderr, "Error: la respuesta HTTP no tiene fin de headers\n");
      return 1;
    }
    body = body + 4; // salteamos el \r\n\r\n
```

- En HTTP, entre los headers y el cuerpo hay una **línea vacía**. El final del último header más
  la línea vacía forman la secuencia **`\r\n\r\n`**.
- **`strstr(texto, buscado)`** busca la primera aparición de `buscado` dentro de `texto` y
  devuelve un **puntero a donde empieza**, o `NULL` si no está. Podemos usar `strstr` porque
  `consulta_ip` dejó la respuesta terminada en `'\0'`.
- Si no hay `\r\n\r\n`, la respuesta está mal formada: error.
- **`body + 4`**: corremos el puntero 4 lugares para saltear los 4 caracteres `\r \n \r \n`.
  Ahora `body` apunta al primer carácter del cuerpo, que es el primer dígito de la IP.

### Líneas 109 a 117: recortar la IP

```c
    // la ip son los primeros caracteres del body que sean digitos o puntos
    len = strspn(body, "0123456789.");
    if (len == 0 || len >= INET4_ADDRSTRLEN)
    {
      fprintf(stderr, "Error: el body no tiene una IP: %s\n", body);
      return 1;
    }
    memcpy(ip, body, len);
    ip[len] = '\0';
```

- **`strspn(texto, permitidos)`** cuenta cuántos caracteres **seguidos, desde el principio**,
  pertenecen al conjunto `permitidos`. Con `"0123456789."`, cuenta los dígitos y puntos y se
  detiene en el primer carácter que no lo es (un salto de línea, el `'\0'`, lo que sea). Ese
  tramo es la IP. Ejemplo: en `"190.225.36.143\n"` devuelve 14.
- **`len == 0`**: el cuerpo no empieza con una IP. **`len >= 16`**: es más largo que la IPv4 más
  larga posible (15) y no entraría en `ip` con su `'\0'`. En los dos casos, error.
- **`memcpy(ip, body, len)`** copia exactamente `len` bytes del cuerpo a `ip`. Se usa `memcpy` y
  no `strcpy` porque queremos copiar **solo la IP**, no todo lo que sigue hasta el `'\0'`.
- **`ip[len] = '\0'`**: como `memcpy` no agrega el terminador, lo ponemos nosotros. Ahora `ip` es
  un string válido.

### Líneas 119 a 126: validar que sea una IPv4

```c
    // validamos que sea una IPv4 de verdad (4 números entre 0 y 255 separados por puntos)
    if (inet_pton(AF_INET, ip, &addr) != 1)
    {
      fprintf(stderr, "Error: IP invalida: %s\n", ip);
      return 1;
    }
    return 0;
}
```

- **`inet_pton`** (*presentation to network*) convierte una IP de texto a binario. Devuelve
  **1** si el texto es una IPv4 válida, 0 si no lo es y -1 si la familia es incorrecta.
- La usamos como **validación**: `strspn` solo garantiza que haya dígitos y puntos, así que un
  texto como `999.1.1.1` o `1.2.3` pasaría. `inet_pton` comprueba que sean **4 números de 0 a
  255 separados por puntos**.
- El resultado binario queda en `addr`, que no usamos. Lo que importa es que, si llegamos al
  `return 0`, en `ip` hay una IPv4 bien formada, lista para armar el pedido.
- Su inversa es `inet_ntop` (binario → texto), que usamos en `autenticacion.c`.

---

## Cómo lo usa el `main`

```c
char respuesta[3000];
char ip[INET4_ADDRSTRLEN];

if (consulta_ip(respuesta, sizeof(respuesta)) != 0)
    return 1;
if (obtener_ip(respuesta, ip) != 0)
    return 1;
printf("IP pública: %s\n", ip);
```

El `main` reserva los dos buffers, llama a las dos funciones en orden y corta si alguna falla.
Los 3000 bytes alcanzan de sobra: la respuesta de ipify mide unos 225.

## Cómo se ve en la captura

Frames 1 a 9 de `TP_D_captura2.pcapng`:

| Frames | Qué es | Qué línea del código lo produce |
|---|---|---|
| 1–3 | 3-way handshake con `104.26.12.205:80` (ipify) | `connect` |
| 4 | El `GET` (58 bytes de datos) | `send` |
| 5 | ACK de ipify (confirma los 58 bytes: `Ack=59`) | lo hace TCP solo |
| 6 | El `200 OK` con la IP (225 bytes de datos) | lo lee el `recv` |
| 7 | Nuestro ACK de la respuesta | lo hace TCP solo |
| 8 | FIN de ipify (cierra porque pedimos `Connection: close`) | hace que el siguiente `recv` devuelva 0 |
| 9 | Nuestro FIN | `close(s)` |
| 11 | El ACK final de ipify (sale después del primer datagrama UDP, el frame 10) | lo hace TCP solo |

Los 58 bytes del `GET` son: `GET / HTTP/1.1\r\n` (16) + `Host: api.ipify.org\r\n` (21) +
`Connection: close\r\n` (19) + `\r\n` (2).

## Preguntas que pueden hacer sobre este archivo

| Pregunta | Respuesta corta |
|---|---|
| ¿Por qué no leen la IP de la propia máquina? | Porque es privada; la pública la pone el NAT del router y solo la ve alguien de afuera |
| ¿Por qué `"80"` entre comillas? | `getaddrinfo` recibe el servicio como texto |
| ¿Por qué `AF_INET`? | El servidor espera una IPv4 en el pedido |
| ¿Para qué el `Host:`? | Es obligatorio en HTTP/1.1 (virtual hosting); sin él da 400 |
| ¿Para qué `Connection: close`? | Para que el server cierre y el fin de la respuesta sea el EOF (`recv` = 0) |
| ¿Por qué un loop de `recv`? | TCP es un flujo de bytes: la respuesta puede venir en varios pedazos |
| ¿Por qué `buffer_len - 1`? | Para dejar lugar al `'\0'` y poder usar `strstr` después |
| ¿Qué pasa si `recv` devuelve 0? | El servidor cerró la conexión (EOF): la respuesta está completa |
| ¿Por qué `gai_strerror` y no `strerror`? | `getaddrinfo` no usa `errno`, devuelve su propio código |
| ¿Para qué `inet_pton` si ya usaron `strspn`? | `strspn` solo mira que sean dígitos y puntos; `inet_pton` valida 4 números de 0 a 255 |
