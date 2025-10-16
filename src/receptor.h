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
bool receptor_start_async(const char *filepath, const char *key_bits, bool automatic);

#endif // RECEPTOR_H