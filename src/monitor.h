#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * monitor.h
     * ---------
     * Pequeño módulo para mantener contadores atómicos relacionados con la
     * creación y finalización de emisores (procesadores) y receptores.
     *
     * El uso principal es que distintas partes del programa registren cuando
     * inician o terminan, permitiendo al finalizador o a herramientas de
     * diagnóstico consultar recuentos consistentes sin necesidad de locks.
     *
     * Todas las funciones son seguras para llamadas concurrentes desde
     * múltiples hilos: utilizan operaciones atómicas internas.
     */

    /**
     * monitor_processor_started
     *   Indica que se ha creado y arrancado un nuevo emisor (processor).
     *   Efecto: incrementa los contadores 'total' y 'active'.
     */
    void monitor_processor_started(void);

    /**
     * monitor_processor_stopped
     *   Indica que un emisor ha terminado su ejecución.
     *   Efecto: decrementa el contador 'active'. No modifica 'total'.
     */
    void monitor_processor_stopped(void);

    /**
     * monitor_receptor_started
     *   Indica que se ha creado y arrancado un nuevo receptor.
     *   Efecto: incrementa los contadores 'total' y 'active'.
     */
    void monitor_receptor_started(void);

    /**
     * monitor_receptor_stopped
     *   Indica que un receptor ha terminado su ejecución.
     *   Efecto: decrementa el contador 'active'. No modifica 'total'.
     */
    void monitor_receptor_stopped(void);

    /**
     * monitor_get_counts
     *   Recupera los recuentos actuales. Cada puntero es opcional (puede ser NULL).
     *
     * Parámetros de salida (todos opcionales):
     *  - total_processors: número total de emisores creados
     *  - active_processors: emisores actualmente activos
     *  - total_receptors: número total de receptores creados
     *  - active_receptors: receptores actualmente activos
     *
     * Nota: los valores leídos representan un snapshot atómico por cada contador
     * individual. No hay garantía de que los cuatro valores representen un
     * estado consistente combinado en el tiempo si el sistema está siendo
     * modificado concurrentemente por otros hilos.
     */
    void monitor_get_counts(uint32_t *total_processors,
                            uint32_t *active_processors,
                            uint32_t *total_receptors,
                            uint32_t *active_receptors);

#ifdef __cplusplus
}
#endif

#endif // MONITOR_H