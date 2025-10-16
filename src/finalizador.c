#include "finalizador.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

/*
 * finalizador.c
 * ---------------
 * Contiene la lógica para realizar un apagado ordenado del sistema.
 * El módulo recopila estadísticas del sistema, envía un evento SDL_QUIT
 * para indicar a los hilos/componentes que deben terminar, espera un
 * tiempo razonable a que los hilos finalicen y, finalmente, libera
 * recursos (memoria compartida y SDL).
 *
 * Notas de diseño:
 * - Este archivo asume que existen funciones externas proporcionadas por
 *   otros módulos del proyecto:
 *     - memory_capacity(), memory_size(), memory_shutdown() en `memory.h`.
 *     - monitor_get_counts(...) en `monitor.h` (declarada indirectamente en headers).
 * - Usa SDL para propagar un evento de cierre (SDL_QUIT). No gestiona la
 *   recepción del evento; los consumidores deben procesar SDL_PollEvent/WaitEvent.
 * - Todas las cadenas de salida/información están en español y se imprimen
 *   por stdout/stderr para facilitar debugging durante la ejecución del programa.
 *
 * Contrato general (resumen):
 * - finalizador_shutdown_system(total_chars_written)
 *   Entrada: número total de caracteres que fueron escritos/transferidos.
 *   Efecto: intenta apagar el sistema de forma ordenada, esperar la
 *           terminación de hilos dependientes, recopilar y mostrar
 *           estadísticas y liberar recursos.
 *   Retorna: true si la rutina terminó su ejecución (no implica que todos
 *            los hilos hayan acabado correctamente), siempre devuelve true
 *            salvo que se modifique la implementación.
 */

/**
 * collect_statistics
 * -------------------
 * Recopila estadísticas de estado del sistema en un único struct.
 *
 * Parámetros:
 *  - total_chars_written: contador acumulado de caracteres escritos
 *                        durante la ejecución (transferidos).
 *
 * Retorno:
 *  - SystemStats: estructura con los campos rellenados.
 *
 * Efectos secundarios:
 *  - Llama a funciones externas para consultar tamaño/capacidad de memoria
 *    y recuentos de hilos/procesos (monitor_get_counts()). Estas llamadas
 *    deben ser seguras en contexto concurrente (lectura de contadores).
 *
 * Casos borde/consideraciones:
 *  - Si la memoria o el monitor no están inicializados correctamente,
 *    las funciones consultadas deberían devolver valores coherentes (0)
 *    o manejar internamente los errores. Este módulo no hace comprobación
 *    adicional de errores al obtener dichas métricas.
 */
static SystemStats collect_statistics(size_t total_chars_written)
{
    SystemStats stats = {0};

    /* Estadísticas de memoria */
    stats.memory_capacity = memory_capacity();   /* Capacidad total en caracteres */
    stats.chars_in_memory_final = memory_size(); /* Caracteres actualmente en memoria */

    /* Caracteres transferidos */
    stats.total_chars_transferred = total_chars_written;

    /* Estadísticas de procesos/hilos: se obtienen mediante monitor_get_counts */
    uint32_t total_p = 0, active_p = 0, total_r = 0, active_r = 0;
    monitor_get_counts(&total_p, &active_p, &total_r, &active_r);
    stats.total_processors = (size_t)total_p;
    stats.active_processors = (size_t)active_p;
    stats.total_receptors = (size_t)total_r;
    stats.active_receptors = (size_t)active_r;

    return stats;
}

// Recopila y muestra estadísticas del sistema, luego apaga todo ordenadamente.
/**
 * display_statistics
 * ------------------
 * Muestra por stdout un resumen con formato de las estadísticas del sistema.
 *
 * Parámetros:
 *  - stats: puntero a SystemStats (no nulo) con los valores a mostrar.
 *
 * Efectos secundarios:
 *  - Escribe en stdout usando printf. No modifica la estructura `stats`.
 *
 * Consideraciones:
 *  - Esta función es idempotente y segura para llamarse en contexto de
 *    finalización donde sólo se desea informar del estado. No realiza
 *    llamadas que cambien el estado global.
 */
static void display_statistics(const SystemStats *stats)
{
    printf("\n\n\x1b[36m================================================\x1b[0m \n");
    printf("\x1b[36m| \x1b[33m\x1b[1mESTADÍSTICAS GENERALES DEL SISTEMA\x1b[0m\x1b[36m |\x1b[0m \n");
    printf("\x1b[36m================================================\x1b[0m \n");

    /* Estadísticas de Transferencia */
    printf("\x1b[32m- Transferencia:\x1b[0m \n");
    printf("  \x1b[34m- Caracteres Transferidos (Escritos): \x1b[0m%zu \n", stats->total_chars_transferred);

    /* Estadísticas de Memoria */
    printf("\x1b[32m- Memoria Compartida:\x1b[0m \n");
    printf("  \x1b[34m- Capacidad Total: \x1b[0m%zu caracteres \n", stats->memory_capacity);
    printf("  \x1b[34m- Caracteres Pendientes (Final): \x1b[0m%zu caracteres \n", stats->chars_in_memory_final);

    /* Estadísticas de Procesos/Hilos */
    /* Mostrar activos/total. Se imprime como active/total para facilitar lectura. */
    printf("\x1b[32m- Procesos/Hilos:\x1b[0m \n");
    printf("  \x1b[34m- Emisores Vivos/Totales: \x1b[0m%zu / %zu \n", stats->active_processors, stats->total_processors);
    printf("  \x1b[34m- Receptores Vivos/Totales: \x1b[0m%zu / %zu \n", stats->active_receptors, stats->total_receptors);

    /* Memoria Utilizada (como porcentaje) */
    float usage_percent = (stats->memory_capacity > 0)
                              ? ((float)stats->chars_in_memory_final / stats->memory_capacity) * 100.0f
                              : 0.0f;
    printf("\x1b[32m- Utilización de Memoria (Final):\x1b[0m %.2f%% \n", usage_percent);

    printf("\x1b[36m================================================\x1b[0m\n");
}

/**
 * finalizador_shutdown_system
 * ---------------------------
 * Intenta realizar un apagado ordenado del sistema.
 *
 * Flujo de ejecución:
 *  1. Recopila estadísticas actuales (memoria, contadores de hilos, etc.).
 *  2. Empuja un evento SDL_QUIT en la cola de eventos SDL para notificar a
 *     los componentes que deben finalizar.
 *  3. Espera hasta `max_attempts` iteraciones (con un retardo de `delay_ms`)
 *     a que los contadores de emisores y receptores lleguen a 0.
 *  4. Muestra estadísticas, libera la memoria compartida y cierra SDL.
 *
 * Parámetros:
 *  - total_chars_written: número total de caracteres escritos/transferidos
 *                         que se incluirán en el resumen estadístico.
 *
 * Retorno:
 *  - true: la función completa su ejecución. No implica que todos los
 *          hilos externos finalizaron correctamente; esa información
 *          aparece en la salida impresa.
 *
 * Consideraciones de concurrencia:
 *  - Esta función espera de forma activa (polling con SDL_Delay) a que
 *    los contadores de hilos bajen a 0. Esto es una espera ocupada de baja
 *    intensidad y supone que las otras partes del programa responderán al
 *    evento SDL_QUIT y decrementar sus contadores.
 *  - Si algunas tareas no responden, se hace un log por stderr y se procede
 *    a liberar recursos para evitar deadlocks permanentes.
 */
bool finalizador_shutdown_system(size_t total_chars_written)
{
    printf("[FINALIZADOR] Iniciando apagado elegante del sistema. \n");

    /* Recopilar estadísticas antes de iniciar la secuencia de apagado */
    SystemStats stats = collect_statistics(total_chars_written);

    /* Preparar y enviar evento SDL_QUIT para notificar a los consumidores */
    SDL_Event quit_event;
    quit_event.type = SDL_QUIT;

    /* Enviar evento: SDL_PushEvent devuelve 0 en caso de error */
    if (SDL_PushEvent(&quit_event) == 0)
    {
        fprintf(stderr, "[FINALIZADOR] Error al enviar evento SDL_QUIT: %s \n", SDL_GetError());
    }
    else
    {
        printf("[FINALIZADOR] Evento SDL_QUIT enviado a todos los procesos. \n");
    }

    /* Esperar a que emisores y receptores terminen, hasta un máximo */
    int attempts = 0;
    const int max_attempts = 50; /* Número máximo de iteraciones de espera */
    const int delay_ms = 100;    /* Retardo entre iteraciones (ms) */
    bool receptor_finished = false;

    while (attempts < max_attempts)
    {
        uint32_t tp, ap, tr, ar;
        monitor_get_counts(&tp, &ap, &tr, &ar);
        /* Condición de terminación: no hay emisores ni receptores activos */
        if (ap == 0 && ar == 0)
        {
            printf("[FINALIZADOR] Todos los procesos han terminado. \n");
            receptor_finished = true;
            break;
        }
        /* Pequeña espera cooperativa: evita busy-waiting intenso */
        SDL_Delay(delay_ms);
        attempts++;
    }
    if (attempts == max_attempts)
    {
        fprintf(stderr, "[FINALIZADOR] Tiempo de espera agotado. Algunos procesos no respondieron. \n");
    }

    if (!receptor_finished)
    {
        /* Si no terminaron, se emite una advertencia y se procede a liberar */
        fprintf(stderr, "[FINALIZADOR] Advertencia: Receptor no terminó elegantemente. Forzando liberación. \n");
    }
    else
    {
        printf("[FINALIZADOR] Apagado de hilos dependientes exitoso. \n");
    }

    /* Mostrar las estadísticas recopiladas anteriormente */
    display_statistics(&stats);

    /* Liberar recursos dependientes */
    memory_shutdown(); /* Cierra/limpia la memoria compartida */
    SDL_Quit();        /* Finaliza subsistema SDL */

    printf("[FINALIZADOR] Sistema apagado y recursos liberados. \n");
    return true;
}
