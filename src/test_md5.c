#include <stdio.h>
#include <string.h>
#include "md5.h"

/* Calcula md5(mensaje) y lo compara con el resultado esperado.
   Devuelve 1 si coincide, 0 si no. */
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

int main(void) {
    int pasaron = 0;
    int total = 5;

    /* Vectores de prueba del RFC 1321 */
    pasaron += probar("", "d41d8cd98f00b204e9800998ecf8427e");
    pasaron += probar("abc", "900150983cd24fb0d6963f7d28e17f72");
    pasaron += probar("12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                      "57edf4a22be3c955ac49da2e2107b67a");

    /* Ejemplos del enunciado del TP */
    pasaron += probar("grupo01|180.11.1.3|secreto01", "f21d11f08cf40377860fd75d8bfd73cb");
    pasaron += probar("200|Port|17189|secreto01", "6d720e86a353852c268b2071dffaf069");

    printf("\n%d de %d pruebas pasaron\n", pasaron, total);

    if (pasaron == total)
        return 0;
    return 1;
}
