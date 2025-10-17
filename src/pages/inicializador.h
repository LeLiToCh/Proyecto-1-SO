/* ============================================================================
 * inicializador.h — Interfaz de la pantalla de inicialización
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Declarar las funciones para manejar eventos y renderizar la vista donde
 *    se ingresan el identificador, la capacidad y la clave binaria (hasta 9 bits).
 *
 * Puntos clave:
 *  - page_one_handle_event(): gestiona focos, clicks y entrada de texto.
 *  - page_one_render(): dibuja labels, cajas de texto y el botón "Continuar".
 * ========================================================================== */

#pragma once
#include <stdbool.h>
#include <SDL.h>
#include <SDL_ttf.h>

#ifndef INICIALIZADOR_H
#define INICIALIZADOR_H

/**
 * @brief Maneja los eventos de la pantalla Inicializador (inputs/navegación).
 * @param e             Evento SDL entrante.
 * @param out_next_page Índice de la siguiente página cuando corresponde.
 */
void page_one_handle_event(SDL_Event *e, int *out_next_page);

/**
 * @brief Renderiza la pantalla Inicializador (UI de entradas y acciones).
 * @param ren  Renderer de SDL.
 * @param font Fuente TTF para los textos.
 */
void page_one_render(SDL_Renderer *ren, TTF_Font *font);

#endif // INICIALIZADOR_H
