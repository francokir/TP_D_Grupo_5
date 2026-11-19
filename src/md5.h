#ifndef MD5_H
#define MD5_H

/* MD5 implementado a mano segun el RFC 1321:
   https://www.rfc-editor.org/rfc/rfc1321 */

#define MD5_HEX_LEN 33   // 32 caracteres hex + '\0'

/* calcula el MD5 de los primeros 'len' bytes de 'mensaje' y lo escribe
   en 'salida' como 32 caracteres hexadecimales en minuscula + '\0'. */
void md5_hex(char *mensaje, int len, char *salida);

#endif