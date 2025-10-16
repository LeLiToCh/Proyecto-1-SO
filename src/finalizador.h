#ifndef FINALIZADOR_H
#define FINALIZADOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory.h"
#include "processor.h"
#include "receptor.h"

typedef struct
{
    size_t total_chars_transferred;
    size_t chars_in_memory_final;
    size_t memory_capacity;
    uint32_t active_processors; // Emisores vivos
    uint32_t total_processors;  // Total de emisores
    uint32_t active_receptors;  // Receptores vivos
    uint32_t total_receptors;   // Total de receptores
} SystemStats;

/*
 * SystemStats
 * ------------
 * Estructura que agrupa métricas del estado del sistema al momento del
 * apagado/consulta. Los campos representan lo siguiente:
 *
 * - total_chars_transferred: contador acumulado de caracteres transferidos
 *   (escritos) durante la ejecución del sistema.
 * - chars_in_memory_final: número de caracteres que permanecen en la
 *   memoria compartida al momento del apagado.
 * - memory_capacity: capacidad total de la memoria compartida en
 *   caracteres.
 * - active_processors: número de emisores (procesadores) que aún están
 *   activos/alive.
 * - total_processors: número total de emisores creados durante la ejecución.
 * - active_receptors: número de receptores que aún se encuentran activos.
 * - total_receptors: número total de receptores creados durante la ejecución.
 */

/**
 * finalizador_shutdown_system
 * ---------------------------
 * Inicia un apagado ordenado del sistema.
 *
 * Descripción:
 *  - Envía una señal (evento SDL_QUIT) para notificar a los componentes
 *    que deben terminar.
 *  - Espera un tiempo limitado a que emisores y receptores finalicen.
 *  - Recopila y muestra estadísticas (SystemStats) y libera recursos
 *    (memoria y SDL).
 *
 * Parámetros:
 *  - total_chars_written: contador total de caracteres escritos que se
 *    incluirá en las estadísticas mostradas.
 *
 * Retorno:
 *  - true si la función completó su secuencia de apagado (no garantiza
 *    que todos los hilos hayan finalizado correctamente, sino que la
 *    secuencia de parada se ejecutó hasta su fin).
 */
bool finalizador_shutdown_system(size_t total_chars_written);

#endif // FINALIZADOR_H