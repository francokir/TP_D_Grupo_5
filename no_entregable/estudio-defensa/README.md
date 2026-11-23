# Material de estudio para la defensa oral – TP D (Grupo 5)

Defensa: **martes 24/11/2026**. Tenemos **15 minutos por grupo**, y ese tiempo incluye las
preguntas que nos hagan. Las preguntas pueden ser sobre el código y sobre cómo funciona el
programa en general. La consigna avisa que *es obligatorio conocer el funcionamiento detallado
del código*, así que **los tres tenemos que poder explicar cualquier archivo**, no solo la
parte que presentamos.

## Qué hay en esta carpeta

| Archivo | Para qué sirve |
|---|---|
| [01-guion-15-minutos.md](01-guion-15-minutos.md) | Qué dice cada uno, en qué orden y qué mostrar en pantalla |
| [02-preguntas.md](02-preguntas.md) | Preguntas probables, con la respuesta corta para decir en voz alta |
| [03-vision-general.md](03-vision-general.md) | El programa de punta a punta: la secuencia completa y el `main` (`cliente.c`) línea por línea |
| [04-ip_publica.md](04-ip_publica.md) | `ip_publica.c` y `.h` línea por línea: la IP pública por HTTP escrito a mano |
| [05-md5.md](05-md5.md) | `md5.c` y `.h` línea por línea: el MD5 implementado según el RFC 1321 |
| [06-autenticacion.md](06-autenticacion.md) | `autenticacion.c` y `.h` línea por línea: el pedido firmado, el UDP, la espera con `select` y la verificación |
| [07-conexion_tcp.md](07-conexion_tcp.md) | `conexion_tcp.c` y `.h` línea por línea: la conexión TCP y el loop del desafío 2 |
| [08-pdu.md](08-pdu.md) | `pdu.c` y `.h` línea por línea: el acumulador (framing), la validación y el reporte |
| [09-tests-y-makefile.md](09-tests-y-makefile.md) | El `Makefile` línea por línea, las funciones de prueba y qué prueba cada test |
| [10-captura.md](10-captura.md) | La captura de Wireshark, frame por frame |
| [11-conceptos-decisiones-y-limites.md](11-conceptos-decisiones-y-limites.md) | Conceptos de la materia, decisiones que tomamos y lo que el programa no hace |

## Cómo estudiarlo

1. Leer primero [03-vision-general.md](03-vision-general.md), para tener el mapa completo.
2. Leer los archivos 04 a 08 **con el código abierto al lado**. Recorren cada archivo completo
   y en orden, desde los `#include` hasta el último `return`: cada bloque de código va con las
   líneas que ocupa y debajo la explicación de cada línea. Al final de cada uno hay una tabla
   de preguntas que pueden hacer sobre ese archivo.
3. Ensayar el guion con un cronómetro: la presentación tiene que entrar en ~9 minutos, así
   quedan ~6 para preguntas.
4. Tomarse las preguntas de [02-preguntas.md](02-preguntas.md) entre nosotros: uno pregunta
   y otro contesta sin mirar.

> Estos archivos son solo para estudiar. No forman parte de la entrega.
