#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>

#include "pages/page_main.h"
#include "pages/page_one.h"
#include "pages/page_two.h"

#define WINDOW_W 800
#define WINDOW_H 600

typedef enum { PAGE_MAIN, PAGE_ONE, PAGE_TWO } Page;

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("App con 2 botones", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "CreateWindow Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        fprintf(stderr, "CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Load font - expects "font.ttf" in current working directory
    TTF_Font *font = TTF_OpenFont("font.ttf", 24);
    if (!font) {
        fprintf(stderr, "Failed to open font.ttf: %s\n", TTF_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event e;
    Page page = PAGE_MAIN;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else {
                int next = -1;
                if (page == PAGE_MAIN) page_main_handle_event(&e, ren, &next);
                else if (page == PAGE_ONE) page_one_handle_event(&e, &next);
                else if (page == PAGE_TWO) page_two_handle_event(&e, &next);
                if (next >= 0) page = (Page)next;
            }
        }

        // Render
        SDL_SetRenderDrawColor(ren, 220, 220, 220, 255);
        SDL_RenderClear(ren);

        if (page == PAGE_MAIN) {
            page_main_render(ren, font);
        } else if (page == PAGE_ONE) {
            page_one_render(ren, font);
        } else if (page == PAGE_TWO) {
            page_two_render(ren, font);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(10);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
