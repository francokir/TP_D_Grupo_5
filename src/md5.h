#ifndef MD5_H
#define MD5_H

#define MD5_HEX_LEN 33   // 32 caracteres hex + '\0'

/* Calcula el MD5 de los primeros 'len' bytes de 'mensaje' y lo escribe
   en 'salida' como 32 caracteres hexadecimales en minuscula + '\0'. */
void md5_hex(char *mensaje, int len, char *salida);

#endif