#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>  /* necesario para memset(), strlen() */
#include <errno.h>   /* necesario para codigos de errores */
#include <netinet/in.h>
#include <netdb.h>   /* necesario para getaddrinfo() */
#include <unistd.h>  /* necesario para close() */
#include <sys/select.h> /* necesario para select() */
#include <arpa/inet.h>  /* necesario para inet_ntop() */
#include "autenticacion.h"
#include "md5.h"
#include "ip_publica.h" /* por INET4_ADDRSTRLEN */

#define SERVIDOR_PUERTO_UDP "20262"

int armar_pedido(char *usuario, char *ip, char *secreto, char *pedido, int pedido_len) {
    char a_firmar[PEDIDO_LEN];
    char firma[MD5_HEX_LEN];
    int n;

    // Texto a firmar: <USUARIO>|<IP>|<SECRETO>
    n = snprintf(a_firmar, sizeof(a_firmar), "%s|%s|%s", usuario, ip, secreto);
    if (n < 0 || n >= (int) sizeof(a_firmar)) {
        fprintf(stderr, "Error: el texto a firmar no entra en el buffer\n");
        return 1;
    }
    md5_hex(a_firmar, n, firma);

    // Pedido: <USUARIO>|<IP>|<FIRMA>
    n = snprintf(pedido, pedido_len, "%s|%s|%s", usuario, ip, firma);
    if (n < 0 || n >= pedido_len) {
        fprintf(stderr, "Error: el pedido no entra en el buffer\n");
        return 1;
    }
    return 0;
}

int enviar_pedido(char *ip_servidor, char *pedido) {
    int s; /*file descriptor*/
    struct addrinfo hints;
    struct addrinfo *servinfo; /* es donde van los resultados */
    int status;
    int enviados;

    memset(&hints,0,sizeof(hints));     /* es necesario inicializar con ceros */
    hints.ai_family = AF_INET;          /* Address Family */
    hints.ai_socktype = SOCK_DGRAM;     /* Socket Type: UDP */

    // Armamos la direccion del servidor (IP + puerto)
    if ( (status = getaddrinfo(ip_servidor, SERVIDOR_PUERTO_UDP, &hints, &servinfo))!=0)
    {
      fprintf(stderr, "Error en getaddrinfo: %s\n",gai_strerror(status));
      return -1;
    }
    // Creamos el socket UDP
    s = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (s == -1)
    {
      fprintf(stderr,"Error en socket %i: %s\n",errno,strerror(errno));
      freeaddrinfo(servinfo);
      return -1;
    }

    // Mandamos el pedido en un datagrama (sin connect: le decimos el destino a sendto)
    enviados = sendto(s, pedido, strlen(pedido), 0, servinfo->ai_addr, servinfo->ai_addrlen);
    freeaddrinfo(servinfo);
    if (enviados == -1)
    {
      fprintf(stderr,"Error en sendto %i: %s\n",errno,strerror(errno));
      close(s);
      return -1;
    }
    return s;
}

int esperar_respuesta(int s, int timeout_ms, char *respuesta, int respuesta_len, char *ip_origen) {
    /* variables del select */
    struct timeval tv;
    fd_set rfds;
    int retval;
    /* variables para saber quien mando el datagrama */
    struct sockaddr_in their_addr;
    socklen_t addrsize;
    int inbytes;

    /* pre-select: configuramos el timeout */
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    /* pre-select: borramos el SET de file descriptors y agregamos el socket */
    FD_ZERO(&rfds);
    FD_SET(s, &rfds);

    /* Esperamos hasta que el socket tenga algo para leer o se venza el tiempo */
    retval = select(s + 1, &rfds, NULL, NULL, &tv);
    if (retval == -1)
    {
      fprintf(stderr,"Error en select %i: %s\n",errno,strerror(errno));
      return -1;
    }
    if (retval == 0)
      return 1; /* timeout: no llego nada */

    /* Hay un datagrama esperando: lo leemos */
    addrsize = sizeof(their_addr); /* very important: inicializarlo antes de recvfrom */
    memset(respuesta, 0, respuesta_len);
    inbytes = recvfrom(s, respuesta, respuesta_len - 1, 0, (struct sockaddr *) &their_addr, &addrsize);
    if (inbytes == -1)
    {
      fprintf(stderr,"Error en recvfrom %i: %s\n",errno,strerror(errno));
      return -1;
    }

    /* Pasamos la IP de quien lo mando a texto */
    inet_ntop(AF_INET, &(their_addr.sin_addr), ip_origen, INET4_ADDRSTRLEN);
    return 0;
}

int separar_campos(char *mensaje, char **campos) {
    char *pipe;
    int i;

    // Los primeros 3 campos terminan cada uno en un '|'
    for (i = 0; i < 3; i++) {
        campos[i] = mensaje;
        pipe = strchr(mensaje, '|');
        if (pipe == NULL)
            return 1;          /* faltan campos */
        *pipe = '\0';          /* cortamos el campo ahi */
        mensaje = pipe + 1;    /* el siguiente empieza despues del '|' */
    }

    // El cuarto campo es todo lo que queda, y no puede tener mas '|'
    campos[3] = mensaje;
    if (strchr(mensaje, '|') != NULL)
        return 1;              /* sobran campos */
    return 0;
}

int verificar_respuesta(char *respuesta, char *secreto, int *puerto) {
    char copia[PEDIDO_LEN];
    char *campos[4];
    char a_firmar[PEDIDO_LEN];
    char firma[MD5_HEX_LEN];
    char *fin;
    long numero;
    int n;

    // Trabajamos sobre una copia porque separar_campos modifica el mensaje
    if (strlen(respuesta) >= sizeof(copia))
        return RESPUESTA_INVALIDA;
    strcpy(copia, respuesta);
    if (separar_campos(copia, campos) != 0)
        return RESPUESTA_INVALIDA;

    // 400 o 401: el servidor rechazo el pedido (la firma son 32 ceros, no se verifica)
    if (strcmp(campos[0], "400") == 0 || strcmp(campos[0], "401") == 0) {
        fprintf(stderr, "Error del servidor %s: %s\n", campos[0], campos[2]);
        return RESPUESTA_ERROR;
    }

    // Lo unico que aceptamos ademas es 200|Port|...
    if (strcmp(campos[0], "200") != 0 || strcmp(campos[1], "Port") != 0)
        return RESPUESTA_INVALIDA;

    // Verificamos la firma: md5(<CODE>|<ACTION>|<TEXT>|<SECRETO>)
    n = snprintf(a_firmar, sizeof(a_firmar), "%s|%s|%s|%s", campos[0], campos[1], campos[2], secreto);
    if (n < 0 || n >= (int) sizeof(a_firmar))
        return RESPUESTA_INVALIDA;
    md5_hex(a_firmar, n, firma);
    if (strcmp(firma, campos[3]) != 0)
        return RESPUESTA_INVALIDA;  /* firma invalida: descartar en silencio */

    // El TEXT tiene que ser un numero de puerto valido (1 a 65535)
    numero = strtol(campos[2], &fin, 10);
    if (fin == campos[2] || *fin != '\0' || numero < 1 || numero > 65535)
        return RESPUESTA_INVALIDA;

    *puerto = (int) numero;
    return RESPUESTA_OK;
}

int recibir_puerto(int s, char *ip_servidor, char *secreto, int *puerto) {
    struct timeval inicio, ahora;
    int transcurrido_ms, restante_ms;
    char respuesta[PEDIDO_LEN];
    char ip_origen[INET4_ADDRSTRLEN];
    int resultado;

    gettimeofday(&inicio, NULL); /* anotamos cuando empezamos a esperar */

    while (1)
    {
        // Cuanto tiempo queda de los 10 segundos
        gettimeofday(&ahora, NULL);
        transcurrido_ms = (ahora.tv_sec - inicio.tv_sec) * 1000
                        + (ahora.tv_usec - inicio.tv_usec) / 1000;
        restante_ms = RESPUESTA_TIMEOUT_MS - transcurrido_ms;
        if (restante_ms <= 0)
            break;
        
        // Esperamos un datagrama, como mucho el tiempo que queda
        resultado = esperar_respuesta(s, restante_ms, respuesta, sizeof(respuesta), ip_origen);
        if (resultado == -1)
            return 1;            /* error (ya se mostro) */
        if (resultado == 1)
            break;               /* timeout */

        // Si no viene del servidor, lo ignoramos
        if (strcmp(ip_origen, ip_servidor) != 0)
            continue;

        resultado = verificar_respuesta(respuesta, secreto, puerto);
        if (resultado == RESPUESTA_OK)
            return 0;
        if (resultado == RESPUESTA_ERROR)
            return 1;            /* 400/401 (ya se mostro) */
        /* RESPUESTA_INVALIDA: descartar en silencio y seguir esperando */
    }

    fprintf(stderr, "Error: no hubo respuesta valida en %d segundos\n", RESPUESTA_TIMEOUT_MS / 1000);
    return 1;
}

int enviar_al_servidor(int s, char *ip_servidor, char *mensaje) {
    struct addrinfo hints;
    struct addrinfo *servinfo; /* es donde van los resultados */
    int status;
    int enviados;

    memset(&hints,0,sizeof(hints));     /* es necesario inicializar con ceros */
    hints.ai_family = AF_INET;          /* Address Family */
    hints.ai_socktype = SOCK_DGRAM;     /* Socket Type: UDP */

    // Armamos la direccion del servidor (IP + puerto 20262)
    if ( (status = getaddrinfo(ip_servidor, SERVIDOR_PUERTO_UDP, &hints, &servinfo))!=0)
    {
      fprintf(stderr, "Error en getaddrinfo: %s\n",gai_strerror(status));
      return 1;
    }

    enviados = sendto(s, mensaje, strlen(mensaje), 0, servinfo->ai_addr, servinfo->ai_addrlen);
    freeaddrinfo(servinfo);
    if (enviados == -1)
    {
      fprintf(stderr,"Error en sendto %i: %s\n",errno,strerror(errno));
      return 1;
    }
    return 0;
}