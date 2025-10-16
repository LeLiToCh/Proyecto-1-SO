// modo_operacion.h
#ifndef PAGE_MAIN_H
#define PAGE_MAIN_H

#include <SDL.h>
#include <SDL_ttf.h>

typedef enum { PAGE_MAIN_ID } PageId;

// event handler needs renderer to compute layout for centering
void page_main_handle_event(SDL_Event *e, SDL_Renderer *ren, int *out_next_page);
void page_main_render(SDL_Renderer *ren, TTF_Font *font);
// Obtener el modo de ejecución seleccionado en la pantalla principal
// Devuelve "Automatico" o "Manual" según el botón pulsado.
const char* page_main_get_execution_mode(void);

#endif // PAGE_MAIN_H
