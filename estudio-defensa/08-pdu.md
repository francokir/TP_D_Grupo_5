# `pdu.c`: framing, validación y reporte

Este es el archivo central del desafío 2. No toca la red: todo se puede probar con
`test_pdu.c`.

## Qué es una PDU en este TP

```
<TIMESTAMP>&<MESSAGE>\r\n
```

- `TIMESTAMP`: unix timestamp en dígitos ASCII, hasta 11 caracteres.
- `&`: el separador (`0x26`).
- `MESSAGE`: de 1 a 100 caracteres entre `0x20` y `0x7D`, sin `&` ni `|`.
- `\r\n` (`0x0D 0x0A`): el delimitador, el que marca dónde termina la PDU.
- En total, como mucho 122 bytes.

## El problema: el framing en TCP

TCP es un **flujo de bytes**: garantiza que llegan todos y en orden, pero **no conserva los
límites de los mensajes**. Lo que el servidor mandó como "una PDU" puede llegarnos:

- **partido** en varios `recv` (menos de una PDU por segmento);
- **junto** con otras en un solo `recv` (más de una PDU por segmento);
- en una **cantidad no entera** (una PDU y media, por ejemplo).

*Framing* es reconstruir dónde empieza y termina cada mensaje. Acá lo hacemos con el
delimitador `\r\n`: todo lo que hay hasta un `\r\n` es una PDU.

## Las constantes (`pdu.h`)

```c
#define ACUMULADOR_LEN 4096
#define PDU_OK 200
#define PDU_BAD_REQUEST 400   // error no tipificado (ej: no hay '&')
#define PDU_TS_INVALIDO 405   // timestamp con algo que no es dígito
#define PDU_TS_EXPIRADO 406   // más de 300 s de antigüedad
#define PDU_TS_LARGO 407      // más de 11 caracteres
#define PDU_MSG_INVALIDO 408  // caracteres inválidos en el mensaje
#define PDU_MSG_VACIO 409
#define PDU_MSG_LARGO 410     // más de 100 caracteres
#define TS_MAX_LEN 11
#define MSG_MAX_LEN 100
#define TS_MAX_ANTIGUEDAD 300

#define REPORTE_OK 0
#define REPORTE_NO_ENVIAR 1   // la PDU tiene '|'
#define REPORTE_ERROR -1      // no entra en el buffer
#define REPORTE_LEN (ACUMULADOR_LEN + 128)
```

Usar nombres en lugar de números sueltos hace que el código se lea solo: `return
PDU_TS_LARGO` en vez de `return 407`.

---

## El acumulador

```c
typedef struct {
    char datos[ACUMULADOR_LEN];   // los bytes recibidos que todavía no son una PDU
    int largo;                    // cuántos bytes válidos hay
} acumulador;
```

Es una "sala de espera" para los bytes: guarda lo que llegó y todavía no se pudo separar.
`largo` es clave: los datos **no** terminan en `'\0'`, así que siempre sabemos cuántos bytes
son válidos por `largo`.

### `acumulador_iniciar(a)`: línea 7

Pone `largo = 0`. No hace falta limpiar `datos`: lo que importa es `largo`.

### `acumulador_agregar(a, datos, len)`: línea 11

```c
if (a->largo + len > ACUMULADOR_LEN) return 1;     // no entra
memcpy(a->datos + a->largo, datos, len);           // lo pegamos al final
a->largo += len;
return 0;
```

Pega lo que trajo un `recv` **al final** de lo que ya había. Si no entra, devuelve 1 (el
programa lo trata como error: llegaron más de 4096 bytes sin ningún `\r\n`).

### `extraer_pdu(a, pdu)`: línea 19

Si hay una PDU completa, la saca. Devuelve su largo, o **-1** si todavía no hay ninguna
completa.

```c
for (i = 0; i + 1 < a->largo; i++) {
    if (a->datos[i] == '\r' && a->datos[i + 1] == '\n') { fin = i; break; }
}
if (fin == -1) return -1;                         // todavía no llegó el \r\n

memcpy(pdu, a->datos, fin);                       // la PDU sin el \r\n
pdu[fin] = '\0';

a->largo -= fin + 2;                              // sacamos la PDU y su \r\n
memmove(a->datos, a->datos + fin + 2, a->largo);  // corremos el resto al principio
return fin;
```

1. **Busca el `\r` seguido de `\n`**, byte por byte, solo dentro de los `largo` bytes válidos.
   La condición `i + 1 < a->largo` evita leer fuera cuando el `\r` es el último byte.
2. Si no lo encuentra, la PDU está incompleta: devuelve -1 y los bytes **quedan en el
   acumulador** esperando el próximo `recv`.
3. Si lo encuentra en la posición `fin`, copia los `fin` bytes anteriores a `pdu` (sin el
   delimitador) y le agrega `'\0'` para poder usarla como string.
4. Saca esos `fin + 2` bytes del acumulador y **corre lo que sobra al principio**.

**¿Por qué no `strstr(a->datos, "\r\n")`?** `strstr` busca hasta encontrar un `'\0'`, y los
bytes de `recv` no lo tienen: se pasaría de los bytes válidos y leería basura.

**¿Por qué `memmove` y no `memcpy`?** Porque el origen (`datos + fin + 2`) y el destino
(`datos`) están en el **mismo arreglo** y se pueden superponer. `memcpy` no garantiza un
resultado correcto en ese caso; `memmove` sí.

**Un `\n` solo no es delimitador** (hay un test para eso): solo cuenta el par `\r\n`.

### Los tres casos de la consigna con este código

| Cómo llegan los bytes | Qué pasa |
|---|---|
| `"…&Quere"`, `"s un cuarto mens"`, `"aje\r\n"` (una PDU en 3 `recv`) | Los dos primeros `extraer_pdu` devuelven -1; al llegar el tercero aparece el `\r\n` y sale la PDU completa |
| `"…&Second Message\r\n…&3er Hello\r\n"` (dos en un `recv`) | El `while` de `procesar_pdus` llama dos veces a `extraer_pdu` y salen las dos |
| `"…&Hola\r\n…&Ch"` y después `"au\r\n"` (una y media) | Sale `Hola`; `…&Ch` queda guardado y se completa con el siguiente `recv` |
| `"…&Hola\r"` y después `"\n"` (el delimitador partido) | En el primero, el `\r` es el último byte y el loop no lo mira; en el segundo aparece el par |

---

## `validar_pdu(pdu, len, ahora)`: línea 44

Recibe la PDU **sin** el `\r\n` y devuelve su código. Si hay varios errores, devuelve **el
primero que encuentra**. `ahora` se recibe como parámetro (en vez de llamar a `time(NULL)`
adentro) para poder probarla con horas fijas en los tests.

```c
// 1. Estructura: buscar el '&'
for (i = 0; i < len; i++) if (pdu[i] == '&') { amp = i; break; }
if (amp == -1) return PDU_BAD_REQUEST;          // no hay 2 campos

ts_len  = amp;                                  // el timestamp va de 0 al '&'
msg     = pdu + amp + 1;                        // el mensaje empieza después
msg_len = len - amp - 1;

// 2. Timestamp
if (ts_len == 0) return PDU_BAD_REQUEST;        // vacío: no tipificado
for (i = 0; i < ts_len; i++)
    if (pdu[i] < '0' || pdu[i] > '9') return PDU_TS_INVALIDO;   // 405
if (ts_len > TS_MAX_LEN) return PDU_TS_LARGO;                   // 407
ts = strtol(pdu, NULL, 10);                     // todos dígitos: corta en el '&'
if (ahora - ts > TS_MAX_ANTIGUEDAD) return PDU_TS_EXPIRADO;     // 406

// 3. Mensaje
if (msg_len == 0) return PDU_MSG_VACIO;                         // 409
for (i = 0; i < msg_len; i++) {
    c = (unsigned char) msg[i];
    if (c < 0x20 || c > 0x7D || c == '&' || c == '|') return PDU_MSG_INVALIDO;  // 408
}
if (msg_len > MSG_MAX_LEN) return PDU_MSG_LARGO;                // 410
return PDU_OK;
```

| Orden | Chequeo | Código | Ejemplo |
|---|---|---|---|
| 1 | No hay `&`, o el timestamp está vacío | 400 | `1792303544^Temperatura…`, `&Hola` |
| 2 | El timestamp tiene algo que no es dígito | 405 | `12345.1&…`, `17x0000000&…` |
| 3 | El timestamp tiene más de 11 caracteres | 407 | `123456789012&…` (12 dígitos) |
| 4 | Pasaron más de 300 s desde el timestamp | 406 | `Timestamp de ayer` |
| 5 | El mensaje está vacío | 409 | `1794663170&` |
| 6 | Hay un carácter fuera de `0x20`–`0x7D`, o `&`, o `\|` | 408 | `Mensaje con ~ invalido` |
| 7 | El mensaje tiene más de 100 caracteres | 410 | 101 letras `A` |
| — | Nada de lo anterior | 200 | `Primer mensaje` |

Detalles para saber explicar:

- **Busca el primer `&`.** Si el mensaje tiene otro `&`, ese queda dentro del mensaje y da 408.
- **El orden entre 405 y 407:** primero se chequea que sean dígitos y después el largo. Por eso
  `1111762303544` (13 dígitos) da 407 y `12345.1` da 405.
- **Recién después de saber que son dígitos y que son ≤ 11** se convierte con `strtol`. Con 11
  dígitos el número entra en un `long` de 64 bits (Linux).
- **`ahora - ts > 300`:** con 300 segundos exactos es válido; con 301, 406. Un timestamp **del
  futuro** da una resta negativa, así que es 200 (la consigna solo rechaza los viejos).
- **El cast a `unsigned char`:** un `char` puede ser con signo, y los bytes ≥ `0x80` (por
  ejemplo, los de `⁰` en UTF-8) serían negativos. Sin el cast, `c < 0x20` daría verdadero por
  casualidad; con el cast, la comparación es la correcta.
- **`}` es `0x7D` (válido) y `~` es `0x7E` (inválido).** El tab (`0x09`) también es inválido.

---

## `armar_reporte(usuario, pdu, codigo, secreto, reporte, reporte_len)`: línea 93

```c
if (strchr(pdu, '|') != NULL) return REPORTE_NO_ENVIAR;
n = snprintf(a_firmar, sizeof(a_firmar), "%s|%s|%d|%s", usuario, pdu, codigo, secreto);
md5_hex(a_firmar, n, firma);
n = snprintf(reporte, reporte_len, "%s|%s|%d|%s", usuario, pdu, codigo, firma);
return REPORTE_OK;
```

1. **Si la PDU tiene `|`, no se reenvía.** El reporte usa `|` para separar sus 4 campos: una
   PDU con `|` adentro haría que el servidor viera 5 campos. La consigna lo pide así ("no
   haremos byte stuffing para simplificar").
2. Firma `usuario|PDU|código|secreto` y arma `usuario|PDU|código|firma`. El código se escribe
   con `%d` (es un `int`).
3. La PDU va **sin el `\r\n`** (ya se lo sacó `extraer_pdu`). Lo confirmamos: en la web de la
   cátedra los 9 reportes aparecen en verde.

Ejemplo real de la captura (frame 18):
`grupo05|1794669697&Primer mensaje|200|bf8b30caa3dd3b5d5a11c69c6c9b955a`.
