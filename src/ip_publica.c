#include "ip_publica.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#define IP_API_HOST "api.ipify.org"
#define IP_API_PORT "80"

int consulta_ip(char *buffer, int buffer_len) {
    int s; // File descriptor
    struct addrinfo hints;
    struct addrinfo *servinfo; // Resultados

    int status;
    int inbytes;
    int total = 0;

    memset(&hints,0,sizeof(hints));     /* es necesario inicializar con ceros */
    hints.ai_family = AF_INET;          /* Address Family */
    hints.ai_socktype = SOCK_STREAM;    /* Socket Type: TCP */

    // Consulta DNS con getaddrinfo. La respuesta se guarda en servinfo
    if ( (status = getaddrinfo(IP_API_HOST, IP_API_PORT, &hints, &servinfo))!=0)
    {
      fprintf(stderr, "Error en getaddrinfo: %s\n",gai_strerror(status));
      return 1;
    }
    // Armamos socket con respuesta de la consulta DNS getaddrinfo
    s = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (s == -1)
    {
      fprintf(stderr,"Error en socket %i: %s\n",errno,strerror(errno));
      return 1;
    }
    // Establecemos conexión TCP con connect (3-way handshake)
    switch (connect(s, servinfo->ai_addr, servinfo->ai_addrlen))
    {
      case -1:
        fprintf(stderr,"Error en connect %i: %s\n",errno,strerror(errno));
        close(s);
        return 1;
      case 0:
        //printf("Conectado a %s\n\n", IP_API_HOST);
        break;
    }
    freeaddrinfo(servinfo);

    // Envio del request HTTP
    char *request = "GET / HTTP/1.1\r\n"
                    "Host: " IP_API_HOST "\r\n"
                    "Connection: close\r\n"
                    "\r\n";

    send(s, request, strlen(request), 0);

    // loop de lectura (acumulamos toda la respuesta en el buffer)
    memset(buffer, 0, buffer_len);
    while (1)
    {
      if (total >= buffer_len - 1) {
        fprintf(stderr, "Error: la respuesta no entra en el buffer\n");
        close(s);
        return 1;
      }

      inbytes = recv(s, buffer + total, buffer_len - 1 - total, 0); /* -1: dejo lugar para el '\0' */
      if (inbytes == 0) /* cuando recv devuelve 0 implica EOF */
        break;
      if (inbytes == -1)
      {
        fprintf(stderr,"Error en recv %i: %s\n",errno,strerror(errno));
        close(s);
        return 1;
      }
      total += inbytes;
    }

    close(s);
    return 0;
}

int obtener_ip(char *respuesta, char *ip) {
    char *body;
    int len;
    struct in_addr addr; // solo para validar la IP con inet_pton

    // Chequeamos que el servidor haya respondido 200 OK
    if (strncmp(respuesta, "HTTP/1.1 200", 12) != 0)
    {
      fprintf(stderr, "Error: respuesta HTTP inesperada:\n%s\n", respuesta);
      return 1;
    }

    // Buscamos la linea vacia que separa los headers del body
    body = strstr(respuesta, "\r\n\r\n");
    if (body == NULL)
    {
      fprintf(stderr, "Error: la respuesta HTTP no tiene fin de headers\n");
      return 1;
    }
    body = body + 4; // salteamos el \r\n\r\n

    // La IP son los primeros caracteres del body que sean digitos o puntos
    len = strspn(body, "0123456789.");
    if (len == 0 || len >= INET4_ADDRSTRLEN)
    {
      fprintf(stderr, "Error: el body no tiene una IP: %s\n", body);
      return 1;
    }
    memcpy(ip, body, len);
    ip[len] = '\0';

    // Validamos que sea una IPv4 de verdad (4 números entre 0 y 255 separados por puntos)
    if (inet_pton(AF_INET, ip, &addr) != 1)
    {
      fprintf(stderr, "Error: IP invalida: %s\n", ip);
      return 1;
    }
    return 0;
}