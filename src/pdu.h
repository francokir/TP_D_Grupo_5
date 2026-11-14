#ifndef PDU_H
#define PDU_H

#define ACUMULADOR_LEN 4096   // bytes recibidos que todavia no formaron una PDU

/* Codigos de parseo de una PDU (ver enunciado) */
#define PDU_OK               200
#define PDU_BAD_REQUEST      400  /* error no tipificado (ej: no hay '&') */
#define PDU_TS_INVALIDO      405  /* timestamp con caracteres que no son digitos */
#define PDU_TS_EXPIRADO      406  /* timestamp con mas de 300 s de antiguedad */
#define PDU_TS_LARGO         407  /* timestamp de mas de 11 caracteres */
#define PDU_MSG_INVALIDO     408  /* mensaje con caracteres invalidos */
#define PDU_MSG_VACIO        409  /* mensaje vacio */
#define PDU_MSG_LARGO        410  /* mensaje de mas de 100 caracteres */

#define TS_MAX_LEN       11
#define MSG_MAX_LEN      100
#define TS_MAX_ANTIGUEDAD 300

/* Acumulador de bytes recibidos por TCP: guarda lo que llego
   y todavia no se separo en PDUs. */
typedef struct {
    char datos[ACUMULADOR_LEN];
    int largo;                  /* cuantos bytes validos hay en datos */
} acumulador;

/* Resultados de armar_reporte */
#define REPORTE_OK         0   /* reporte armado */
#define REPORTE_NO_ENVIAR  1   /* la PDU tiene '|': no se reenvia */
#define REPORTE_ERROR     -1   /* el reporte no entra en el buffer */
#define REPORTE_LEN (ACUMULADOR_LEN + 128)  /* entra cualquier PDU que salga del acumulador */

/* Deja el acumulador vacio. */
void acumulador_iniciar(acumulador *a);

/* Agrega 'len' bytes al final del acumulador.
   Devuelve 0 si entraron, 1 si no hay lugar. */
int acumulador_agregar(acumulador *a, char *datos, int len);

/* Si en el acumulador hay una PDU completa (terminada en \r\n), la copia
   en 'pdu' SIN el \r\n y terminada en '\0', y la saca del acumulador.
   'pdu' tiene que tener lugar para ACUMULADOR_LEN bytes.
   Devuelve el largo de la PDU (puede ser 0), o -1 si todavia no hay
   ninguna PDU completa. */
int extraer_pdu(acumulador *a, char *pdu);

/* Valida una PDU <TIMESTAMP>&<MESSAGE> (sin el \r\n) de 'len' bytes y
   devuelve su codigo de parseo. 'ahora' es la hora actual (unix timestamp):
   se recibe como parametro para poder probarla con horas fijas.
   Si hay varios errores, devuelve el primero que encuentra. */
int validar_pdu(char *pdu, int len, long ahora);

/* Arma el reporte UDP <USUARIO>|<PDU>|<CODIGO>|<FIRMA>, donde
   FIRMA = md5(<USUARIO>|<PDU>|<CODIGO>|<SECRETO>). Lo deja en 'reporte'.
   Las PDUs que contienen '|' no se reenvian (devuelve REPORTE_NO_ENVIAR). */
int armar_reporte(char *usuario, char *pdu, int codigo, char *secreto,
                  char *reporte, int reporte_len);

#endif