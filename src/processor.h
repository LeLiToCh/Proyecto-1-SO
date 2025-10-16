#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stdbool.h>

// Process the given UTF-8 text file.
// key_bits: a C-string like "101010101" (8 or 9 bits supported; if 9, the MSB is ignored to use 8-bit XOR as per spec). 
// automatic=true -> process continuously; automatic=false -> wait for Enter between each character.
// Returns true on success.
bool process_file_into_memory(const char *filepath, const char *key_bits, bool automatic);

// Inicia el procesamiento en un hilo en segundo plano.
// Devuelve true si el hilo se creó correctamente.
bool processor_start_async(const char *filepath, const char *key_bits, bool automatic);

#endif // PROCESSOR_H
