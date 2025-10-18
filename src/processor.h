/* ============================================================================
 * processor.h — Interfaz del procesador de archivo → memoria
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Declarar las funciones para codificar un archivo con XOR de 8 bits y
 *    escribirlo en el backend de memoria (local/compartido).
 *  - Ahora incluye soporte para procesos pesados (sin hilos SDL).
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
 * @brief Inicia el procesamiento como un proceso independiente (sin hilos).
 * @param filepath Ruta del archivo origen (UTF-8).
 * @param key_bits Cadena binaria para derivar clave XOR de 8 bits.
 * @param automatic true=Automático, false=Manual.
 * @return true si el proceso se lanzó correctamente; false en caso contrario.
 */
bool processor_start_heavy(const char *filepath, const char *key_bits, bool automatic);

#endif // PROCESSOR_H
