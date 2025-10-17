/* ============================================================================
 * nueva_instancia.h — Interfaz de la pantalla "sender"
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Declarar las funciones para manejar eventos y renderizar la vista de envío,
 *    donde se elige el archivo y se inicia el pipeline (receptor + procesador).
 *
 * Puntos clave:
 *  - page_sender_handle_event(): Volver, Nueva Instancia, Buscar e Iniciar.
 *  - page_sender_render(): dibuja la UI del selector de archivo y acciones.
 * ========================================================================== */

#pragma once
#include <SDL.h>
#include <SDL_ttf.h>

/**
 * @brief Maneja eventos de la pantalla "sender" (archivo + arranque).
 * @param e             Evento SDL.
 * @param out_next_page Índice de la página destino cuando corresponde.
 */
void page_sender_handle_event(SDL_Event *e, int *out_next_page);

/**
 * @brief Renderiza la pantalla "sender" (botones, selector y estado).
 * @param ren  Renderer de SDL.
 * @param font Fuente TTF para los textos.
 */
void page_sender_render(SDL_Renderer *ren, TTF_Font *font);
