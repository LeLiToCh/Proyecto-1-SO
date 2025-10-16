#include "monitor.h"
#include <stdatomic.h>
#include <stdint.h>

/*
 * monitor.c
 * ---------
 * Implementación de un pequeño monitor basado en contadores atómicos.
 *
 * Los contadores mantienen dos métricas para emisores (processors) y
 * receptores: el total creado y el número actualmente activo. Todas las
 * operaciones usan atomics para evitar la necesidad de mutexes y para ser
 * seguras en entornos multihilo.
 */

static atomic_uint g_total_processors = ATOMIC_VAR_INIT(0);
static atomic_uint g_active_processors = ATOMIC_VAR_INIT(0);
static atomic_uint g_total_receptors = ATOMIC_VAR_INIT(0);
static atomic_uint g_active_receptors = ATOMIC_VAR_INIT(0);

/*
 * Se incrementan los contadores total y active al iniciar un emisor.
 * Se usan operaciones de suma atómica (fetch_add) para evitar race conditions.
 */
void monitor_processor_started(void)
{
    atomic_fetch_add(&g_total_processors, 1u);
    atomic_fetch_add(&g_active_processors, 1u);
}

/*
 * Al detener un emisor solo decrementar el contador active.
 * No se modifica el contador total porque representa el histórico.
 */
void monitor_processor_stopped(void)
{
    atomic_fetch_sub(&g_active_processors, 1u);
}

/*
 * Funciones equivalentes para receptores.
 */
void monitor_receptor_started(void)
{
    atomic_fetch_add(&g_total_receptors, 1u);
    atomic_fetch_add(&g_active_receptors, 1u);
}

void monitor_receptor_stopped(void)
{
    atomic_fetch_sub(&g_active_receptors, 1u);
}

/*
 * monitor_get_counts
 *  - Copia los valores actuales de los contadores atómicos a los punteros
 *    proporcionados (si no son NULL). Cada carga es atómica y devuelve el
 *    snapshot del contador correspondiente en ese instante.
 */
void monitor_get_counts(uint32_t *total_processors,
                        uint32_t *active_processors,
                        uint32_t *total_receptors,
                        uint32_t *active_receptors)
{
    if (total_processors)
        *total_processors = (uint32_t)atomic_load(&g_total_processors);
    if (active_processors)
        *active_processors = (uint32_t)atomic_load(&g_active_processors);
    if (total_receptors)
        *total_receptors = (uint32_t)atomic_load(&g_total_receptors);
    if (active_receptors)
        *active_receptors = (uint32_t)atomic_load(&g_active_receptors);
}