# La captura de Wireshark, frame por frame

Archivo: **`TP_D_captura2.pcapng`** (desafíos 1 y 2). `TP_D_captura.pcapng` tiene solo el
desafío 1.

Filtro usado: `ip.addr == 167.114.129.206 || tcp.port == 80` (el servidor de la cátedra más la
consulta HTTP a ipify). La consigna exige entregar la captura filtrada para reducir el tráfico
que no tiene que ver con el TP.

- Cliente: `192.168.1.25` (IP privada). Sale a Internet como `190.225.36.143`.
- ipify: `104.26.12.205:80`.
- Servidor de la cátedra: `167.114.129.206`.

| Frames | t (s) | Qué se ve | Qué demuestra |
|---|---|---|---|
| 1–3 | 0,000–0,006 | SYN, SYN-ACK, ACK con ipify | 3-way handshake de `connect` |
| 4 | 0,006 | `GET / HTTP/1.1` (58 bytes) | El pedido HTTP escrito a mano |
| 6 | 0,168 | `HTTP/1.1 200 OK` con la IP (225 bytes) | La respuesta que parsea `obtener_ip` |
| 8–9 | 0,170 | FIN de ipify y FIN nuestro | `Connection: close`: `recv` devuelve 0 |
| 10 | 0,171 | UDP `56872 → 20262`: `grupo05\|190.225.36.143\|1baf07b1…` | El pedido firmado (el secreto no aparece) |
| 12 | 0,351 | UDP `20262 → 56872`: `200\|Port\|30029\|4238aec9…` | La respuesta vuelve al **mismo puerto**: NAT |
| 13–15 | 0,353–0,536 | 3-way handshake al puerto 30029 | Conexión inmediata (1,2 ms después), RTT ≈ 183 ms |
| 16 → 18 | 0,719 | Una PDU en un segmento (27 B) → su reporte 200 | Caso simple |
| 19 → 21, 22 | 0,819 | **Dos PDUs en un segmento** (49 B) → dos reportes seguidos | Más de una PDU por segmento |
| 23, 25, 27 → 29 | 0,921–1,124 | **Una PDU en tres segmentos** (16 + 16 + 5 B) → un reporte | Menos de una PDU por segmento |
| 30 | 1,225 | `17x0000000&Mensaje \| invalido`, y **ningún UDP** | Las PDUs con `\|` no se reenvían |
| 32 → 34 | 1,327 | `Mensaje con ~ invalido` → 408 | `~` = `0x7E` |
| 35 → 37 | 1,428 | `Timestamp de ayer` → 406 | Más de 300 s |
| 38 → 40 | 1,530 | `123456789012&…` → 407 | Más de 11 caracteres |
| 41 → 43 | 1,632 | `<ts>&` → 409 | Mensaje vacío |
| 44 → 46 | 1,733 | `<ts>&` + 101 `A` (114 B) → 410 | Mensaje de más de 100 |
| 47–49 | 1,734–1,917 | FIN del servidor, FIN nuestro, ACK | `recv` devuelve 0 y termina el loop |

## Cosas para señalar si piden mostrar la captura

- **Las cuentas de bytes cierran:** una PDU de 25 caracteres + `\r\n` = 27 bytes (frame 16).
  Las dos juntas del frame 19 son 27 + 22 = 49.
- **El cliente manda un ACK puro por cada segmento con datos** (frames 17, 20, 24, …): es TCP
  confirmando lo recibido, no algo que hagamos nosotros.
- **El servidor nunca contesta los reportes:** después del frame 12 no hay ningún UDP desde el
  servidor. Si un reporte se perdiera, nadie se enteraría.
- **Todos los UDP salen del mismo puerto 56872**, el del socket de la autenticación.
- Para ver el contenido: clic derecho en un paquete TCP → *Follow* → *TCP Stream* (el flujo
  completo de PDUs), o mirar el panel de bytes de cada UDP.
