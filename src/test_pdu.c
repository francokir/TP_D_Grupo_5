#include <stdio.h>
#include <string.h>
#include "pdu.h"

/* mete los fragmentos en un acumulador, uno por uno (como si fueran recv
   sucesivos), y despues de cada uno saca todas las PDUs completas.
   Compara las PDUs obtenidas con las esperadas, en orden.
   Devuelve 1 si la prueba paso, 0 si no. */
int probar_framing(char *nombre, char **fragmentos, int n_fragmentos,
                   char **esperadas, int n_esperadas) {
    acumulador a;
    char pdu[ACUMULADOR_LEN];
    int i;
    int obtenidas = 0;

    acumulador_iniciar(&a);
    for (i = 0; i < n_fragmentos; i++) {
        acumulador_agregar(&a, fragmentos[i], strlen(fragmentos[i]));
        while (extraer_pdu(&a, pdu) != -1) {
            if (obtenidas >= n_esperadas || strcmp(pdu, esperadas[obtenidas]) != 0) {
                printf("FALLA %s: PDU %d = \"%s\"\n", nombre, obtenidas + 1, pdu);
                return 0;
            }
            obtenidas++;
        }
    }

    if (obtenidas != n_esperadas || a.largo != 0) {
        printf("FALLA %s: %d PDUs de %d, sobraron %d bytes\n",
               nombre, obtenidas, n_esperadas, a.largo);
        return 0;
    }
    printf("OK    %s\n", nombre);
    return 1;
}

/* valida 'pdu' con la hora 'ahora' y compara con el codigo esperado.
   Devuelve 1 si la prueba paso, 0 si no. */
int probar_validar(char *pdu, long ahora, int esperado) {
    int codigo = validar_pdu(pdu, strlen(pdu), ahora);

    if (codigo == esperado) {
        printf("OK    %d  \"%s\"\n", codigo, pdu);
        return 1;
    }
    printf("FALLA %d (esperado %d)  \"%s\"\n", codigo, esperado, pdu);
    return 0;
}

// arma una PDU con un mensaje de 'n' letras A y la valida.
int probar_mensaje_largo(int n, int esperado) {
    char pdu[200];

    strcpy(pdu, "1794663170&");
    memset(pdu + 11, 'A', n);
    pdu[11 + n] = '\0';
    return probar_validar(pdu, 1794663170, esperado);
}

/* arma el reporte de 'pdu' con 'codigo' (usuario grupo01, secreto secreto01)
   y compara con lo esperado. Si esperado_resultado no es REPORTE_OK, no
   compara el texto. Devuelve 1 si la prueba paso, 0 si no. */
int probar_reporte(char *pdu, int codigo, int esperado_resultado, char *esperado) {
    char reporte[REPORTE_LEN];
    int resultado;

    resultado = armar_reporte("grupo01", pdu, codigo, "secreto01", reporte, sizeof(reporte));
    if (resultado != esperado_resultado) {
        printf("FALLA reporte(\"%s\") -> resultado %d (esperado %d)\n", pdu, resultado, esperado_resultado);
        return 0;
    }
    if (resultado == REPORTE_OK && strcmp(reporte, esperado) != 0) {
        printf("FALLA reporte(\"%s\")\n      esperado: %s\n      obtenido: %s\n", pdu, esperado, reporte);
        return 0;
    }
    if (resultado == REPORTE_OK)
        printf("OK    %s\n", reporte);
    else
        printf("OK    \"%s\" -> no se envia\n", pdu);
    return 1;
}

int main(void) {
    int pasaron = 0;
    int total = 38;

    // 1 PDU en 1 recv
    char *f1[] = {"1794663169&Primer mensaje\r\n"};
    char *e1[] = {"1794663169&Primer mensaje"};
    pasaron += probar_framing("una PDU entera", f1, 1, e1, 1);

    // 2 PDUs en 1 recv (como Second Message + 3er Hello del servidor)
    char *f2[] = {"1794663170&Second Message\r\n1794663170&3er Hello\r\n"};
    char *e2[] = {"1794663170&Second Message", "1794663170&3er Hello"};
    pasaron += probar_framing("dos PDUs en un recv", f2, 1, e2, 2);

    // 1 PDU en 3 recv (como el cuarto mensaje del servidor)
    char *f3[] = {"1794663170&Quere", "s un cuarto mens", "aje\r\n"};
    char *e3[] = {"1794663170&Queres un cuarto mensaje"};
    pasaron += probar_framing("una PDU en tres recv", f3, 3, e3, 1);

    // cantidad no entera: una PDU y media, y despues el resto
    char *f4[] = {"1794663170&Hola\r\n1794663170&Ch", "au\r\n"};
    char *e4[] = {"1794663170&Hola", "1794663170&Chau"};
    pasaron += probar_framing("PDU y media en un recv", f4, 2, e4, 2);

    // el delimitador partido: el \r en un recv y el \n en el siguiente
    char *f5[] = {"1794663170&Hola\r", "\n"};
    char *e5[] = {"1794663170&Hola"};
    pasaron += probar_framing("\\r\\n partido entre dos recv", f5, 2, e5, 1);

    // mensaje vacio: la PDU existe aunque no tenga mensaje
    char *f6[] = {"1794663170&\r\n"};
    char *e6[] = {"1794663170&"};
    pasaron += probar_framing("PDU con mensaje vacio", f6, 1, e6, 1);

    // un \n solo no es delimitador
    char *f7[] = {"17946&a\nb\r\n"};
    char *e7[] = {"17946&a\nb"};
    pasaron += probar_framing("\\n solo no delimita", f7, 1, e7, 1);

    // byte por byte: cada recv trae un solo caracter
    char *f8[] = {"1", "&", "a", "\r", "\n", "2", "&", "b", "\r", "\n"};
    char *e8[] = {"1&a", "2&b"};
    pasaron += probar_framing("byte por byte", f8, 10, e8, 2);

    // validar_pdu: ejemplos del enunciado (hora actual 1792303547)
    pasaron += probar_validar("1792303544&Temperatura ambiente 11 grados", 1792303547, PDU_OK);
    pasaron += probar_validar("12345.1&Temperatura ambiente 11 grados", 1792303547, PDU_TS_INVALIDO);
    pasaron += probar_validar("1762303544&Temperatura ambiente 11 grados", 1792303547, PDU_TS_EXPIRADO);
    pasaron += probar_validar("1111762303544&Temperatura ambiente 11 grados", 1792303547, PDU_TS_LARGO);
    /* "11 ⁰C": el ⁰ son 3 bytes en UTF-8. El "C" va en otro string porque
       \xb0C se leeria como un solo numero hexadecimal */
    pasaron += probar_validar("1792303544&Temperatura ambiente 11 \xe2\x81\xb0" "C", 1792303547, PDU_MSG_INVALIDO);
    pasaron += probar_validar("1792303544&", 1792303547, PDU_MSG_VACIO);
    pasaron += probar_validar("1792303544^Temperatura ambiente 11 \xe2\x81\xb0" "C", 1792303547, PDU_BAD_REQUEST);

    // validar_pdu: las PDUs del servidor de la catedra (hora 1794663170)
    pasaron += probar_validar("1794663169&Primer mensaje", 1794663170, PDU_OK);
    pasaron += probar_validar("1794663170&Second Message", 1794663170, PDU_OK);
    pasaron += probar_validar("1794663170&3er Hello", 1794663170, PDU_OK);
    pasaron += probar_validar("1794663170&Queres un cuarto mensaje", 1794663170, PDU_OK);
    pasaron += probar_validar("17x0000000&Mensaje | invalido", 1794663170, PDU_TS_INVALIDO);
    pasaron += probar_validar("1794663170&Mensaje con ~ invalido", 1794663170, PDU_MSG_INVALIDO);
    pasaron += probar_validar("1794576770&Timestamp de ayer", 1794663170, PDU_TS_EXPIRADO);
    pasaron += probar_validar("123456789012&Timestamp demasiado largo", 1794663170, PDU_TS_LARGO);
    pasaron += probar_validar("1794663170&", 1794663170, PDU_MSG_VACIO);
    pasaron += probar_mensaje_largo(101, PDU_MSG_LARGO);

    // validar_pdu: bordes
    pasaron += probar_mensaje_largo(100, PDU_OK);                                 // justo el maximo
    pasaron += probar_validar("1794662870&Hola", 1794663170, PDU_OK);              // 300 s exactos
    pasaron += probar_validar("1794662869&Hola", 1794663170, PDU_TS_EXPIRADO);     // 301 s
    pasaron += probar_validar("1794663999&Hola", 1794663170, PDU_OK);              // en el futuro
    pasaron += probar_validar("01794663170&Hola", 1794663170, PDU_OK);             // 11 digitos
    pasaron += probar_validar("&Hola", 1794663170, PDU_BAD_REQUEST);               // timestamp vacio
    pasaron += probar_validar("1794663170&Hola}", 1794663170, PDU_OK);             // '}' = 0x7D, el ultimo valido
    pasaron += probar_validar("1794663170&Hola&Chau", 1794663170, PDU_MSG_INVALIDO); // '&' en el mensaje
    pasaron += probar_validar("1794663170&Hola\tChau", 1794663170, PDU_MSG_INVALIDO); // tab = 0x09

    // armar_reporte (firmas calculadas con md5sum)
    pasaron += probar_reporte("1794663169&Primer mensaje", PDU_OK, REPORTE_OK,
                              "grupo01|1794663169&Primer mensaje|200|e7ce8d440c4b1f9f323efe5f3c0defe4");
    pasaron += probar_reporte("1794663170&Mensaje con ~ invalido", PDU_MSG_INVALIDO, REPORTE_OK,
                              "grupo01|1794663170&Mensaje con ~ invalido|408|f386a3d3b660612833a32d8d5c73d4bf");
    pasaron += probar_reporte("1794663170&", PDU_MSG_VACIO, REPORTE_OK,
                              "grupo01|1794663170&|409|d0e6c3968e89d8bfd2f30fff45792bb8");
    pasaron += probar_reporte("17x0000000&Mensaje | invalido", PDU_TS_INVALIDO, REPORTE_NO_ENVIAR, NULL);

    printf("\n%d de %d pruebas pasaron\n", pasaron, total);

    if (pasaron == total)
        return 0;
    return 1;
}