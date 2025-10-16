#ifndef RECEPTOR_H
#define RECEPTOR_H

#include <stdbool.h>

// Proceso de lectura de caracteres desde la memoria compartida
// key_bits: una cadena C como "101010101" (se admiten 8 o 9 bits; si es 9, se ignora el MSB para usar XOR de 8 bits según la especificación).
// automatic=true -> leer continuamente; automatic=false -> esperar a Enter entre cada carácter.
// Devuelve true si se leyó correctamente.
bool process_memory_to_output(const char *filepath, const char *key_bits, bool automatic);

// Inicia la recepción en un hilo en segundo plano.
// Devuelve true si el hilo se creó correctamente.
bool receptor_start_async(const char *key_bits, bool automatic);

/*
 * Notas sobre la semántica y comportamiento del receptor:
 * - El receptor lee entradas del buffer/memoria compartida mediante
 *   `memory_read_entry` (definida en `memory.h`). Por cada entrada válida,
 *   decodifica el byte usando XOR con una clave de 8 bits derivada de
 *   `key_bits` y lo escribe en el fichero `filepath`.
 * - Si `automatic` es false, el receptor espera pulsar Enter entre cada
 *   lectura; si es true, procesa continuamente hasta recibir un evento
 *   de terminación (SDL_QUIT).
 * - La función `process_memory_to_output` realiza I/O en disco. El llamador
 *   debe asegurarse de que la ruta `filepath` es válida y que hay permisos
 *   de escritura.
 * - `receptor_start_async` crea un hilo SDL que ejecuta el receptor en
 *   background y registra el inicio con el monitor. Al finalizar, el hilo
 *   llama a `monitor_receptor_stopped()`.
 */

#endif // RECEPTOR_H