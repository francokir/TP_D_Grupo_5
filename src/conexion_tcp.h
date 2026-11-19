#ifndef CONEXION_TCP_H
#define CONEXION_TCP_H

/* 3-way handshake para conectarnos por TCP a ip:puerto
    devuelve el socket, o -1 si hubo error. */
int conectar_tcp(char *ip, int puerto);

/* lee las PDUs que manda el servidor por el socket TCP 's_tcp' hasta que
   cierra la conexion. Por cada PDU completa: la valida, arma el reporte
   firmado y lo manda por el socket UDP 's_udp' al servidor.
   Devuelve la cantidad de reportes enviados, o -1 si hubo error. */
int procesar_pdus(int s_tcp, int s_udp, char *ip_servidor, char *usuario, char *secreto);

#endif