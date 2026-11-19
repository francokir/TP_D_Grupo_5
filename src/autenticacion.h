#ifndef AUTENTICACION_H
#define AUTENTICACION_H

#define PEDIDO_LEN 256
#define RESPUESTA_TIMEOUT_MS 10000
#define RESPUESTA_OK 0 
#define RESPUESTA_ERROR 1
#define RESPUESTA_INVALIDA 2

/* arma el pedido de autenticacion <USUARIO>|<IP>|<FIRMA>, donde
   FIRMA = md5(<USUARIO>|<IP>|<SECRETO>). Lo deja en 'pedido'.
   Devuelve 0 si salio bien, 1 si hubo error. */
int armar_pedido(char *usuario, char *ip, char *secreto, char *pedido, int pedido_len);

/* manda 'pedido' por UDP a ip_servidor, puerto 20262.
   Devuelve el socket (para despues recibir la respuesta por el mismo),
   o -1 si hubo error. */
int enviar_pedido(char *ip_servidor, char *pedido);

/* espera hasta 10 segundos a que llegue un datagrama por el socket s.
   Si llega, lo deja en 'respuesta' (terminado en '\0') y la IP de
   quien lo mando en 'ip_origen'.
   Devuelve 0 si llego, 1 si se vencio el tiempo, -1 si hubo error. */
int esperar_respuesta(int s, int timeout_ms, char *respuesta, int respuesta_len, char *ip_origen);

/* separa "A|B|C|D" en sus 4 campos. MODIFICA 'mensaje': reemplaza cada '|'
   por '\0' y deja en campos[0..3] punteros al principio de cada campo.
   Devuelve 0 si hay exactamente 4 campos, 1 si hay mas o menos. */
int separar_campos(char *mensaje, char **campos);

/* analiza la respuesta del servidor <CODE>|<ACTION>|<TEXT>|<SIGNATURE>.
   - 200 con firma md5(<CODE>|<ACTION>|<TEXT>|<SECRETO>) valida: deja el
     puerto en *puerto y devuelve RESPUESTA_OK.
   - 400 o 401: muestra el error y devuelve RESPUESTA_ERROR.
   - Cualquier otra cosa (formato raro, firma que no coincide, puerto
     invalido): devuelve RESPUESTA_INVALIDA sin mostrar nada.
   No modifica 'respuesta'. */
int verificar_respuesta(char *respuesta, char *secreto, int *puerto);

/* espera la respuesta del servidor, como maximo RESPUESTA_TIMEOUT_MS EN TOTAL.
   Descarta en silencio lo que no venga de ip_servidor o sea invalido, y sigue
   esperando el tiempo que queda.
   Devuelve 0 y deja el puerto en *puerto si llego un 200 valido, 1 si no. */
int recibir_puerto(int s, char *ip_servidor, char *secreto, int *puerto);

/* manda 'mensaje' en un datagrama UDP a ip_servidor, puerto 20262, usando
   el socket 's' que ya existe (el de la autenticacion).
   Devuelve 0 si salio bien, 1 si hubo error. */
int enviar_al_servidor(int s, char *ip_servidor, char *mensaje);

#endif