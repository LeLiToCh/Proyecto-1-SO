// page_main.h
#ifndef PAGE_MAIN_H
#define PAGE_MAIN_H

#include <SDL.h>
#include <SDL_ttf.h>

typedef enum { PAGE_MAIN_ID } PageId;

// event handler needs renderer to compute layout for centering
void page_main_handle_event(SDL_Event *e, SDL_Renderer *ren, int *out_next_page);
void page_main_render(SDL_Renderer *ren, TTF_Font *font);

#endif // PAGE_MAIN_H
