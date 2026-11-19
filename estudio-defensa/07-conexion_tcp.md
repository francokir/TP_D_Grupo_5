# `conexion_tcp.c`: la conexión TCP y el loop del desafío 2

## Qué resuelve

Se conecta al puerto que nos dio el servidor y maneja el **loop principal del desafío 2**:
recibir bytes, separar las PDUs, validarlas y reportarlas. La lógica de cada PDU (framing,
validación, armado del reporte) está en `pdu.c`. Este archivo hace la parte de **red** y
coordina.

---

## `conectar_tcp(ip, puerto)`: línea 14

Devuelve el socket conectado, o -1 si hubo error.

```c
char puerto_str[6];                                  // "65535" + '\0'
snprintf(puerto_str, sizeof(puerto_str), "%d", puerto);
hints.ai_family = AF_INET;
hints.ai_socktype = SOCK_STREAM;                     // TCP
getaddrinfo(ip, puerto_str, &hints, &servinfo);
s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
switch (connect(s, servinfo->ai_addr, servinfo->ai_addrlen)) {
    case -1: /* error: mostrar errno, liberar, cerrar, devolver -1 */
    case 0:  break;
}
freeaddrinfo(servinfo);
return s;
```

- El puerto llega como número, pero `getaddrinfo` lo pide como texto: por eso el `snprintf`.
  6 caracteres alcanzan para el más largo (`"65535"`) más el `'\0'`.
- `connect` hace el 3-way handshake (frames 13 a 15 de la captura).
- **Hay que llamarla enseguida:** el servidor deja el puerto abierto solo 10 segundos. En la
  captura el SYN sale 1,2 ms después de recibir el 200.
- Si `connect` falla (por ejemplo, llegamos tarde y el puerto ya se cerró), muestra el error
  con `strerror(errno)` y libera todo antes de salir.

---

## `procesar_pdus(s_tcp, s_udp, ip_servidor, usuario, secreto)`: línea 56

Devuelve la cantidad de reportes enviados, o -1 si hubo error.

```c
char buffer[1024];            // lo que trae cada recv
acumulador acum;              // bytes que todavía no formaron una PDU
char pdu[ACUMULADOR_LEN];
char reporte[REPORTE_LEN];

acumulador_iniciar(&acum);
while (1) {
    inbytes = recv(s_tcp, buffer, sizeof(buffer), 0);
    if (inbytes == 0) break;                         // EOF: el servidor cerró
    if (inbytes == -1) { /* error */ return -1; }

    if (acumulador_agregar(&acum, buffer, inbytes) != 0) {
        /* llegaron demasiados bytes sin delimitador */
        return -1;
    }

    while ((len = extraer_pdu(&acum, pdu)) != -1) {  // TODAS las PDUs completas
        codigo = validar_pdu(pdu, len, (long) time(NULL));
        resultado = armar_reporte(usuario, pdu, codigo, secreto, reporte, sizeof(reporte));
        if (resultado == REPORTE_NO_ENVIAR) { printf(...); continue; }   // tiene '|'
        if (resultado == REPORTE_ERROR)     { fprintf(stderr, ...); continue; }
        if (enviar_al_servidor(s_udp, ip_servidor, reporte) != 0) return -1;
        printf("[%d] %s\n", codigo, reporte);
        enviados++;
    }
}
if (acum.largo > 0)
    printf("Aviso: quedaron %d bytes sin delimitador al cerrar la conexion\n", acum.largo);
return enviados;
```

### Los dos loops, explicados

**El loop de afuera** (`while (1)` con `recv`) lee bytes de la conexión hasta que el servidor
la cierra. Cada `recv` trae **lo que haya llegado**, que no necesariamente coincide con una
PDU: puede ser media PDU, una, varias o una y media.

Acá `recv` usa `sizeof(buffer)` entero, sin reservar lugar para un `'\0'`. Está bien, porque
`buffer` **no se trata como string**: se le pasan los bytes al acumulador junto con
`inbytes`, que dice cuántos son válidos.

**El loop de adentro** (`while` con `extraer_pdu`) saca del acumulador **todas** las PDUs
completas que haya después de ese `recv`. Si un `recv` trajo dos PDUs, este loop da dos
vueltas; si trajo media, no da ninguna y los bytes quedan esperando al siguiente `recv`.

### Por cada PDU completa

1. **Validar:** `validar_pdu` devuelve el código (200, 400, 405–410). Le pasamos la hora
   actual con `time(NULL)` (segundos desde 1970, el *unix timestamp*) para el chequeo de los
   300 segundos.
2. **Armar el reporte:** `armar_reporte` arma `usuario|PDU|código|firma`.
   - Si la PDU tiene `|`, devuelve `REPORTE_NO_ENVIAR`: la mostramos en pantalla y
     **seguimos con la siguiente** (`continue`), sin mandar nada.
   - Si el reporte no entra en el buffer, `REPORTE_ERROR`: lo informamos y seguimos.
3. **Mandarlo** por UDP con `enviar_al_servidor`, por el **mismo socket UDP** `s_udp` de la
   autenticación. Si el `sendto` falla, cortamos todo con -1.
4. Lo mostramos en pantalla y contamos uno más.

### Cómo termina

Cuando el servidor termina de mandar PDUs, cierra la conexión (manda FIN). Nuestro `recv`
devuelve **0** y salimos del loop. Si en el acumulador quedaron bytes que nunca recibieron su
`\r\n`, no forman una PDU: lo avisamos y no los reportamos.

El socket TCP lo cierra el `main` con `close(s_tcp)` después de que esta función vuelve.

### Por qué la red y la lógica están separadas

`procesar_pdus` es lo único del desafío 2 que toca sockets. El acumulador, la validación y el
reporte (`pdu.c`) reciben bytes y devuelven resultados, **sin red**. Por eso `test_pdu.c` puede
simular `recv` partidos de cualquier forma (incluso byte por byte) sin conectarse a nada.
