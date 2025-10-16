#pragma once
#include <stdbool.h>
#include <stddef.h>

void app_state_set(const char *identificador, size_t cantidad, const char *clave, bool automatic);
const char* app_state_get_identificador(void);
size_t app_state_get_cantidad(void);
const char* app_state_get_clave(void);
bool app_state_get_automatic(void);
