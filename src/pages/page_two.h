// page_two.h
#ifndef PAGE_TWO_H
#define PAGE_TWO_H

#include <SDL.h>
#include <SDL_ttf.h>

void page_two_handle_event(SDL_Event *e, int *out_next_page);
void page_two_render(SDL_Renderer *ren, TTF_Font *font);

#endif // PAGE_TWO_H
