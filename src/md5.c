#include <string.h>
#include "md5.h"

void md5_hex(char *mensaje, int len, char *salida) {
    /* Por ahora no calculamos nada: devolvemos 32 ceros */
    memset(salida, '0', 32);
    salida[32] = '\0';
}