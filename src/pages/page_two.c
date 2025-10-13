#include "page_two.h"

static SDL_Rect back = {20,20,100,40};

void page_two_handle_event(SDL_Event *e, int *out_next_page) {
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x;
        int my = e->button.y;
        if (mx >= back.x && mx <= back.x + back.w && my >= back.y && my <= back.y + back.h) {
            *out_next_page = 0; // PAGE_MAIN
        }
    }
}

void page_two_render(SDL_Renderer *ren, TTF_Font *font) {
    SDL_SetRenderDrawColor(ren, 250,250,250,255);
    SDL_RenderClear(ren);
    SDL_SetRenderDrawColor(ren, 34,139,34,255);
    SDL_RenderFillRect(ren, &back);
    if (font) {
        SDL_Color textColor = {255,255,255,255};
        SDL_Surface *s = TTF_RenderUTF8_Blended(font, "Volver", textColor);
        SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
        int tw, th; SDL_QueryTexture(t, NULL, NULL, &tw, &th);
        SDL_Rect dest = { back.x + (back.w - tw)/2, back.y + (back.h - th)/2, tw, th };
        SDL_RenderCopy(ren, t, NULL, &dest);
        SDL_DestroyTexture(t); SDL_FreeSurface(s);
    }
}
