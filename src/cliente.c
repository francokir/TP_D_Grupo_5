#include <stdio.h>
#include "ip_publica.h"
#include "md5.h"

int main(int argc, char *argv[]) {
    char respuesta[3000];
    char ip[INET4_ADDRSTRLEN];

    if (consulta_ip(respuesta, sizeof(respuesta)) != 0)
        return 1;
    //printf("%s\n", respuesta); // Prueba de que la respuesta se guarda bien en el buffer

    if (obtener_ip(respuesta, ip) != 0)
        return 1;
    
    printf("IP pública: %s\n", ip);

    return 0;
}