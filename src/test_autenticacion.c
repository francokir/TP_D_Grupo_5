#include <stdio.h>
#include <string.h>
#include "autenticacion.h"

/* Separa 'mensaje' y compara con lo esperado. Si esperado_ok es 0, se espera
   que separar_campos falle. Devuelve 1 si la prueba paso, 0 si no. */
int probar_separar(char *mensaje, int esperado_ok, char *c0, char *c1, char *c2, char *c3) {
    char copia[PEDIDO_LEN];
    char *campos[4];
    int resultado;

    strcpy(copia, mensaje); /* separar_campos modifica el mensaje: trabajamos sobre una copia */
    resultado = separar_campos(copia, campos);

    if (!esperado_ok) {
        if (resultado != 0) {
            printf("OK    separar(\"%s\") -> rechazado\n", mensaje);
            return 1;
        }
        printf("FALLA separar(\"%s\") -> tendria que rechazarlo\n", mensaje);
        return 0;
    }

    if (resultado == 0 && strcmp(campos[0], c0) == 0 && strcmp(campos[1], c1) == 0
        && strcmp(campos[2], c2) == 0 && strcmp(campos[3], c3) == 0) {
        printf("OK    separar(\"%s\")\n", mensaje);
        return 1;
    }
    printf("FALLA separar(\"%s\")\n", mensaje);
    return 0;
}

/* Verifica 'respuesta' con 'secreto' y compara con lo esperado.
   Si se espera RESPUESTA_OK, tambien compara el puerto. */
int probar_verificar(char *respuesta, char *secreto, int esperado, int puerto_esperado) {
    int puerto = 0;
    int resultado;

    resultado = verificar_respuesta(respuesta, secreto, &puerto);
    if (resultado == esperado && (esperado != RESPUESTA_OK || puerto == puerto_esperado)) {
        printf("OK    verificar(\"%s\")\n", respuesta);
        return 1;
    }
    printf("FALLA verificar(\"%s\") -> resultado %d (esperado %d), puerto %d\n",
           respuesta, resultado, esperado, puerto);
    return 0;
}

int main(void) {
    int pasaron = 0;
    int total = 15;

    /* Mensajes validos: 4 campos */
    pasaron += probar_separar("200|Port|17189|6d720e86a353852c268b2071dffaf069", 1,
                              "200", "Port", "17189", "6d720e86a353852c268b2071dffaf069");
    pasaron += probar_separar("401|Message|Unauthorized|00000000000000000000000000000000", 1,
                              "401", "Message", "Unauthorized", "00000000000000000000000000000000");
    pasaron += probar_separar("200||17189|abc", 1, "200", "", "17189", "abc"); /* campo vacio */

    /* Mensajes invalidos: mas o menos de 4 campos */
    pasaron += probar_separar("hola", 0, NULL, NULL, NULL, NULL);
    pasaron += probar_separar("200|Port|17189", 0, NULL, NULL, NULL, NULL);
    pasaron += probar_separar("200|Port|17189|abc|extra", 0, NULL, NULL, NULL, NULL);
    
    /* verificar_respuesta: 200 con firma valida (ejemplo del enunciado) */
    pasaron += probar_verificar("200|Port|17189|6d720e86a353852c268b2071dffaf069", "secreto01", RESPUESTA_OK, 17189);

    /* 200 con firma invalida: hay que descartarlo */
    pasaron += probar_verificar("200|Port|17189|6d720e86a353852c268b2071dffaf069", "otro_secreto", RESPUESTA_INVALIDA, 0);
    pasaron += probar_verificar("200|Port|17189|00000000000000000000000000000000", "secreto01", RESPUESTA_INVALIDA, 0);
    pasaron += probar_verificar("200|Port|17190|6d720e86a353852c268b2071dffaf069", "secreto01", RESPUESTA_INVALIDA, 0); /* puerto alterado */

    /* 200 con firma valida pero puerto invalido */
    pasaron += probar_verificar("200|Port|abc|4caea1bd333df128b882c61fa4fd5903", "secreto01", RESPUESTA_INVALIDA, 0);
    pasaron += probar_verificar("200|Port|70000|7a2b2ae6985088249e3e4549286df390", "secreto01", RESPUESTA_INVALIDA, 0);

    /* Errores del servidor */
    pasaron += probar_verificar("400|Message|Bad format|00000000000000000000000000000000", "secreto01", RESPUESTA_ERROR, 0);
    pasaron += probar_verificar("401|Message|Unauthorized|00000000000000000000000000000000", "secreto01", RESPUESTA_ERROR, 0);

    /* Cualquier otra cosa */
    pasaron += probar_verificar("hola", "secreto01", RESPUESTA_INVALIDA, 0);


    printf("\n%d de %d pruebas pasaron\n", pasaron, total);

    if (pasaron == total)
        return 0;
    return 1;
}