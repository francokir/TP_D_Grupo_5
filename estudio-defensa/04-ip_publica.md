# `ip_publica.c`: la IP pública por HTTP escrito a mano

## Para qué existe

El servidor de la cátedra valida que la IP que mandamos en el pedido sea la **pública**, o sea
la dirección con la que salimos a Internet. Nuestra PC tiene una IP privada
(`192.168.1.25`), que el router traduce por NAT a la pública (`190.225.36.143`). Desde adentro
no la podemos ver, así que se la preguntamos a un servicio externo: `api.ipify.org`, que lee la
dirección de origen del paquete (ya traducida) y la devuelve en el cuerpo de la respuesta.

La consigna prohíbe libcurl y cualquier biblioteca HTTP, así que **el pedido HTTP lo escribimos
nosotros como texto** y lo mandamos por un socket TCP.

## El header (`ip_publica.h`)

```c
#define INET4_ADDRSTRLEN 16   // "255.255.255.255" son 15 caracteres + '\0'

int consulta_ip(char *buffer, int buffer_len);   // llena el buffer con la respuesta HTTP
int obtener_ip(char *respuesta, char *ip);       // busca la IP dentro de respuesta
```

Lo dividimos en dos funciones a propósito: `consulta_ip` hace la parte de **red**, y
`obtener_ip` hace la parte de **texto**, que se puede probar sin conectarse a nada.

## `consulta_ip(buffer, buffer_len)`: línea 15

Devuelve 0 si salió bien y 1 si hubo error. Paso a paso:

### 1. Resolver el nombre: `getaddrinfo`

```c
memset(&hints, 0, sizeof(hints));   // todo en cero: "sin preferencias" salvo lo que pongamos
hints.ai_family = AF_INET;          // IPv4
hints.ai_socktype = SOCK_STREAM;    // TCP
status = getaddrinfo("api.ipify.org", "80", &hints, &servinfo);
```

- `getaddrinfo` hace la consulta DNS y devuelve en `servinfo` una lista de direcciones **ya
  armadas** (`ai_addr`, `ai_addrlen`), listas para pasarle a `connect`. Usamos la primera.
- El puerto va como **texto** (`"80"`), porque el segundo parámetro es el "servicio". Al
  principio pusimos `80` sin comillas y fue un error.
- Pedimos IPv4 (`AF_INET`) porque el servidor de la cátedra espera una IPv4 en el pedido. Si
  ipify contestara por IPv6, nos devolvería una IPv6.
- Si falla, el error se muestra con `gai_strerror(status)`, porque `getaddrinfo` **no** usa
  `errno`: devuelve su propio código de error.

### 2. Crear el socket y conectar

```c
s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
switch (connect(s, servinfo->ai_addr, servinfo->ai_addrlen)) {
    case -1: /* error: mostrar errno, cerrar y salir */
    case 0:  break;      /* conectado */
}
freeaddrinfo(servinfo); // la lista ya no hace falta
```

- `socket` crea el descriptor (un número, como un archivo abierto).
- `connect` hace el **3-way handshake** (SYN, SYN-ACK, ACK). Cuando devuelve 0, la conexión
  está establecida. El `switch` con `case -1` / `case 0` es el estilo de los ejemplos de clase.

### 3. Mandar el pedido HTTP

```c
char *request = "GET / HTTP/1.1\r\n"
                "Host: api.ipify.org\r\n"
                "Connection: close\r\n"
                "\r\n";
send(s, request, strlen(request), 0);
```

- **`GET / HTTP/1.1`**: la línea de pedido (método, recurso, versión).
- **`Host:`** es obligatorio en HTTP/1.1. Le dice al servidor qué sitio queremos, porque en una
  misma IP puede haber varios (virtual hosting). Sin él la respuesta es `400 Bad Request`.
- **`Connection: close`** le pide al servidor que cierre la conexión al terminar. Así sabemos
  que la respuesta está completa cuando `recv` devuelve 0, y no hace falta interpretar el
  `Content-Length`.
- La **línea vacía** (`\r\n` solo) marca el fin de los headers. Cada línea termina en `\r\n`.

### 4. Leer toda la respuesta: loop de `recv` hasta EOF

```c
memset(buffer, 0, buffer_len);
while (1) {
    if (total >= buffer_len - 1) { /* no entra: error */ }
    inbytes = recv(s, buffer + total, buffer_len - 1 - total, 0);
    if (inbytes == 0) break;          // EOF: el servidor cerró
    if (inbytes == -1) { /* error */ }
    total += inbytes;                 // acumulamos
}
close(s);
```

- TCP es un flujo de bytes: la respuesta puede llegar en uno o varios `recv`. Por eso hacemos
  un loop y **acumulamos**: cada `recv` escribe en `buffer + total`, a continuación de lo que
  ya teníamos.
- Los tres valores posibles de `recv`: **> 0** = cantidad de bytes leídos; **0** = el otro
  extremo cerró (EOF); **-1** = error, con el detalle en `errno`.
- `buffer_len - 1 - total` es el espacio que queda, **dejando un byte libre para el `'\0'`**.
  Como el buffer arranca en ceros (`memset`), lo leído queda terminado en `'\0'` y se puede
  tratar como string.

## `obtener_ip(respuesta, ip)`: línea 88

No toca la red: recibe el texto de la respuesta y deja la IP en `ip`.

```c
if (strncmp(respuesta, "HTTP/1.1 200", 12) != 0) { /* respuesta inesperada */ }
body = strstr(respuesta, "\r\n\r\n");   // fin de los headers
body = body + 4;                        // salteamos el \r\n\r\n
len = strspn(body, "0123456789.");      // cuántos caracteres seguidos son dígitos o puntos
if (len == 0 || len >= INET4_ADDRSTRLEN) { /* no hay IP */ }
memcpy(ip, body, len);
ip[len] = '\0';
if (inet_pton(AF_INET, ip, &addr) != 1) { /* IP inválida */ }
```

1. Chequea que la primera línea diga `HTTP/1.1 200`. Si no, algo salió mal.
2. Busca la **línea vacía** `\r\n\r\n` que separa los headers del cuerpo, y se para justo
   después.
3. `strspn` cuenta cuántos caracteres del principio del cuerpo son dígitos o puntos. Esa es la
   IP. Si son 0 o más de 15, no es una IP.
4. La copia con `memcpy` y le agrega el `'\0'`.
5. `inet_pton` la valida de verdad: comprueba que sean 4 números entre 0 y 255 separados por
   puntos. Un `999.1.1.1` pasa el `strspn`, pero no el `inet_pton`.

Acá sí podemos usar `strstr`, porque `consulta_ip` dejó la respuesta terminada en `'\0'`.

## Cómo se ve en la captura

Frames 1 a 9 de `TP_D_captura2.pcapng`: 3-way handshake con `104.26.12.205:80` (ipify), el
`GET` (58 bytes de datos), el `200 OK` con la IP (225 bytes) y el cierre con FIN en los dos
sentidos.
