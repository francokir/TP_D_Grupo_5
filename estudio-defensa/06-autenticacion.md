# `autenticacion.c`: el desafío 1 por UDP

## Qué resuelve

Todo lo que pasa por UDP: armar el pedido firmado, mandarlo al puerto 20262, esperar la
respuesta como mucho 10 segundos, verificar que sea auténtica y sacar el puerto TCP. También
tiene `enviar_al_servidor`, que se usa en el desafío 2 para mandar los reportes.

## Constantes (`autenticacion.h`)

```c
#define PEDIDO_LEN 256              // tamaño de los buffers de pedido y respuesta
#define RESPUESTA_TIMEOUT_MS 10000  // 10 segundos
#define RESPUESTA_OK 0              // 200 con firma válida
#define RESPUESTA_ERROR 1           // 400 o 401 del servidor
#define RESPUESTA_INVALIDA 2        // cualquier otra cosa: se descarta en silencio
```

En el `.c` está `#define SERVIDOR_PUERTO_UDP "20262"`, también como texto para `getaddrinfo`.

---

## `armar_pedido(usuario, ip, secreto, pedido, pedido_len)`: línea 19

Arma el pedido `<USUARIO>|<IP>|<FIRMA>`.

```c
n = snprintf(a_firmar, sizeof(a_firmar), "%s|%s|%s", usuario, ip, secreto);
md5_hex(a_firmar, n, firma);
n = snprintf(pedido, pedido_len, "%s|%s|%s", usuario, ip, firma);
```

1. Arma el texto a firmar: `grupo05|190.225.36.143|<secreto>`.
2. Le calcula el MD5 → 32 caracteres hexa.
3. Arma el pedido **sin el secreto**: `grupo05|190.225.36.143|1baf07b1…`.

**Por qué funciona como autenticación:** el servidor también conoce nuestro secreto.
Recalcula `md5(grupo05|IP|secreto)` y compara con la firma que mandamos. Si alguien no conoce
el secreto, no puede fabricar una firma válida. Y el secreto nunca viaja por la red.

**`snprintf` y su chequeo:** `snprintf` nunca escribe más allá del tamaño indicado y devuelve
cuántos caracteres *habría* escrito. Si `n >= tamaño`, el texto no entró (quedó cortado) y lo
tratamos como error. Si `n < 0`, hubo un error de formato.

---

## `enviar_pedido(ip_servidor, pedido)`: línea 41

Crea el socket UDP, manda el pedido y **devuelve el socket abierto** (o -1 si hubo error).

```c
hints.ai_family = AF_INET;
hints.ai_socktype = SOCK_DGRAM;                      // UDP
getaddrinfo(ip_servidor, "20262", &hints, &servinfo);
s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
enviados = sendto(s, pedido, strlen(pedido), 0, servinfo->ai_addr, servinfo->ai_addrlen);
freeaddrinfo(servinfo);
return s;
```

- `SOCK_DGRAM` = UDP: no hay conexión ni handshake. Cada `sendto` es un datagrama
  independiente y lleva la dirección de destino como parámetro.
- No hacemos `bind`, así que el sistema operativo le asigna al socket un **puerto efímero**
  en el primer `sendto` (en la captura fue el 56872).
- **¿Por qué devolver el socket en vez de cerrarlo?** Porque el servidor contesta al puerto
  de origen del pedido. Si cerrábamos el socket y abríamos otro, el nuevo tendría otro puerto
  y la respuesta no le llegaría. Además, el NAT del router creó una entrada para ese puerto
  cuando salió el pedido: es lo que deja pasar la respuesta.

---

## `esperar_respuesta(s, timeout_ms, respuesta, respuesta_len, ip_origen)`: línea 79

Espera **un** datagrama con un tiempo máximo. Devuelve 0 si llegó, 1 si se venció el tiempo y
-1 si hubo error.

### Por qué hace falta `select`

UDP no garantiza nada: el pedido o la respuesta se pueden perder. Si llamáramos a `recvfrom`
directamente, se quedaría bloqueado para siempre esperando algo que nunca va a llegar.
`select` espera hasta que pase una de dos cosas: que el socket tenga algo para leer, o que se
venza el tiempo.

```c
tv.tv_sec  = timeout_ms / 1000;           // segundos
tv.tv_usec = (timeout_ms % 1000) * 1000;  // microsegundos
FD_ZERO(&rfds);                           // conjunto vacío
FD_SET(s, &rfds);                         // agregamos nuestro socket
retval = select(s + 1, &rfds, NULL, NULL, &tv);
if (retval == -1) /* error */;
if (retval == 0)  return 1;               // timeout: no llegó nada
```

- `fd_set` es un **conjunto de descriptores** que se quieren vigilar. `FD_ZERO` lo vacía y
  `FD_SET` agrega el socket.
- El primer argumento de `select` es **el descriptor más alto + 1**.
- Los tres conjuntos son: para leer, para escribir y para excepciones. Solo nos interesa leer.
- `select` devuelve cuántos descriptores están listos: 0 = timeout, -1 = error.

### Leer el datagrama

```c
addrsize = sizeof(their_addr);            // ¡inicializarlo antes!
memset(respuesta, 0, respuesta_len);
inbytes = recvfrom(s, respuesta, respuesta_len - 1, 0,
                   (struct sockaddr *) &their_addr, &addrsize);
inet_ntop(AF_INET, &(their_addr.sin_addr), ip_origen, INET4_ADDRSTRLEN);
```

- `recvfrom` lee el datagrama **entero** (en UDP no hay mensajes "partidos") y además llena
  `their_addr` con **quién lo mandó**.
- `addrsize` es de entrada y salida: entra el tamaño del espacio disponible y sale el tamaño
  real. Si no se inicializa, el comportamiento es indefinido (en los ejemplos de clase a veces
  no estaba inicializado; nosotros lo corregimos).
- `respuesta_len - 1` deja lugar para el `'\0'`.
- `inet_ntop` pasa la IP de binario a texto (`"167.114.129.206"`) para compararla después.

---

## `separar_campos(mensaje, campos)`: línea 121

Corta `"A|B|C|D"` en 4 campos. Devuelve 0 si hay **exactamente** 4 y 1 si hay más o menos.

```c
for (i = 0; i < 3; i++) {
    campos[i] = mensaje;              // el campo empieza acá
    pipe = strchr(mensaje, '|');      // buscamos el próximo '|'
    if (pipe == NULL) return 1;       // faltan campos
    *pipe = '\0';                     // cortamos el campo ahí
    mensaje = pipe + 1;               // el siguiente empieza después
}
campos[3] = mensaje;                  // el último es lo que queda
if (strchr(mensaje, '|') != NULL) return 1;  // sobran campos
```

- **Modifica el mensaje:** reemplaza cada `|` por `'\0'`, así cada `campos[i]` queda como un
  string independiente que apunta adentro del mismo buffer. No copia nada.
- **¿Por qué no `strtok`?** `strtok` saltea separadores seguidos (`"200||17189|abc"` le daría
  3 campos en vez de 4 con uno vacío) y guarda estado global. Con `strchr` controlamos
  exactamente cuántos campos hay.

Ejemplo: `"200|Port|30029|4238…"` → `campos = {"200", "Port", "30029", "4238…"}`.

---

## `verificar_respuesta(respuesta, secreto, puerto)`: línea 142

Clasifica una respuesta en `RESPUESTA_OK`, `RESPUESTA_ERROR` o `RESPUESTA_INVALIDA`.

```c
strcpy(copia, respuesta);                       // trabajamos sobre una copia
if (separar_campos(copia, campos) != 0) return RESPUESTA_INVALIDA;

if (strcmp(campos[0], "400") == 0 || strcmp(campos[0], "401") == 0) {
    fprintf(stderr, "Error del servidor %s: %s\n", campos[0], campos[2]);
    return RESPUESTA_ERROR;
}
if (strcmp(campos[0], "200") != 0 || strcmp(campos[1], "Port") != 0)
    return RESPUESTA_INVALIDA;

snprintf(a_firmar, …, "%s|%s|%s|%s", campos[0], campos[1], campos[2], secreto);
md5_hex(a_firmar, n, firma);
if (strcmp(firma, campos[3]) != 0) return RESPUESTA_INVALIDA;   // firma mal: descartar

numero = strtol(campos[2], &fin, 10);
if (fin == campos[2] || *fin != '\0' || numero < 1 || numero > 65535)
    return RESPUESTA_INVALIDA;
*puerto = (int) numero;
return RESPUESTA_OK;
```

1. **Copia:** `separar_campos` modifica el texto, y no queremos romper el original. Antes
   chequea que la respuesta entre en la copia.
2. **400 / 401:** el servidor rechazó el pedido (formato o autenticación). Mostramos el texto
   del error y cortamos. **No se verifica la firma**: vienen con 32 ceros, porque si el
   servidor no nos reconoció no puede saber qué secreto usar.
3. Lo único que aceptamos además es `200|Port|…`.
4. **Verificar la firma:** recalculamos `md5(200|Port|<puerto>|<secreto>)` y la comparamos con
   el cuarto campo. Así sabemos que la respuesta la mandó alguien que conoce el secreto (el
   servidor) y que nadie cambió el puerto en el camino. Si no coincide, **se descarta en
   silencio**, como pide la consigna.
5. **Validar el puerto:** `strtol` convierte el texto a número y deja `fin` apuntando al
   primer carácter que no pudo convertir. `fin == campos[2]` → no había ningún número;
   `*fin != '\0'` → había basura después (`"30029x"`). Además tiene que estar entre 1 y 65535,
   el rango de puertos (16 bits).

---

## `recibir_puerto(s, ip_servidor, secreto, puerto)`: línea 185

El loop del desafío 1. Devuelve 0 con el puerto en `*puerto`, o 1 si no hubo una respuesta
válida en 10 segundos.

### El problema que resuelve

La consigna pide dos cosas que se cruzan:

- esperar la respuesta **10 segundos**;
- **descartar en silencio** las respuestas con firma inválida.

Si llega un datagrama inválido a los 3 segundos, lo descartamos y hay que seguir esperando,
pero solo los **7 segundos que quedan**, no 10 de nuevo.

```c
gettimeofday(&inicio, NULL);                          // anotamos cuándo empezamos
while (1) {
    gettimeofday(&ahora, NULL);
    transcurrido_ms = (ahora.tv_sec - inicio.tv_sec) * 1000
                    + (ahora.tv_usec - inicio.tv_usec) / 1000;
    restante_ms = RESPUESTA_TIMEOUT_MS - transcurrido_ms;
    if (restante_ms <= 0) break;                      // se acabó el tiempo

    resultado = esperar_respuesta(s, restante_ms, respuesta, sizeof(respuesta), ip_origen);
    if (resultado == -1) return 1;                    // error
    if (resultado == 1)  break;                       // timeout

    if (strcmp(ip_origen, ip_servidor) != 0) continue;   // no viene del servidor

    resultado = verificar_respuesta(respuesta, secreto, puerto);
    if (resultado == RESPUESTA_OK)    return 0;
    if (resultado == RESPUESTA_ERROR) return 1;
    /* RESPUESTA_INVALIDA: se ignora y se sigue esperando lo que queda */
}
fprintf(stderr, "Error: no hubo respuesta valida en 10 segundos\n");
return 1;
```

- `gettimeofday` da la hora con precisión de microsegundos (`tv_sec` + `tv_usec`). La resta
  da cuántos milisegundos pasaron.
- **Filtro por IP de origen:** si el datagrama no viene del servidor, se ignora. Ojo: compara
  textos, así que funciona porque al programa le pasamos la IP del servidor (no un nombre).

---

## `enviar_al_servidor(s, ip_servidor, mensaje)`: línea 227

La usa el desafío 2: manda un reporte al puerto 20262 **por el socket que ya existe**. Hace
`getaddrinfo` para armar la dirección de destino y un `sendto`. No crea ni cierra sockets.

## Cómo se ve en la captura

- **Frame 10:** `192.168.1.25:56872 → 167.114.129.206:20262`, 55 bytes de datos:
  `grupo05|190.225.36.143|1baf07b1…`.
- **Frame 12:** la respuesta vuelve a **56872**, 47 bytes: `200|Port|30029|4238aec9…`. Pasaron
  180 ms.
