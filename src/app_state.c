/* ============================================================================
 * app_state.c — Estado global de la aplicación (identificador, capacidad, clave, modo)
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Mantener en memoria el estado elegido en pantallas previas para que
 *    otras vistas/módulos lo consulten (identificador de memoria compartida,
 *    cantidad/capacidad, clave binaria y modo automático/manual).
 *
 * Puntos clave:
 *  - Variables estáticas a nivel de módulo (alcance interno).
 *  - API de setters/getters trivial y sin asignaciones dinámicas.
 *  - Se asegura terminación de cadenas con '\0' (strncpy + null final).
 *
 * Notas:
 *  - No es thread-safe por diseño (se asume uso desde el hilo principal de UI).
 *  - 'clave' se almacena hasta 8 caracteres útiles (buffer de 9 con terminador).
 * ========================================================================== */

#include "app_state.h"
#include <string.h>
#include <stdlib.h>

/* --------------------------- Estado global interno ------------------------ */
static char g_identificador[256] = "";
static size_t g_cantidad = 1;
static char g_clave[9] = "";
static bool g_automatic = true;

/**
 * @brief Establece el estado global de la aplicación.
 * @param identificador Nombre lógico del espacio de memoria compartida.
 * @param cantidad      Capacidad/slots del buffer circular.
 * @param clave         Cadena binaria (hasta 8 caracteres '0'/'1').
 * @param automatic     true=Automático, false=Manual.
 * @note Las cadenas se copian con límite y se garantiza el terminador nulo.
 */
void app_state_set(const char *identificador, size_t cantidad, const char *clave, bool automatic) {
    if (identificador) strncpy(g_identificador, identificador, sizeof(g_identificador)-1);
    g_identificador[sizeof(g_identificador)-1] = '\0';
    g_cantidad = cantidad;
    if (clave) strncpy(g_clave, clave, sizeof(g_clave)-1);
    g_clave[sizeof(g_clave)-1] = '\0';
    g_automatic = automatic;
}

/** @brief Obtiene el identificador configurado. */
const char* app_state_get_identificador(void) { return g_identificador; }
/** @brief Obtiene la capacidad/slots configurada. */
size_t app_state_get_cantidad(void) { return g_cantidad; }
/** @brief Obtiene la clave binaria configurada (hasta 8 bits como texto). */
const char* app_state_get_clave(void) { return g_clave; }
/** @brief Indica si el modo actual es automático (true) o manual (false). */
bool app_state_get_automatic(void) { return g_automatic; }
