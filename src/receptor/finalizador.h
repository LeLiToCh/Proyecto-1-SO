#ifndef FINALIZADOR_H
#define FINALIZADOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../src/memory.h"
#include "../src/processor.h"
#include "receptor.h"

typedef struct {
    size_t total_chars_transferred;
    size_t chars_in_memory_final;
    size_t memory_capacity;
    uint32_t active_processors;     // Emisores vivos
    uint32_t total_processors;      // Total de emisores
    uint32_t active_receptors;      // Receptores vivos
    uint32_t total_receptors;       // Total de receptores
} SystemStats;

// Envia la señal de terminación, espera el apagado de los procesos y presenta estadísticas.
bool finalizador_shutdown_system(size_t total_chars_written);

#endif // FINALIZADOR_H