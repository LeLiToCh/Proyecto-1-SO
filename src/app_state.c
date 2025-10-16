#include "app_state.h"
#include <string.h>
#include <stdlib.h>

static char g_identificador[256] = "";
static size_t g_cantidad = 1;
static char g_clave[9] = "";
static bool g_automatic = true;

void app_state_set(const char *identificador, size_t cantidad, const char *clave, bool automatic) {
    if (identificador) strncpy(g_identificador, identificador, sizeof(g_identificador)-1);
    g_identificador[sizeof(g_identificador)-1] = '\0';
    g_cantidad = cantidad;
    if (clave) strncpy(g_clave, clave, sizeof(g_clave)-1);
    g_clave[sizeof(g_clave)-1] = '\0';
    g_automatic = automatic;
}

const char* app_state_get_identificador(void) { return g_identificador; }
size_t app_state_get_cantidad(void) { return g_cantidad; }
const char* app_state_get_clave(void) { return g_clave; }
bool app_state_get_automatic(void) { return g_automatic; }
