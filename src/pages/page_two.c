#include "page_two.h"

void page_two_handle_event(SDL_Event *e, int *out_next_page) {
    (void)e; (void)out_next_page;
}

void page_two_render(SDL_Renderer *ren, TTF_Font *font) {
    (void)font;
    // Simple placeholder screen background
    SDL_SetRenderDrawColor(ren, 200, 230, 200, 255);
    SDL_RenderClear(ren);
}
