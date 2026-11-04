#ifndef IP_PUBLICA_H
#define IP_PUBLICA_H

#define INET4_ADDRSTRLEN 16

int consulta_ip(char *buffer, int buffer_len);   // llena el buffer con la respuesta HTTP
int obtener_ip(char *respuesta, char *ip);       // busca la IP dentro de respuesta

#endif