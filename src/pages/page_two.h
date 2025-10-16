#pragma once
#include <SDL.h>
#include <SDL_ttf.h>

void page_two_handle_event(SDL_Event *e, int *out_next_page);
void page_two_render(SDL_Renderer *ren, TTF_Font *font);
