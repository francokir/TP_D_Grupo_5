# Cómo compilamos y qué prueba cada test

## El Makefile, línea por línea

`make` lee el `Makefile` y compila solo lo que cambió. Cada **regla** tiene la forma:

```make
objetivo: dependencias
	orden
```

Se lee: "para tener el *objetivo*, hacen falta las *dependencias*; si alguna es más nueva que el
objetivo (o el objetivo no existe), ejecutar la *orden*".

### Líneas 1 y 2: las variables

```make
CC     = gcc
CFLAGS = -Wall -Wextra
```

- **`CC`**: el compilador. Después se usa como `$(CC)`, que se reemplaza por `gcc`.
- **`CFLAGS`**: las opciones del compilador, usadas como `$(CFLAGS)`.
  - **`-Wall`**: activa los warnings más comunes. La consigna exige compilar **sin warnings con
    `-Wall`**.
  - **`-Wextra`**: activa todavía más (por ejemplo, comparar un `int` con un `sizeof`, que es sin
    signo: por eso los casts `(int) sizeof(...)` del código). Compila sin ningún warning.

### Línea 4: `all`

```make
all: cliente test_md5 test_autenticacion test_pdu
```

- La **primera regla** es la que se ejecuta con `make` a secas. `all` no tiene orden: solo
  depende de los cuatro ejecutables, así que `make` los compila todos.

### Líneas 6 y 7: el cliente

```make
cliente: src/cliente.c src/ip_publica.c src/ip_publica.h src/autenticacion.c src/autenticacion.h src/md5.c src/md5.h src/conexion_tcp.c src/conexion_tcp.h src/pdu.c src/pdu.h
	$(CC) $(CFLAGS) -o cliente src/cliente.c src/ip_publica.c src/autenticacion.c src/md5.c src/conexion_tcp.c src/pdu.c
```

- **Dependencias**: todos los `.c` **y** todos los `.h`. Si cambia cualquiera (incluido un
  header), `make` recompila.
- **Orden**: `gcc -Wall -Wextra -o cliente` y los seis `.c`.
  - **`-o cliente`**: el nombre del ejecutable que se genera.
  - **Los `.h` no se pasan al compilador**: los lee él solo cuando encuentra un `#include`.
- **La orden empieza con un TAB**, no con espacios. Es obligatorio en un Makefile: con espacios
  da `missing separator` (nos pasó porque el editor había puesto espacios).

### Líneas 9 a 16: los tres tests

```make
test_md5: src/test_md5.c src/md5.c src/md5.h
	$(CC) $(CFLAGS) -o test_md5 src/test_md5.c src/md5.c

test_autenticacion: src/test_autenticacion.c src/autenticacion.c src/autenticacion.h src/md5.c src/md5.h
	$(CC) $(CFLAGS) -o test_autenticacion src/test_autenticacion.c src/autenticacion.c src/md5.c

test_pdu: src/test_pdu.c src/pdu.c src/pdu.h src/md5.c src/md5.h
	$(CC) $(CFLAGS) -o test_pdu src/test_pdu.c src/pdu.c src/md5.c
```

- **Cada test compila solo lo que necesita**: su propio archivo de prueba, el módulo que prueba y
  `md5.c` si ese módulo firma algo. Por ejemplo, `test_pdu` se arma con
  `test_pdu.c + pdu.c + md5.c`, **sin ningún archivo que toque la red**.
- Cada uno tiene su propio `main` (el de `test_*.c`), así que no se compilan junto con
  `cliente.c`, que también tiene uno.
- `test_autenticacion` incluye `autenticacion.c`, que tiene funciones de red, pero los tests solo
  llaman a `separar_campos` y `verificar_respuesta`, que no tocan la red.

### Líneas 18 y 19: `run`

```make
run: cliente
	./cliente $(ARGS)
```

- Compila el cliente si hace falta y lo corre con los argumentos de la variable `ARGS`, que se
  pasa al llamar: `make run ARGS="grupo05 <secreto> 167.114.129.206"`.

### Líneas 21 a 24: `test`

```make
test: test_md5 test_autenticacion test_pdu
	./test_md5
	./test_autenticacion
	./test_pdu
```

- Compila los tres tests y los corre en orden. **Si uno falla, `make` se detiene**: cada test
  devuelve 1 cuando alguna prueba no pasa, y `make` corta ante cualquier orden que termine con un
  código distinto de 0.

### Líneas 26 y 27: `clean`

```make
clean:
	rm -f cliente test_md5 test_autenticacion test_pdu
```

- Borra los cuatro ejecutables. **`-f`**: no da error si alguno no existe.

### Línea 29: `.PHONY`

```make
.PHONY: all run test clean
```

- Le dice a `make` que estos cuatro objetivos **no son archivos**, sino nombres de tareas. Si no
  estuviera, y alguien creara un archivo llamado `test`, `make test` diría "ya está hecho" y no
  correría nada.

### Cómo se usa

| Comando | Qué hace |
|---|---|
| `make` | Compila `cliente` y los tres tests |
| `make test` | Corre `./test_md5`, `./test_autenticacion` y `./test_pdu` |
| `make run ARGS="<usuario> <secreto> <ip>"` | Compila y corre el cliente |
| `make clean` | Borra los ejecutables |

---

## Por qué hicimos tests sin red

- No queríamos depender del servidor de la cátedra para cada prueba.
- Podemos probar casos que el servidor **nunca** manda: el `\r` en un `recv` y el `\n` en el
  siguiente, todo byte por byte, un timestamp vacío, uno del futuro, firmas alteradas.
- Para que sea posible, **la lógica está separada de la red**: `validar_pdu` recibe la hora como
  parámetro, `extraer_pdu` recibe bytes, `verificar_respuesta` recibe un texto.
- Es la recomendación de la clase de sockets: *probar las cosas por partes*, así cuando se juntan
  ya se sabe que cada una anda.

## La estructura común de los tres tests

Los tres archivos siguen el mismo esquema:

```c
int probar_algo(..., esperado) {
    // 1. llamar a la función que se prueba
    // 2. comparar con lo esperado
    // 3. imprimir "OK" o "FALLA" y devolver 1 o 0
}

int main(void) {
    int pasaron = 0;
    int total = N;

    pasaron += probar_algo(...);    // una línea por caso
    ...

    printf("\n%d de %d pruebas pasaron\n", pasaron, total);
    if (pasaron == total)
        return 0;
    return 1;
}
```

- Cada función `probar_…` devuelve **1 si la prueba pasó y 0 si no**, así se pueden **sumar**:
  `pasaron +=` cuenta cuántas pasaron.
- **`total`** está escrito a mano con la cantidad de casos. Al final se compara con `pasaron`.
- **`main(void)`**: los tests no reciben argumentos.
- El `main` devuelve **0 si pasaron todas y 1 si no**. Es lo que mira `make test` para detenerse.

---

## `test_md5.c` (6 pruebas)

```c
int probar(char *mensaje, char *esperado) {
    char obtenido[MD5_HEX_LEN];

    md5_hex(mensaje, strlen(mensaje), obtenido);

    if (strcmp(obtenido, esperado) == 0) {
        printf("OK    md5(\"%s\")\n", mensaje);
        return 1;
    }
    printf("FALLA md5(\"%s\")\n", mensaje);
    printf("      esperado: %s\n", esperado);
    printf("      obtenido: %s\n", obtenido);
    return 0;
}
```

- Calcula el MD5 del mensaje y lo compara con el resultado conocido (con `strcmp`, que devuelve 0
  si son iguales).
- Si falla, muestra **el esperado y el obtenido**, para ver la diferencia.

Los casos:

- **4 vectores del RFC 1321, apéndice A.5** (con el link al RFC en el comentario):
  - `""` (texto vacío) → `d41d8cd98f00b204e9800998ecf8427e`;
  - `"abc"`;
  - 80 dígitos: **más de un bloque** de 64 bytes;
  - 62 caracteres: el resto es ≥ 56, así que **la cola ocupa dos bloques**.
- **Los 2 ejemplos del enunciado**: `grupo01|180.11.1.3|secreto01` →
  `f21d11f08cf40377860fd75d8bfd73cb` y `200|Port|17189|secreto01` →
  `6d720e86a353852c268b2071dffaf069`.

Juntos cubren los tres caminos del padding (cola de un bloque, de dos bloques, y bloques
completos antes de la cola).

---

## `test_autenticacion.c` (15 pruebas)

### `probar_separar`

```c
int probar_separar(char *mensaje, int esperado_ok, char *c0, char *c1, char *c2, char *c3) {
    char copia[PEDIDO_LEN];
    char *campos[4];
    int resultado;

    strcpy(copia, mensaje); // separar_campos modifica el mensaje: trabajamos sobre una copia
    resultado = separar_campos(copia, campos);
```

- Recibe el mensaje, si **se espera que sea válido** (`esperado_ok`) y los cuatro campos
  esperados.
- **Copia el mensaje** antes, porque `separar_campos` reemplaza los `|` por `'\0'`. Además, los
  mensajes de prueba son **strings literales** (`"200|Port|…"`), que en C no se pueden modificar:
  hacerlo haría que el programa se cayera.

```c
    if (!esperado_ok) {
        if (resultado != 0) {
            printf("OK    separar(\"%s\") -> rechazado\n", mensaje);
            return 1;
        }
        printf("FALLA separar(\"%s\") -> tendria que rechazarlo\n", mensaje);
        return 0;
    }
```

- **Si se esperaba un rechazo** (`!esperado_ok` = "no se esperaba OK"), la prueba pasa cuando
  `separar_campos` devolvió distinto de 0.

```c
    if (resultado == 0 && strcmp(campos[0], c0) == 0 && strcmp(campos[1], c1) == 0
        && strcmp(campos[2], c2) == 0 && strcmp(campos[3], c3) == 0) {
        printf("OK    separar(\"%s\")\n", mensaje);
        return 1;
    }
    printf("FALLA separar(\"%s\")\n", mensaje);
    return 0;
}
```

- **Si se esperaba que fuera válido**, la prueba pasa si devolvió 0 **y** los cuatro campos son
  los esperados. `&&` es el "y" lógico: tienen que cumplirse todas.

### `probar_verificar`

```c
int probar_verificar(char *respuesta, char *secreto, int esperado, int puerto_esperado) {
    int puerto = 0;
    int resultado;

    resultado = verificar_respuesta(respuesta, secreto, &puerto);
    if (resultado == esperado && (esperado != RESPUESTA_OK || puerto == puerto_esperado)) {
        printf("OK    verificar(\"%s\")\n", respuesta);
        return 1;
    }
    printf("FALLA verificar(\"%s\") -> resultado %d (esperado %d), puerto %d\n",
           respuesta, resultado, esperado, puerto);
    return 0;
}
```

- Llama a `verificar_respuesta` y compara el resultado con el esperado.
- **`(esperado != RESPUESTA_OK || puerto == puerto_esperado)`**: el puerto solo se compara cuando
  se espera un OK. Si se espera un rechazo, el puerto no importa. `||` es el "o" lógico.

### Los casos

- **`separar_campos`:** 3 mensajes válidos (incluido uno con un **campo vacío**,
  `200||17189|abc`) y 3 inválidos (`hola`, 3 campos, 5 campos).
- **`verificar_respuesta`:**
  - 200 con firma válida (el ejemplo del enunciado) → OK con puerto 17189.
  - Firma calculada con otro secreto, firma en ceros y **puerto alterado** (17190 con la firma
    del 17189) → INVALIDA. Este último muestra para qué sirve la firma: nadie puede cambiar el
    puerto en el camino.
  - Firma válida pero puerto `abc` o `70000` → INVALIDA (las firmas de esos dos casos se
    calcularon aparte, para que el rechazo sea por el puerto y no por la firma).
  - `400` y `401` → ERROR (se muestran los mensajes, por eso aparecen en la salida del test).
  - `hola` → INVALIDA.

---

## `test_pdu.c` (38 pruebas)

### `probar_framing`: simular `recv` partidos

```c
int probar_framing(char *nombre, char **fragmentos, int n_fragmentos,
                   char **esperadas, int n_esperadas) {
    acumulador a;
    char pdu[ACUMULADOR_LEN];
    int i;
    int obtenidas = 0;

    acumulador_iniciar(&a);
    for (i = 0; i < n_fragmentos; i++) {
        acumulador_agregar(&a, fragmentos[i], strlen(fragmentos[i]));
        while (extraer_pdu(&a, pdu) != -1) {
            if (obtenidas >= n_esperadas || strcmp(pdu, esperadas[obtenidas]) != 0) {
                printf("FALLA %s: PDU %d = \"%s\"\n", nombre, obtenidas + 1, pdu);
                return 0;
            }
            obtenidas++;
        }
    }
```

- **`fragmentos`**: un arreglo de strings, cada uno es **lo que traería un `recv`**.
  **`esperadas`**: las PDUs que tendrían que salir, en orden.
- **Hace exactamente lo mismo que `procesar_pdus`**, pero sin red: por cada fragmento, lo agrega
  al acumulador y saca todas las PDUs completas con el mismo `while`.
- Cada PDU que sale se compara con la que se esperaba en esa posición. Si sobra una
  (`obtenidas >= n_esperadas`) o no coincide, la prueba falla.

```c
    if (obtenidas != n_esperadas || a.largo != 0) {
        printf("FALLA %s: %d PDUs de %d, sobraron %d bytes\n",
               nombre, obtenidas, n_esperadas, a.largo);
        return 0;
    }
    printf("OK    %s\n", nombre);
    return 1;
}
```

- Al final comprueba dos cosas: que hayan salido **todas** las PDUs esperadas, y que **no hayan
  sobrado bytes** en el acumulador.

Los 8 casos (en el `main`, cada uno con dos arreglos `f` de fragmentos y `e` de esperadas):

| Prueba | Fragmentos |
|---|---|
| Una PDU entera | `"…&Primer mensaje\r\n"` |
| Dos PDUs en un `recv` | `"…&Second Message\r\n…&3er Hello\r\n"` |
| Una PDU en tres `recv` | `"…&Quere"`, `"s un cuarto mens"`, `"aje\r\n"` |
| Una PDU y media | `"…&Hola\r\n…&Ch"`, `"au\r\n"` |
| `\r\n` partido | `"…&Hola\r"`, `"\n"` |
| Mensaje vacío | `"1794663170&\r\n"` |
| Un `\n` solo no delimita | `"17946&a\nb\r\n"` → una sola PDU `17946&a\nb` |
| Byte por byte | `"1"`, `"&"`, `"a"`, `"\r"`, `"\n"`, `"2"`, … |

Los tres primeros reproducen **cómo llegaron las PDUs en la captura** real.

### `probar_validar` y `probar_mensaje_largo`

```c
int probar_validar(char *pdu, long ahora, int esperado) {
    int codigo = validar_pdu(pdu, strlen(pdu), ahora);

    if (codigo == esperado) {
        printf("OK    %d  \"%s\"\n", codigo, pdu);
        return 1;
    }
    printf("FALLA %d (esperado %d)  \"%s\"\n", codigo, esperado, pdu);
    return 0;
}
```

- Valida la PDU con una **hora fija** (`ahora`) y compara el código. Gracias a que `validar_pdu`
  recibe la hora, los resultados no dependen de cuándo se corre el test.

```c
int probar_mensaje_largo(int n, int esperado) {
    char pdu[200];

    strcpy(pdu, "1794663170&");
    memset(pdu + 11, 'A', n);
    pdu[11 + n] = '\0';
    return probar_validar(pdu, 1794663170, esperado);
}
```

- Arma una PDU con un mensaje de **`n` letras `A`**, para probar los bordes del largo sin
  escribir 100 letras a mano:
  - `strcpy` pone el timestamp y el `&` (11 caracteres);
  - **`memset(pdu + 11, 'A', n)`** escribe `n` letras `A` a partir de la posición 11;
  - `pdu[11 + n] = '\0'` termina el string.

Los 26 casos de validación:

- **Los 7 ejemplos del enunciado** (con la hora del enunciado, 1792303547): dan 200, 405, 406,
  407, 408, 409 y 400.
  Un detalle del código: el `⁰` de `"11 ⁰C"` se escribe como `\xe2\x81\xb0` (sus 3 bytes en
  UTF-8), y la `C` va en **otro string pegado** (`"…\xb0" "C"`), porque si no `\xb0C` se leería
  como un solo número hexadecimal.
- **Las 10 PDUs que manda el servidor de la cátedra.**
- **9 casos borde:** 100 caracteres (200) y 101 (410), 300 s (200) y 301 s (406), un timestamp
  del futuro (200), 11 dígitos con un cero adelante (200), timestamp vacío (400), `}` (200), `&`
  en el mensaje (408) y un tab (408).

### `probar_reporte`

```c
int probar_reporte(char *pdu, int codigo, int esperado_resultado, char *esperado) {
    char reporte[REPORTE_LEN];
    int resultado;

    resultado = armar_reporte("grupo01", pdu, codigo, "secreto01", reporte, sizeof(reporte));
    if (resultado != esperado_resultado) {
        printf("FALLA reporte(\"%s\") -> resultado %d (esperado %d)\n", pdu, resultado, esperado_resultado);
        return 0;
    }
    if (resultado == REPORTE_OK && strcmp(reporte, esperado) != 0) {
        printf("FALLA reporte(\"%s\")\n      esperado: %s\n      obtenido: %s\n", pdu, esperado, reporte);
        return 0;
    }
    if (resultado == REPORTE_OK)
        printf("OK    %s\n", reporte);
    else
        printf("OK    \"%s\" -> no se envia\n", pdu);
    return 1;
}
```

- Arma el reporte con el usuario y el secreto del enunciado (`grupo01`, `secreto01`).
- Primero compara **el resultado** (OK o no enviar); si es OK, compara además **el texto** del
  reporte completo, con la firma.
- **4 casos:** tres reportes comparados con firmas calculadas aparte con `md5sum`, y la PDU con
  `|`, que tiene que dar "no se envía".

---

## Cómo correrlo

```sh
make && make test
```

- **`&&`**: el segundo comando se ejecuta solo si el primero terminó bien.
- Resultado: `6 de 6`, `15 de 15` y `38 de 38` pruebas pasaron.
