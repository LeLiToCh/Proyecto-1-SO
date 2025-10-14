#include "sender_window.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>
#include <string.h>
#include "processor.h"
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

// Simple sender window: shows file browser and process button
#include <stdio.h>
#define MAX_PATH_LEN 512

static char file_path[MAX_PATH_LEN] = "";
static char clave_text[9] = "";
static bool clave_active = false;

void sender_window_start_async(const char *identificador, int cantidad, const char *clave, bool automatic) {
    SDL_Window *win = SDL_CreateWindow("Emisor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 600, 300, SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    TTF_Font *font = TTF_OpenFont("font.ttf", 20);
    if (!font) { printf("No se pudo cargar la fuente\n"); }

    SDL_Rect clave_rect = {40, 40, 220, 40};
    SDL_Rect fpbox = {40, 100, 400, 40};
    SDL_Rect search_btn = {fpbox.x + fpbox.w + 10, fpbox.y, 80, fpbox.h};
    SDL_Rect process_btn = {200, 180, 160, 50};

    int running = 1;
    SDL_StartTextInput();
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = e.button.x, my = e.button.y;
                if (mx >= clave_rect.x && mx <= clave_rect.x + clave_rect.w && my >= clave_rect.y && my <= clave_rect.y + clave_rect.h) {
                    clave_active = true;
                    SDL_StartTextInput();
                } else if (mx >= fpbox.x && mx <= fpbox.x + fpbox.w && my >= fpbox.y && my <= fpbox.y + fpbox.h) {
                    // No text input for file path
                    clave_active = false;
                    SDL_StopTextInput();
                } else if (mx >= search_btn.x && mx <= search_btn.x + search_btn.w && my >= search_btn.y && my <= search_btn.y + search_btn.h) {
                    // Abrir dialogo nativo
                    #ifdef _WIN32
                    WCHAR wbuf[MAX_PATH_LEN] = {0};
                    OPENFILENAMEW ofn;
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = NULL;
                    ofn.lpstrFile = wbuf;
                    ofn.nMaxFile = MAX_PATH_LEN;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (GetOpenFileNameW(&ofn)) {
                        int needed = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
                        if (needed > 0 && needed <= MAX_PATH_LEN) {
                            WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, file_path, MAX_PATH_LEN, NULL, NULL);
                        }
                    }
                    #else
                    FILE *f = popen("zenity --file-selection 2>/dev/null", "r");
                    if (f) {
                        char buf[MAX_PATH_LEN];
                        if (fgets(buf, sizeof(buf), f) != NULL) {
                            size_t L = strlen(buf);
                            if (L && buf[L-1] == '\n') buf[L-1] = '\0';
                            strncpy(file_path, buf, MAX_PATH_LEN-1); file_path[MAX_PATH_LEN-1] = '\0';
                        }
                        pclose(f);
                    }
                    #endif
                } else if (mx >= process_btn.x && mx <= process_btn.x + process_btn.w && my >= process_btn.y && my <= process_btn.y + process_btn.h) {
                    // Procesar solo si clave válida y archivo seleccionado
                    if (strlen(clave_text) == 8 && file_path[0]) {
                        processor_start_async(file_path, clave_text, automatic);
                    }
                } else {
                    clave_active = false;
                    SDL_StopTextInput();
                }
            } else if (e.type == SDL_TEXTINPUT && clave_active) {
                const char *txt = e.text.text;
                for (size_t i=0; txt[i] != '\0'; ++i) {
                    if ((txt[i] == '0' || txt[i] == '1') && strlen(clave_text) < 8) {
                        size_t L = strlen(clave_text);
                        clave_text[L] = txt[i];
                        clave_text[L+1] = '\0';
                    }
                }
            } else if (e.type == SDL_KEYDOWN && clave_active) {
                if (e.key.keysym.sym == SDLK_BACKSPACE && strlen(clave_text)>0) {
                    clave_text[strlen(clave_text)-1] = '\0';
                }
            }
        }
        // Render
        SDL_SetRenderDrawColor(ren, 245,245,245,255);
        SDL_RenderClear(ren);
        // Clave label y box
        if (font) {
            SDL_Color col = {0,0,0,255};
            SDL_Surface *lab = TTF_RenderUTF8_Blended(font, "Clave de 8 bits (solo 0 y 1)", col);
            SDL_Texture *tlab = SDL_CreateTextureFromSurface(ren, lab);
            int tw, th; SDL_QueryTexture(tlab, NULL, NULL, &tw, &th);
            SDL_Rect d = { clave_rect.x, clave_rect.y - 22, tw, th };
            SDL_RenderCopy(ren, tlab, NULL, &d);
            SDL_DestroyTexture(tlab); SDL_FreeSurface(lab);
        }
        SDL_SetRenderDrawColor(ren, 255,255,255,255);
        SDL_RenderFillRect(ren, &clave_rect);
        if (clave_active) SDL_SetRenderDrawColor(ren, 30,144,255,255); else SDL_SetRenderDrawColor(ren, 200,200,200,255);
        SDL_RenderDrawRect(ren, &clave_rect);
        if (strlen(clave_text) > 0 && font) {
            SDL_Color col = {0,0,0,255};
            SDL_Surface *txt = TTF_RenderUTF8_Blended(font, clave_text, col);
            SDL_Texture *ttxt = SDL_CreateTextureFromSurface(ren, txt);
            int tw, th; SDL_QueryTexture(ttxt, NULL, NULL, &tw, &th);
            SDL_Rect d = { clave_rect.x + 6, clave_rect.y + (clave_rect.h - th)/2, tw, th };
            SDL_RenderCopy(ren, ttxt, NULL, &d);
            SDL_DestroyTexture(ttxt); SDL_FreeSurface(txt);
        }
        // File path label y box
        if (font) {
            SDL_Color col = {0,0,0,255};
            SDL_Surface *lab = TTF_RenderUTF8_Blended(font, "Selecciona tu archivo para procesar", col);
            SDL_Texture *tlab = SDL_CreateTextureFromSurface(ren, lab);
            int tw, th; SDL_QueryTexture(tlab, NULL, NULL, &tw, &th);
            SDL_Rect d = { fpbox.x, fpbox.y - 22, tw, th };
            SDL_RenderCopy(ren, tlab, NULL, &d);
            SDL_DestroyTexture(tlab); SDL_FreeSurface(lab);
        }
        SDL_SetRenderDrawColor(ren, 255,255,255,255);
        SDL_RenderFillRect(ren, &fpbox);
        SDL_SetRenderDrawColor(ren, 200,200,200,255);
        SDL_RenderDrawRect(ren, &fpbox);
        if (strlen(file_path) > 0 && font) {
            SDL_Color col = {0,0,0,255};
            SDL_Surface *txt = TTF_RenderUTF8_Blended(font, file_path, col);
            SDL_Texture *ttxt = SDL_CreateTextureFromSurface(ren, txt);
            int tw, th; SDL_QueryTexture(ttxt, NULL, NULL, &tw, &th);
            SDL_Rect d = { fpbox.x + 6, fpbox.y + (fpbox.h - th)/2, tw, th };
            SDL_RenderCopy(ren, ttxt, NULL, &d);
            SDL_DestroyTexture(ttxt); SDL_FreeSurface(txt);
        }
        // Search button
        SDL_SetRenderDrawColor(ren, 100,149,237,255);
        SDL_RenderFillRect(ren, &search_btn);
        if (font) {
            SDL_Surface *sbtn = TTF_RenderUTF8_Blended(font, "Buscar", (SDL_Color){255,255,255,255});
            SDL_Texture *tsbtn = SDL_CreateTextureFromSurface(ren, sbtn);
            int tw, th; SDL_QueryTexture(tsbtn, NULL, NULL, &tw, &th);
            SDL_Rect ds = { search_btn.x + (search_btn.w - tw)/2, search_btn.y + (search_btn.h - th)/2, tw, th };
            SDL_RenderCopy(ren, tsbtn, NULL, &ds);
            SDL_DestroyTexture(tsbtn); SDL_FreeSurface(sbtn);
        }
        // Process button
        SDL_SetRenderDrawColor(ren, 34,139,34,255);
        SDL_RenderFillRect(ren, &process_btn);
        if (font) {
            SDL_Surface *cont = TTF_RenderUTF8_Blended(font, "Procesar", (SDL_Color){255,255,255,255});
            SDL_Texture *tcont = SDL_CreateTextureFromSurface(ren, cont);
            int tw, th; SDL_QueryTexture(tcont, NULL, NULL, &tw, &th);
            SDL_Rect dcont = { process_btn.x + (process_btn.w - tw)/2, process_btn.y + (process_btn.h - th)/2, tw, th };
            SDL_RenderCopy(ren, tcont, NULL, &dcont);
            SDL_DestroyTexture(tcont); SDL_FreeSurface(cont);
        }
        SDL_RenderPresent(ren);
    }
    SDL_StopTextInput();
    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
}
