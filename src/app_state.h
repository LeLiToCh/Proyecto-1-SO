/* ============================================================================
 * app_state.h — Interfaz del estado global de la aplicación
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Declarar la API mínima para leer/escribir el estado compartido entre
 *    pantallas: identificador, cantidad, clave y modo.
 * ========================================================================== */

#pragma once
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Establece el estado global.
 * @param identificador Nombre lógico del espacio de memoria compartida.
 * @param cantidad      Capacidad/slots del buffer circular.
 * @param clave         Cadena binaria (hasta 8 caracteres '0'/'1').
 * @param automatic     true=Automático, false=Manual.
 */
void app_state_set(const char *identificador, size_t cantidad, const char *clave, bool automatic);

/** @brief Devuelve el identificador actual. */
const char* app_state_get_identificador(void);
/** @brief Devuelve la capacidad/slots actual. */
size_t app_state_get_cantidad(void);
/** @brief Devuelve la clave binaria actual (texto). */
const char* app_state_get_clave(void);
/** @brief Devuelve el modo actual (true=Automático, false=Manual). */
bool app_state_get_automatic(void);
