/* ============================================================================
 * processor.h — Interfaz del procesador de archivo → memoria
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Declarar las funciones para codificar un archivo con XOR de 8 bits y
 *    escribirlo en el backend de memoria (local/compartido).
 * ========================================================================== */

#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stdbool.h>

/**
 * @brief Procesa el archivo de texto/binario indicado.
 * @param filepath Ruta UTF-8 del archivo a leer.
 * @param key_bits Cadena tipo "101010101" (8 o 9 bits soportados; si 9,
 *                 se ignora el bit más significativo y se usan 8 bits para XOR).
 * @param automatic true=proceso continuo; false=espera Enter entre caracteres.
 * @return true en éxito.
 */
bool process_file_into_memory(const char *filepath, const char *key_bits, bool automatic);

/**
 * @brief Inicia el procesamiento en un hilo en segundo plano.
 * @param filepath Ruta del archivo.
 * @param key_bits Cadena binaria para XOR (ver arriba).
 * @param automatic Modo automático/manual.
 * @return true si el hilo se creó correctamente.
 */
bool processor_start_async(const char *filepath, const char *key_bits, bool automatic);

#endif // PROCESSOR_H
