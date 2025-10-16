#include "monitor.h"
#include <stdatomic.h>
#include <stdint.h>

static atomic_uint g_total_processors = ATOMIC_VAR_INIT(0);
static atomic_uint g_active_processors = ATOMIC_VAR_INIT(0);
static atomic_uint g_total_receptors = ATOMIC_VAR_INIT(0);
static atomic_uint g_active_receptors = ATOMIC_VAR_INIT(0);

void monitor_processor_started(void) {
    atomic_fetch_add(&g_total_processors, 1u);
    atomic_fetch_add(&g_active_processors, 1u);
}

void monitor_processor_stopped(void) {
    atomic_fetch_sub(&g_active_processors, 1u);
}

void monitor_receptor_started(void) {
    atomic_fetch_add(&g_total_receptors, 1u);
    atomic_fetch_add(&g_active_receptors, 1u);
}

void monitor_receptor_stopped(void) {
    atomic_fetch_sub(&g_active_receptors, 1u);
}

void monitor_get_counts(uint32_t *total_processors,
                        uint32_t *active_processors,
                        uint32_t *total_receptors,
                        uint32_t *active_receptors) {
    if (total_processors) *total_processors = (uint32_t)atomic_load(&g_total_processors);
    if (active_processors) *active_processors = (uint32_t)atomic_load(&g_active_processors);
    if (total_receptors) *total_receptors = (uint32_t)atomic_load(&g_total_receptors);
    if (active_receptors) *active_receptors = (uint32_t)atomic_load(&g_active_receptors);
}