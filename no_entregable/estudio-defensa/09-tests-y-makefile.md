# Cómo compilamos y qué prueba cada test

## El Makefile

```make
CC     = gcc
CFLAGS = -Wall -Wextra
```

| Comando | Qué hace |
|---|---|
| `make` | Compila `cliente` y los tres tests |
| `make test` | Corre `./test_md5`, `./test_autenticacion` y `./test_pdu` |
| `make run ARGS="<usuario> <secreto> <ip>"` | Compila y corre el cliente |
| `make clean` | Borra los ejecutables |

- **`-Wall -Wextra`:** la consigna exige que no haya warnings con `-Wall`; nosotros agregamos
  `-Wextra`, que avisa todavía más cosas. Compila sin ningún warning.
- Cada objetivo lista como dependencias los `.c` y los `.h` que usa: si cambia cualquiera,
  `make` lo recompila.
- **Cada test compila solo lo que necesita.** Por ejemplo, `test_pdu` se arma con
  `test_pdu.c + pdu.c + md5.c`, sin ningún archivo que toque la red.
- Las órdenes de un Makefile tienen que empezar con **TAB**, no con espacios. Nos dio
  `missing separator` porque el editor había puesto espacios.

## Por qué hicimos tests sin red

- No queríamos depender del servidor de la cátedra para cada prueba.
- Podemos probar casos que el servidor **nunca** manda: el `\r` en un `recv` y el `\n` en el
  siguiente, todo byte por byte, un timestamp vacío, uno del futuro, firmas alteradas.
- Para que sea posible, la lógica está separada de la red: `validar_pdu` recibe la hora como
  parámetro, `extraer_pdu` recibe bytes, `verificar_respuesta` recibe un texto.

## `test_md5` (6 pruebas)

Compara `md5_hex` con resultados conocidos: los 4 vectores del RFC 1321 (texto vacío, `abc`,
80 dígitos y 62 caracteres, que obliga a usar dos bloques de cola) y los 2 ejemplos del
enunciado (`grupo01|180.11.1.3|secreto01` y `200|Port|17189|secreto01`).

## `test_autenticacion` (15 pruebas)

- **`separar_campos`:** 3 mensajes válidos (incluido uno con un campo vacío, `200||17189|abc`)
  y 3 inválidos (`hola`, 3 campos, 5 campos).
- **`verificar_respuesta`:**
  - 200 con firma válida (el ejemplo del enunciado) → OK con puerto 17189.
  - Firma calculada con otro secreto, firma en ceros y **puerto alterado** (17190 con la firma
    del 17189) → INVALIDA. Este último muestra para qué sirve la firma: nadie puede cambiar el
    puerto en el camino.
  - Firma válida pero puerto `abc` o `70000` → INVALIDA.
  - `400` y `401` → ERROR (se muestran los mensajes, por eso aparecen en la salida del test).
  - `hola` → INVALIDA.

## `test_pdu` (38 pruebas)

**Framing (8):** simula `recv` sucesivos con una lista de pedazos y comprueba qué PDUs salen.

| Prueba | Pedazos |
|---|---|
| Una PDU entera | `"…&Primer mensaje\r\n"` |
| Dos PDUs en un `recv` | `"…&Second Message\r\n…&3er Hello\r\n"` |
| Una PDU en tres `recv` | `"…&Quere"`, `"s un cuarto mens"`, `"aje\r\n"` |
| Una PDU y media | `"…&Hola\r\n…&Ch"`, `"au\r\n"` |
| `\r\n` partido | `"…&Hola\r"`, `"\n"` |
| Mensaje vacío | `"1794663170&\r\n"` |
| Un `\n` solo no delimita | `"17946&a\nb\r\n"` → una sola PDU `17946&a\nb` |
| Byte por byte | `"1"`, `"&"`, `"a"`, `"\r"`, `"\n"`, `"2"`, … |

Al final también comprueba que no hayan sobrado bytes en el acumulador.

**Validación (26):** los 7 ejemplos del enunciado, las 10 PDUs que manda el servidor de la
cátedra y 9 casos borde: 100 caracteres (200) y 101 (410), 300 s (200) y 301 s (406), un
timestamp del futuro (200), 11 dígitos con un cero adelante (200), timestamp vacío (400), `}`
(200), `&` en el mensaje (408) y un tab (408).

**Reportes (4):** tres reportes comparados con firmas calculadas aparte con `md5sum`, y la PDU
con `|`, que tiene que dar "no se envía".

## Cómo correrlo

```sh
make && make test
```

Resultado: `6 de 6`, `15 de 15` y `38 de 38` pruebas pasaron.
