#include <stdio.h>
#include <unistd.h>
#include "ip_publica.h"
#include "autenticacion.h"

int main(int argc, char *argv[]) {
    char *usuario;
    char *secreto;
    char *ip_servidor;
    char respuesta[3000];
    char ip[INET4_ADDRSTRLEN];
    char pedido[PEDIDO_LEN];
    int s; // socket UDP
    int puerto;

    // Leemos los argumentos
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <usuario> <secreto> <ip_servidor>\n", argv[0]);
        return 1;
    }
    usuario = argv[1];
    secreto = argv[2];
    ip_servidor = argv[3];

    // Paso 1: IP publica
    if (consulta_ip(respuesta, sizeof(respuesta)) != 0)
        return 1;
    if (obtener_ip(respuesta, ip) != 0)
        return 1;
    printf("IP pública: %s\n", ip);

    // Paso 2: armar el pedido firmado
    if (armar_pedido(usuario, ip, secreto, pedido, sizeof(pedido)) != 0)
        return 1;
    printf("Pedido: %s\n", pedido);
    
    // Paso 3: mandarlo por UDP
    s = enviar_pedido(ip_servidor, pedido);
    if (s == -1)
        return 1;
    printf("Pedido enviado a %s\n", ip_servidor);

    // Paso 4: esperar la respuesta y verificarla
    if (recibir_puerto(s, ip_servidor, secreto, &puerto) != 0) {
        close(s);
        return 1;
    }
    printf("Autenticado. Puerto TCP asignado: %d\n", puerto);

    close(s);

    return 0;
}