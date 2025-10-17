/* ============================================================================
 * emisor.h — API auxiliar para ventana del emisor (selector y procesamiento)
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Exponer una función para abrir una ventana independiente del "emisor"
 *    que permite seleccionar un archivo y lanzar el procesamiento.
 *
 * Puntos clave:
 *  - La ventana administra su propio ciclo de eventos/render.
 *  - Soporta modo automático o manual, y una clave binaria de 8 caracteres.
 *  - Usa un identificador lógico para inicializar/adjuntar memoria compartida.
 *
 * Notas:
 *  - La función es asíncrona respecto al resto de la aplicación principal
 *    (no bloquea el hilo llamante).
 * ========================================================================== */

#ifndef EMISOR_H
#define EMISOR_H

#include <stdbool.h>

/**
 * @brief Inicia una ventana del emisor para seleccionar y procesar un archivo.
 * @param identificador  Nombre base del espacio de memoria compartida.
 * @param cantidad       Capacidad sugerida (número de “páginas”/slots del buffer).
 * @param clave          Clave binaria de 8 caracteres ('0'/'1') para el XOR.
 * @param automatic      true = modo automático; false = modo manual.
 * @note La ventana es autónoma (ciclo de eventos propio) y no bloquea.
 */
void sender_window_start_async(const char *identificador, int cantidad, const char *clave, bool automatic);

#endif // EMISOR_H
