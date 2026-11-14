#include <stdio.h>
#include <unistd.h>
#include "ip_publica.h"
#include "autenticacion.h"
#include "conexion_tcp.h"

int main(int argc, char *argv[]) {
    char *usuario;
    char *secreto;
    char *ip_servidor;
    char respuesta[3000];
    char ip[INET4_ADDRSTRLEN];
    char pedido[PEDIDO_LEN];
    int s; // socket UDP
    int puerto;
    int s_tcp; // socket del desafío 2
    int enviados;

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

    // Desafio 2 - Paso 1: conectarnos por TCP al puerto asignado
    s_tcp = conectar_tcp(ip_servidor, puerto);
    if (s_tcp == -1) {
        close(s);
        return 1;
    }
    printf("Conectado por TCP a %s:%d\n", ip_servidor, puerto);

    // Desafio 2 - Paso 2: recibir las PDUs, validarlas y reenviarlas por UDP
    enviados = procesar_pdus(s_tcp, s, ip_servidor, usuario, secreto);
    close(s_tcp);
    if (enviados == -1) {
        close(s);
        return 1;
    }
    printf("Conexion TCP cerrada por el servidor. Reportes enviados: %d\n", enviados);

    close(s);

    return 0;
}