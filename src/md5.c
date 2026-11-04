#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "md5.h"

/* Constantes K[i] = parte entera de |sen(i+1)| * 2^32 (tabla del RFC 1321) */
static const uint32_t K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

/* Cantidad de bits a rotar en cada paso (tabla del RFC 1321) */
static const int S[64] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

/* Rota los bits de x hacia la izquierda c lugares (los que salen por
   la izquierda entran por la derecha) */
static uint32_t rotar_izq(uint32_t x, int c) {
    return (x << c) | (x >> (32 - c));
}

static void procesar_bloque(uint32_t *estado, uint8_t *bloque) {
    uint32_t M[16]; // Bloque visto como 16 palabras de 32 bits
    int i;

    // Armamos cada palabra con 4 bytes en little-endian
    for (i = 0; i < 16; i++) {
        M[i] = (uint32_t) bloque[i*4]
            | ((uint32_t) bloque[i*4 + 1] << 8)
            | ((uint32_t) bloque[i*4 + 2] << 16)
            | ((uint32_t) bloque[i*4 + 3] << 24);
    }

    uint32_t A = estado[0];
    uint32_t B = estado[1];
    uint32_t C = estado[2];
    uint32_t D = estado[3];
    uint32_t F, temp;
    int g;

    /* 64 pasos en 4 rondas de 16 */
    for (i = 0; i < 64; i++) {
        if (i < 16) {             /* ronda 1 */
            F = (B & C) | (~B & D);
            g = i;
        } else if (i < 32) {      /* ronda 2 */
            F = (D & B) | (~D & C);
            g = (5*i + 1) % 16;
        } else if (i < 48) {      /* ronda 3 */
            F = B ^ C ^ D;
            g = (3*i + 5) % 16;
        } else {                  /* ronda 4 */
            F = C ^ (B | ~D);
            g = (7*i) % 16;
        }

        temp = D;
        D = C;
        C = B;
        B = B + rotar_izq(A + F + K[i] + M[g], S[i]);
        A = temp;
    }

    /* Sumamos el resultado del bloque al estado */
    estado[0] += A;
    estado[1] += B;
    estado[2] += C;
    estado[3] += D;
}   

void md5_hex(char *mensaje, int len, char *salida) {
    uint8_t *datos = (uint8_t *) mensaje; // Castear mensaje a bytes sin signo
    int bloques_completos = len/64; // Bloques que no necesitan padding
    int resto = len % 64; // Bytes que si necesitan padding
    uint8_t cola[128]; // Resto más padding (a lo sumo 2 bloques)    

    int cola_len;
    uint64_t largo_bits = (uint64_t) len * 8;
    uint32_t estado[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476}; // A,B,C,D fijos sacados del RFC
    uint8_t byte;
    int i;
    
    // Procesamos los bloques completos directamente desde el mensaje
    for (i = 0; i < bloques_completos; i++)
        procesar_bloque(estado, datos + i*64);

    // Copiamos los bytes que sobran y agregamos el bit 1 (byte 0x80)
    memset(cola, 0, sizeof(cola)); // Limpiamos la cola
    memcpy(cola, datos + bloques_completos * 64, resto); // Copir bytes del bloque incompleto a la cola
    cola[resto] = 0x80;

    // Si entran los 8 bytes del largo usamos un bloque, sino 2
    if (resto < 56)
        cola_len = 64;
    else
        cola_len = 128;
        
    // Ponemos el largo en bits en little endian en los últimos 8 bytes
    for (i = 0; i < 8; i++)
        cola[cola_len - 8 + i] = (largo_bits >> (8*i)) & 0xff;
    
    // Procesamos la cola (1 o 2 bloques)
    for (i = 0; i < cola_len; i += 64)
        procesar_bloque(estado, cola + i);
    
    // Salida: el estado son 16 bytes (A, B, C, D en little-endian).
    // Cada byte se escribe como 2 caracteres hex en minuscula.
    for (i = 0; i < 16; i++) {
        byte = (estado[i / 4] >> (8 * (i % 4))) & 0xff;
        sprintf(salida + i*2, "%02x", byte);
    }
}