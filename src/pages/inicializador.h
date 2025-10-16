// inicializador.h
#pragma once
#include <stdbool.h>
#include <SDL.h>
#include <SDL_ttf.h>

#ifndef INICIALIZADOR_H
#define INICIALIZADOR_H

void page_one_handle_event(SDL_Event *e, int *out_next_page);
void page_one_render(SDL_Renderer *ren, TTF_Font *font);

#endif // INICIALIZADOR_H
