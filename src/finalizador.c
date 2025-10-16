#include "finalizador.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

static SystemStats collect_statistics( size_t total_chars_written) {
    SystemStats stats = {0};

    // Estadisticas de memoria
    stats.memory_capacity = memory_capacity();
    stats.chars_in_memory_final = memory_size();

    // Caracteres transferidos
    stats.total_chars_transferred = total_chars_written;

    // Estadisticas de procesos
    uint32_t total_p = 0, active_p = 0, total_r = 0, active_r = 0;
    monitor_get_counts(&total_p, &active_p, &total_r, &active_r);
    stats.total_processors = (size_t) total_p;
    stats.active_processors = (size_t) active_p;
    stats.total_receptors = (size_t) total_r;
    stats.active_receptors = (size_t) active_r;

    return stats;
}

// Recopila y muestra estadísticas del sistema, luego apaga todo ordenadamente.
static void display_statistics(const SystemStats *stats) {
    printf("\n\n\x1b[36m================================================\x1b[0m \n");
    printf("\x1b[36m| \x1b[33m\x1b[1mESTADÍSTICAS GENERALES DEL SISTEMA\x1b[0m\x1b[36m |\x1b[0m \n");
    printf("\x1b[36m================================================\x1b[0m \n");

    // Estadísticas de Transferencia
    printf("\x1b[32m- Transferencia:\x1b[0m \n");
    printf("  \x1b[34m- Caracteres Transferidos (Escritos): \x1b[0m%zu \n", stats->total_chars_transferred);
    
    // Estadísticas de Memoria
    printf("\x1b[32m- Memoria Compartida:\x1b[0m \n");
    printf("  \x1b[34m- Capacidad Total: \x1b[0m%zu caracteres \n", stats->memory_capacity);
    printf("  \x1b[34m- Caracteres Pendientes (Final): \x1b[0m%zu caracteres \n", stats->chars_in_memory_final);
    
    // Estadísticas de Procesos
    printf("\x1b[32m- Procesos/Hilos:\x1b[0m \n");
    printf("  \x1b[34m- Emisores Vivos/Totales: \x1b[0m%u / %u \n", stats->active_processors, stats->total_processors);
    printf("  \x1b[34m- Receptores Vivos/Totales: \x1b[0m%u / %u \n", stats->active_receptors, stats->total_receptors);
    
    // Memoria Utilizada (como porcentaje)
    float usage_percent = (stats->memory_capacity > 0) 
                          ? ((float)stats->chars_in_memory_final / stats->memory_capacity) * 100.0f 
                          : 0.0f;
    printf("\x1b[32m- Utilización de Memoria (Final):\x1b[0m %.2f%% \n", usage_percent);
    
    printf("\x1b[36m================================================\x1b[0m\n");
}

bool finalizador_shutdown_system(size_t total_chars_written) {
    printf("[FINALIZADOR] Iniciando apagado elegante del sistema. \n");

    SystemStats stats = collect_statistics(total_chars_written);

    SDL_Event quit_event;
    quit_event.type = SDL_QUIT;

    if (SDL_PushEvent(&quit_event) == 0) {
        fprintf(stderr, "[FINALIZADOR] Error al enviar evento SDL_QUIT: %s \n", SDL_GetError());
    } else {
        printf("[FINALIZADOR] Evento SDL_QUIT enviado a todos los procesos. \n");
    }

    int attempts = 0;
    const int max_attempts = 50;
    const int delay_ms = 100;
    bool receptor_finished = false;

    while (attempts < max_attempts) {
        uint32_t tp, ap, tr, ar;
        monitor_get_counts(&tp, &ap, &tr, &ar);
        if (ap == 0 && ar == 0) {
            printf("[FINALIZADOR] Todos los procesos han terminado. \n");
            receptor_finished = true;
            break;
        }
        SDL_Delay(delay_ms);
        attempts++;
    }
    if (attempts == max_attempts) {
        fprintf(stderr, "[FINALIZADOR] Tiempo de espera agotado. Algunos procesos no respondieron. \n");
    }

    if (!receptor_finished) {
        fprintf(stderr, "[FINALIZADOR] Advertencia: Receptor no terminó elegantemente. Forzando liberación. \n");
    } else {
        printf("[FINALIZADOR] Apagado de hilos dependientes exitoso. \n");
    }

    display_statistics(&stats);

    memory_shutdown();
    SDL_Quit();

    printf("[FINALIZADOR] Sistema apagado y recursos liberados. \n");
    return true;
}
