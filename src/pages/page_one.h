// page_one.h
#pragma once
#include <stdbool.h>
#include <SDL.h>
#include <SDL_ttf.h>

#ifndef PAGE_ONE_H
#define PAGE_ONE_H

#include <SDL.h>
#include <SDL_ttf.h>

void page_one_handle_event(SDL_Event *e, int *out_next_page);
void page_one_render(SDL_Renderer *ren, TTF_Font *font);

#endif // PAGE_ONE_H
