#include "page_sender.h"
#include "processor.h"
#include "app_state.h"
#include "memory.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

static char file_path[512] = "";

// helper: open native file dialog into out buffer (UTF-8). Returns 1 on success.
static int open_file_dialog_local(char *out, size_t outlen) {
#ifdef _WIN32
    WCHAR wbuf[MAX_PATH] = {0};
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = wbuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        int needed = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
        if (needed > 0 && (size_t)needed <= outlen) {
            WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, out, (int)outlen, NULL, NULL);
            return 1;
        }
    }
    return 0;
#else
    FILE *f = popen("zenity --file-selection 2>/dev/null", "r");
    if (!f) return 0;
    char buf[1024];
    if (fgets(buf, sizeof(buf), f) != NULL) {
        size_t L = strlen(buf);
        if (L && buf[L-1] == '\n') buf[L-1] = '\0';
        strncpy(out, buf, outlen-1); out[outlen-1] = '\0';
        pclose(f);
        return 1;
    }
    pclose(f);
    return 0;
#endif
}

void page_sender_handle_event(SDL_Event *e, int *out_next_page) {
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
    SDL_Rect back = {20,20,100,40};
    SDL_Rect fpbox = {120,490,480,30};
    SDL_Rect search_btn = { fpbox.x + fpbox.w + 8, fpbox.y, 80, fpbox.h };
    SDL_Rect iniciar_btn = {200, 180, 160, 50};

        if (mx >= back.x && mx <= back.x + back.w && my >= back.y && my <= back.y + back.h) {
            // go back to inicializador
            *out_next_page = 1; // PAGE_ONE
            return;
        }

        if (mx >= search_btn.x && mx <= search_btn.x + search_btn.w && my >= search_btn.y && my <= search_btn.y + search_btn.h) {
            // open native file dialog
            if (open_file_dialog_local(file_path, sizeof(file_path))) {
                // selected
            }
        } else if (mx >= iniciar_btn.x && mx <= iniciar_btn.x + iniciar_btn.w && my >= iniciar_btn.y && my <= iniciar_btn.y + iniciar_btn.h) {
            // Iniciar: initialize memory then start processing
            const char *ident = app_state_get_identificador();
            size_t cantidad = app_state_get_cantidad();
            const char *clave = app_state_get_clave();
            bool automatic = app_state_get_automatic();
            if (strlen(clave) != 8) {
                printf("[ERROR] Clave invalida: debe tener 8 bits\n");
                return;
            }
            if (!file_path[0]) {
                printf("[AVISO] No selecciono archivo para procesar.\n");
                return;
            }
            if (cantidad == 0) cantidad = 1;
            bool created = false;
            bool ok = memory_init_shared(ident[0] ? ident : "mem", cantidad, &created);
            printf("Memoria compartida %s (capacidad=%zu): %s\n", created ? "creada" : "adjunta", memory_capacity(), ok ? "OK" : "ERROR");
            processor_start_async(file_path, clave, automatic);
        }
    }
}

void page_sender_render(SDL_Renderer *ren, TTF_Font *font) {
    SDL_SetRenderDrawColor(ren, 245,245,245,255);
    SDL_RenderClear(ren);
    // render back button
    SDL_Rect back = {20,20,100,40};
    SDL_SetRenderDrawColor(ren, 70,130,180,255);
    SDL_RenderFillRect(ren, &back);
    if (font) {
        SDL_Surface *sv = TTF_RenderUTF8_Blended(font, "Volver", (SDL_Color){255,255,255,255});
        SDL_Texture *tv = SDL_CreateTextureFromSurface(ren, sv);
        int tvw, tvh; SDL_QueryTexture(tv, NULL, NULL, &tvw, &tvh);
        SDL_Rect dv = { back.x + (back.w - tvw)/2, back.y + (back.h - tvh)/2, tvw, tvh };
        SDL_RenderCopy(ren, tv, NULL, &dv);
        SDL_DestroyTexture(tv); SDL_FreeSurface(sv);
    }

    // file selector label and box (moved from page_one)
    SDL_Rect fp_label = {120,460,300,20};
    SDL_Rect fpbox = {120,490,480,30};
    SDL_Rect search_btn = { fpbox.x + fpbox.w + 8, fpbox.y, 80, fpbox.h };
    if (font) {
        SDL_Color col = {0,0,0,255};
        SDL_Surface *flab = TTF_RenderUTF8_Blended(font, "Selecciona tu archivo para procesar", col);
        SDL_Texture *tflab = SDL_CreateTextureFromSurface(ren, flab);
        int tw, th; SDL_QueryTexture(tflab, NULL, NULL, &tw, &th);
        SDL_Rect d4 = { fp_label.x, fp_label.y, tw, th };
        SDL_RenderCopy(ren, tflab, NULL, &d4);
        SDL_DestroyTexture(tflab); SDL_FreeSurface(flab);
    }
    SDL_SetRenderDrawColor(ren, 255,255,255,255);
    SDL_RenderFillRect(ren, &fpbox);
    SDL_SetRenderDrawColor(ren, 200,200,200,255);
    SDL_RenderDrawRect(ren, &fpbox);
    // search button
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
    // show selected file inside box
    if (file_path[0] && font) {
        SDL_Color col = {0,0,0,255};
        SDL_Surface *fp = TTF_RenderUTF8_Blended(font, file_path, col);
        SDL_Texture *tfp = SDL_CreateTextureFromSurface(ren, fp);
        int tw, th; SDL_QueryTexture(tfp, NULL, NULL, &tw, &th);
        SDL_Rect dfp = { fpbox.x + 6, fpbox.y + (fpbox.h - th)/2, tw, th };
        SDL_RenderCopy(ren, tfp, NULL, &dfp);
        SDL_DestroyTexture(tfp); SDL_FreeSurface(fp);
    }

    // iniciar button (single Volver is the top back button)
    SDL_Rect iniciar_btn = {200, 180, 160, 50};
    SDL_SetRenderDrawColor(ren, 34,139,34,255);
    SDL_RenderFillRect(ren, &iniciar_btn);
    if (font) {
        SDL_Surface *cont = TTF_RenderUTF8_Blended(font, "Iniciar", (SDL_Color){255,255,255,255});
        SDL_Texture *tcont = SDL_CreateTextureFromSurface(ren, cont);
        int tw, th; SDL_QueryTexture(tcont, NULL, NULL, &tw, &th);
        SDL_Rect dcont = { iniciar_btn.x + (iniciar_btn.w - tw)/2, iniciar_btn.y + (iniciar_btn.h - th)/2, tw, th };
        SDL_RenderCopy(ren, tcont, NULL, &dcont);
        SDL_DestroyTexture(tcont); SDL_FreeSurface(cont);
    }
}
