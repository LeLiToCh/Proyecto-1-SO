/* ============================================================================
 * modo_operacion.h — Menú principal para seleccionar modo de ejecución
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Ofrecer dos opciones: "Automatico" y "Manual", y recordar la elección.
 *
 * Puntos clave:
 *  - page_main_handle_event(): detecta clicks y decide navegación.
 *  - page_main_render(): dibuja encabezados y botones centrados.
 *  - page_main_get_execution_mode(): devuelve el modo elegido.
 * ========================================================================== */

#ifndef PAGE_MAIN_H
#define PAGE_MAIN_H

#include <SDL.h>
#include <SDL_ttf.h>

/* Identificador de esta pantalla (por claridad en control de navegación) */
typedef enum { PAGE_MAIN_ID } PageId;

/**
 * @brief Maneja los eventos del menú principal (elección de modo).
 * @param e             Evento SDL.
 * @param ren           Renderer para cálculos de layout.
 * @param out_next_page Índice de la siguiente página cuando se elige un modo.
 */
void page_main_handle_event(SDL_Event *e, SDL_Renderer *ren, int *out_next_page);

/**
 * @brief Renderiza el menú principal con los botones de modo.
 * @param ren  Renderer de SDL.
 * @param font Fuente TTF para los textos.
 */
void page_main_render(SDL_Renderer *ren, TTF_Font *font);

/**
 * @brief Obtiene el modo de ejecución seleccionado en la pantalla principal.
 * @return "Automatico" o "Manual" según el último botón pulsado (por defecto "Automatico").
 */
const char* page_main_get_execution_mode(void);

#endif // PAGE_MAIN_H
