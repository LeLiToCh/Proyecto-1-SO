/* ============================================================================
 * modo_operacion.c — Pantalla principal (elección de modo de ejecución)
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Presentar dos botones centrados: "Automatico" y "Manual".
 *  - Guardar el modo elegido para que otras pantallas lo consulten.
 *  - Navegar hacia la página de inicialización (PAGE_ONE) tras la elección.
 *
 * Puntos clave:
 *  - compute_layout() calcula posiciones en función del tamaño del renderer.
 *  - page_main_handle_event() detecta clicks y decide navegación.
 *  - page_main_render() dibuja botones, título y subtítulo con leve sombra.
 *
 * Notas:
 *  - page_main_get_execution_mode() devuelve "Automatico" por defecto cuando
 *    aún no se eligió explícitamente.
 * ========================================================================== */

#include "modo_operacion.h"
#include <string.h>

/* Calcularemos posiciones de botones según el viewport del renderer */
static SDL_Rect btn1, btn2;
/* Modo de ejecución elegido: "Automatico" o "Manual" (cadena estática) */
static const char* g_execution_mode = ""; // "Automatico" o "Manual"

/**
 * @brief Calcula layout centrado para los botones en el área del renderer.
 */
static void compute_layout(SDL_Renderer *ren) {
    int w, h;
    SDL_GetRendererOutputSize(ren, &w, &h);
    int bw = 180, bh = 70; // tamaño de botón
    int gap = 40;
    int total_width = bw * 2 + gap;
    int start_x = (w - total_width) / 2;
    int y = h/2 - bh/2;
    btn1.x = start_x; btn1.y = y; btn1.w = bw; btn1.h = bh;
    btn2.x = start_x + bw + gap; btn2.y = y; btn2.w = bw; btn2.h = bh;
}

/**
 * @brief Maneja la interacción en la pantalla principal.
 * @param e             Evento SDL.
 * @param ren           Renderer (para refrescar layout).
 * @param out_next_page Salida con el índice de la siguiente página.
 *
 * Click izquierdo:
 *  - Botón izquierdo -> "Automatico" y navegar a PAGE_ONE.
 *  - Botón derecho   -> "Manual"     y navegar a PAGE_ONE.
 */
void page_main_handle_event(SDL_Event *e, SDL_Renderer *ren, int *out_next_page) {
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        // Asegurar layout actualizado antes de probar hit-testing
        compute_layout(ren);
        int mx = e->button.x;
        int my = e->button.y;
        if (mx >= btn1.x && mx <= btn1.x + btn1.w && my >= btn1.y && my <= btn1.y + btn1.h) {
            g_execution_mode = "Automatico";
            *out_next_page = 1; // Ir a PAGE_ONE (Inicializador)
        } else if (mx >= btn2.x && mx <= btn2.x + btn2.w && my >= btn2.y && my <= btn2.y + btn2.h) {
            g_execution_mode = "Manual";
            *out_next_page = 1; // Ir a PAGE_ONE
        }
    }
}

/**
 * @brief Dibuja la pantalla principal: encabezados y botones de modo.
 * @param ren  Renderer de SDL.
 * @param font Fuente TTF para los textos.
 */
void page_main_render(SDL_Renderer *ren, TTF_Font *font) {
    compute_layout(ren);

    int w, h;
    SDL_GetRendererOutputSize(ren, &w, &h);

    // Dibujar botones primero (fondos)
    SDL_SetRenderDrawColor(ren, 100, 149, 237, 255); // botón izquierdo
    SDL_RenderFillRect(ren, &btn1);
    SDL_SetRenderDrawColor(ren, 72, 61, 139, 255);   // botón derecho (más oscuro)
    SDL_RenderFillRect(ren, &btn2);

    if (font) {
        // Encabezado centrado arriba
        SDL_Color headerColor = {30,30,30,255};
        SDL_Surface *hs = TTF_RenderUTF8_Blended(font, "Comunicacion de procesos sincronizada", headerColor);
        if (hs) {
            SDL_Texture *ths = SDL_CreateTextureFromSurface(ren, hs);
            int tw, tht; SDL_QueryTexture(ths, NULL, NULL, &tw, &tht);
            SDL_Rect dest = { (w - tw)/2, 40, tw, tht };
            SDL_RenderCopy(ren, ths, NULL, &dest);
            SDL_DestroyTexture(ths);
            SDL_FreeSurface(hs);
        }

        // Subtítulo centrado con sombra sutil
        SDL_Color subColor = {80,80,80,255};
        SDL_Color shadowColor = {0,0,0,255};
        SDL_Surface *ss = TTF_RenderUTF8_Blended(font, "Modos de ejecucion", subColor);
        if (ss) {
            SDL_Texture *tss = SDL_CreateTextureFromSurface(ren, ss);
            int tw, tht; SDL_QueryTexture(tss, NULL, NULL, &tw, &tht);
            SDL_Rect dest = { (w - tw)/2, btn1.y - 40, tw, tht };
            // sombra
            SDL_Surface *sssh = TTF_RenderUTF8_Blended(font, "Modos de ejecucion", shadowColor);
            if (sssh) {
                SDL_Texture *tssh = SDL_CreateTextureFromSurface(ren, sssh);
                SDL_Rect shad = { dest.x + 1, dest.y + 1, tw, tht };
                SDL_RenderCopy(ren, tssh, NULL, &shad);
                SDL_DestroyTexture(tssh);
                SDL_FreeSurface(sssh);
            }
            SDL_RenderCopy(ren, tss, NULL, &dest);
            SDL_DestroyTexture(tss);
            SDL_FreeSurface(ss);
        }

        // Etiquetas de cada botón con sombra ligera
        SDL_Color btnText = {255,255,255,255};
        SDL_Color sh = {0,0,0,255};

        // "Automatico"
        SDL_Surface *sa_sh = TTF_RenderUTF8_Blended(font, "Automatico", sh);
        SDL_Surface *sa = TTF_RenderUTF8_Blended(font, "Automatico", btnText);
        if (sa && sa_sh) {
            SDL_Texture *tash = SDL_CreateTextureFromSurface(ren, sa_sh);
            SDL_Texture *ta = SDL_CreateTextureFromSurface(ren, sa);
            int tw, tht; SDL_QueryTexture(ta, NULL, NULL, &tw, &tht);
            SDL_Rect dest = { btn1.x + (btn1.w - tw)/2, btn1.y + (btn1.h - tht)/2, tw, tht };
            SDL_Rect destsh = { dest.x + 1, dest.y + 1, tw, tht };
            SDL_RenderCopy(ren, tash, NULL, &destsh);
            SDL_RenderCopy(ren, ta, NULL, &dest);
            SDL_DestroyTexture(tash); SDL_DestroyTexture(ta);
        }
        if (sa) SDL_FreeSurface(sa);
        if (sa_sh) SDL_FreeSurface(sa_sh);

        // "Manual"
        SDL_Surface *sm_sh = TTF_RenderUTF8_Blended(font, "Manual", sh);
        SDL_Surface *sm = TTF_RenderUTF8_Blended(font, "Manual", btnText);
        if (sm && sm_sh) {
            SDL_Texture *tmsh = SDL_CreateTextureFromSurface(ren, sm_sh);
            SDL_Texture *tm = SDL_CreateTextureFromSurface(ren, sm);
            int tw, tht; SDL_QueryTexture(tm, NULL, NULL, &tw, &tht);
            SDL_Rect dest = { btn2.x + (btn2.w - tw)/2, btn2.y + (btn2.h - tht)/2, tw, tht };
            SDL_Rect destsh = { dest.x + 1, dest.y + 1, tw, tht };
            SDL_RenderCopy(ren, tmsh, NULL, &destsh);
            SDL_RenderCopy(ren, tm, NULL, &dest);
            SDL_DestroyTexture(tmsh); SDL_DestroyTexture(tm);
        }
        if (sm) SDL_FreeSurface(sm);
        if (sm_sh) SDL_FreeSurface(sm_sh);
    }
}

/**
 * @brief Devuelve el modo elegido; por defecto "Automatico".
 */
const char* page_main_get_execution_mode(void) {
    return g_execution_mode && g_execution_mode[0] ? g_execution_mode : "Automatico"; // default
}
