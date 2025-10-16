#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void monitor_processor_started(void);
void monitor_processor_stopped(void);
void monitor_receptor_started(void);
void monitor_receptor_stopped(void);

void monitor_get_counts(uint32_t *total_processors,
                        uint32_t *active_processors,
                        uint32_t *total_receptors,
                        uint32_t *active_receptors);

#ifdef __cplusplus
}
#endif

#endif // MONITOR_H