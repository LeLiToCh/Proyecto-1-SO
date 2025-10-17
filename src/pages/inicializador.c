/* ============================================================================
 * inicializador.c — Pantalla de inicialización (inputs + preparación de estado)
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Recoger parámetros del usuario: identificador del espacio compartido,
 *    cantidad (capacidad del buffer), y la clave binaria (hasta 9 bits).
 *  - Preparar el estado global de la app y navegar hacia la pantalla "sender".
 *
 * Puntos clave:
 *  - Los campos de texto cambian foco según el click y usan SDL_StartTextInput.
 *  - El botón "Continuar" valida, inicializa memoria compartida y guarda
 *    el estado vía app_state_set(...), luego cambia a PAGE_SENDER.
 *  - Se fuerza un mínimo de 1 para 'cantidad'; se imprime información de
 *    depuración por stdout.
 *  - reset_page_state() limpia entradas y borra memoria (memory_clear()).
 *
 * Notas:
 *  - En Windows, open_file_dialog() usa el diálogo nativo (UTF-16/UTF-8).
 *  - En Linux/macOS, usa 'zenity' como alternativa simple vía popen().
 *  - 'continue_disabled' evita múltiples inicializaciones por doble click.
 * ========================================================================== */

// Inicializador page: collects inputs and prints them when "Continuar" is clicked
#include <stdbool.h>   // <-- debe ir PRIMERO de todo
#include "inicializador.h"
#include "modo_operacion.h" // para obtener modo seleccionado en pantalla principal
#include "memory.h"    // backend de memoria
#include "processor.h" // procesado de archivo a memoria
#include "receptor.h"
#include "finalizador.h"
#include "app_state.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif


/* -------------------------- Elementos de UI fijos ------------------------- */
static SDL_Rect back = {20,20,100,40};
static SDL_Rect continue_btn = {0,0,160,50};
/* Se deshabilita tras la primera carga para evitar reprocesos */
static bool continue_disabled = false;

/* ------------------------------ Estado de inputs -------------------------- */
#define MAX_TEXT 256
static char identificador[MAX_TEXT] = "";
static bool identificador_active = false;
/* Ruta del archivo (declarada temprano para uso en eventos/render) */
static char filepath[MAX_TEXT] = "";
static bool filepath_active = false;

static int cantidad = 1;

/* Clave de 9 bits: campo de texto binario (0/1), máx 9 dígitos */
static SDL_Rect mode_rect = {0,0,220,30};
static char mode_text[16] = ""; /* almacenará hasta 9 dígitos '0'/'1' */
static bool mode_active = false;

/**
 * @brief Restablece por completo el estado de la página.
 *        - Limpia textos/focos.
 *        - Habilita nuevamente el botón "Continuar".
 *        - Detiene captura de texto.
 *        - Limpia memoria (memory_clear()) para no acumular residuos.
 */
static void reset_page_state(void) {
    identificador[0] = '\0';
    filepath[0] = '\0';
    mode_text[0] = '\0';
    cantidad = 1;
    identificador_active = filepath_active = mode_active = false;
    continue_disabled = false;
    SDL_StopTextInput();
    // Limpiar memoria para que no se guarde nada
    memory_clear();
}

/**
 * @brief Abre un diálogo nativo de archivo y devuelve la ruta UTF-8 en 'out'.
 * @param out    Buffer de salida.
 * @param outlen Tamaño del buffer de salida.
 * @return 1 si el usuario seleccionó archivo, 0 en caso contrario.
 */
static int open_file_dialog(char *out, size_t outlen) {
#ifdef _WIN32
    // Usar GetOpenFileNameW y convertir a UTF-8
    WCHAR wbuf[MAX_PATH] = {0};
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = wbuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        // Convertir a UTF-8
        int needed = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
        if (needed > 0 && (size_t)needed <= outlen) {
            WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, out, (int)outlen, NULL, NULL);
            return 1;
        }
    }
    return 0;
#else
    // Alternativa simple: usar zenity (Linux) para seleccionar archivo
    FILE *f = popen("zenity --file-selection 2>/dev/null", "r");
    if (!f) return 0;
    char buf[1024];
    if (fgets(buf, sizeof(buf), f) != NULL) {
        // Quitar salto de línea final si existe
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

/* Ruta de archivo (comentada arriba) */

/**
 * @brief Helper para detección de click dentro de un rectángulo.
 */
static bool pt_in_rect(int x, int y, SDL_Rect *r) {
    return x >= r->x && x <= r->x + r->w && y >= r->y && y <= r->y + r->h;
}

/**
 * @brief Manejador de eventos de la página Inicializador.
 * @param e               Evento SDL recibido.
 * @param out_next_page   Escribe el índice de la próxima página cuando navega.
 *
 * Flujo:
 *  - Click en "Volver": limpia estado y va a PAGE_MAIN.
 *  - Click en "Continuar": valida, inicializa memoria compartida, guarda
 *    estado de app y navega a PAGE_SENDER.
 *  - Clicks en campos: actualiza foco e inicia/detiene captura de texto.
 *  - Botones up/down: ajustan 'cantidad' por saltos de 50.
 *  - Entrada de texto: agrega dígitos válidos (identificador y clave 0/1).
 */
void page_one_handle_event(SDL_Event *e, int *out_next_page) {
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        /* Alinear rect de "Continuar" con el layout de render para que el
           primer click sea efectivo (evita doble click) */
        continue_btn.x = 120; continue_btn.y = 540; continue_btn.w = 160; continue_btn.h = 50;
        int mx = e->button.x;
        int my = e->button.y;
        if (pt_in_rect(mx,my,&back)) {
            // Restablecer estado e ir a la pantalla principal
            reset_page_state();
            *out_next_page = 0; // PAGE_MAIN
            return;
        }

        if (pt_in_rect(mx,my,&continue_btn) && !continue_disabled) {
            /* Imprimir valores en stdout para depuración */
            printf("Identificador: %s\n", identificador);
            printf("Cantidad: %d\n", cantidad);
            /* Inicializar memoria compartida con 'identificador' y 'cantidad' */
            if (cantidad <= 0) cantidad = 1; // validar
            bool created = false;
            bool ok = memory_init_shared(identificador[0] ? identificador : "mem", (size_t)cantidad, &created);
            printf("Memoria compartida %s (capacidad=%zu): %s\n",
                   created ? "creada" : "adjunta",
                   memory_capacity(), ok ? "OK" : "ERROR");

            /* Mostrar modo elegido en la pantalla principal (botones) */
            const char *exec_mode_main = page_main_get_execution_mode();
            printf("Modo de ejecucion (inicio): %s\n", exec_mode_main);

            /* Mostrar clave binaria (hasta 9 bits) ingresada */
            printf("Clave de 9 bits: %s\n", mode_text[0] ? mode_text : "(vacia)");
            printf("Archivo: %s\n", filepath);
            fflush(stdout);

            /* Guardar estado y navegar a PAGE_SENDER */
            bool automatic = true;
            if (exec_mode_main && (strcmp(exec_mode_main, "Manual") == 0)) automatic = false;
            app_state_set(identificador, (size_t)cantidad, mode_text, automatic);
            *out_next_page = 3; // PAGE_SENDER
            continue_disabled = true;
            return;
        }

        /* Coordenadas que deben coincidir con el render para el foco */
        SDL_Rect idbox = {120,120,520,40};
        /* cantidad_box no se usa aquí, solo en render */
        SDL_Rect up = {660,210,30,30};
        SDL_Rect down = {660,250,30,30};
        /* Selector de archivo se movió a la página sender */
        SDL_Rect fpbox = {0,0,0,0};
        SDL_Rect search_btn = {0,0,0,0};
        /* Campo de modo (coincidir con render) */
        mode_rect.x = 120; mode_rect.y = 280; mode_rect.w = 300; mode_rect.h = 30;

        if (pt_in_rect(mx,my,&idbox)) {
            identificador_active = true;
            filepath_active = false;
            mode_active = false;
            SDL_StartTextInput();
        } else if (pt_in_rect(mx,my,&mode_rect)) {
            // Foco en textfield de modo (solo acepta 0/1, hasta 9)
            mode_active = true;
            identificador_active = false;
            filepath_active = false;
            SDL_StartTextInput();
        } else {
            // Click fuera -> detener captura
            if (identificador_active || filepath_active || mode_active) SDL_StopTextInput();
            identificador_active = false;
            filepath_active = false;
            mode_active = false;
        }

        /* Ajuste de 'cantidad' con flechas (±50) y límites convenientes */
        if (pt_in_rect(mx,my,&up)) {
            cantidad += 50;
            if (cantidad > 1000000) cantidad = 1000000; // cota superior
        }
        if (pt_in_rect(mx,my,&down)) {
            cantidad -= 50;
            if (cantidad < 0) cantidad = 0;
        }

        // Sin dropdown: no hay acción adicional aquí
        // (mantenido arriba) click en filepath -> foco
    } else if (e->type == SDL_TEXTINPUT) {
        if (identificador_active) {
            strncat(identificador, e->text.text, MAX_TEXT - strlen(identificador) - 1);
        } else if (mode_active) {
            /* Solo aceptar '0' o '1' y limitar a 9 dígitos */
            const char *txt = e->text.text;
            for (size_t i=0; txt[i] != '\0'; ++i) {
                if ((txt[i] == '0' || txt[i] == '1') && strlen(mode_text) < 9) {
                    size_t L = strlen(mode_text);
                    mode_text[L] = txt[i];
                    mode_text[L+1] = '\0';
                }
            }
        }
    } else if (e->type == SDL_KEYDOWN) {
        if (identificador_active) {
            if (e->key.keysym.sym == SDLK_BACKSPACE && strlen(identificador)>0) {
                identificador[strlen(identificador)-1] = '\0';
            }
        } else if (mode_active) {
            if (e->key.keysym.sym == SDLK_BACKSPACE && strlen(mode_text)>0) {
                mode_text[strlen(mode_text)-1] = '\0';
            }
        } else if (filepath_active) {
            if (e->key.keysym.sym == SDLK_BACKSPACE && strlen(filepath)>0) {
                filepath[strlen(filepath)-1] = '\0';
            }
        }
        /* Permitir escritura directa en filepath cuando no es identificador_active:
           no implementado por completo */
    }
}

/**
 * @brief Renderiza la página de inicialización (labels, inputs y botón Continuar).
 * @param ren  Renderer de SDL.
 * @param font Fuente TTF para los textos.
 */
void page_one_render(SDL_Renderer *ren, TTF_Font *font) {
    // Posiciones (ajustadas para mejor espaciado)
    SDL_Rect idlabel = {120,90,300,20};
    SDL_Rect idbox = {120,120,520,40};
    SDL_Rect cantidad_label = {120,180,300,20};
    SDL_Rect cantidad_box = {120,210,520,50};
    SDL_Rect up = {660,210,30,30};
    SDL_Rect down = {660,250,30,30};
    // Posicionar campo de modo
    mode_rect.x = 120; mode_rect.y = 280; mode_rect.w = 300; mode_rect.h = 30;
    SDL_Rect fp_label = {120,460,300,20};
    SDL_Rect fpbox = {120,490,480,30};
    continue_btn.x = 120; continue_btn.y = 540; continue_btn.w = 160; continue_btn.h = 50;

    // Fondo
    SDL_SetRenderDrawColor(ren, 245,245,245,255);
    SDL_RenderClear(ren);

    // Botón "Volver"
    SDL_SetRenderDrawColor(ren, 70,130,180,255);
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

    // Título centrado
    if (font) {
        SDL_Color titleColor = {20,20,20,255};
        SDL_Surface *titleS = TTF_RenderUTF8_Blended(font, "Inicializador", titleColor);
        if (titleS) {
            SDL_Texture *tt = SDL_CreateTextureFromSurface(ren, titleS);
            int tw, th; SDL_QueryTexture(tt, NULL, NULL, &tw, &th);
            int rw, rh; SDL_GetRendererOutputSize(ren, &rw, &rh);
            SDL_Rect td = { (rw - tw)/2, 20, tw, th };
            SDL_RenderCopy(ren, tt, NULL, &td);
            SDL_DestroyTexture(tt); SDL_FreeSurface(titleS);
        }
    }

    if (font) {
        SDL_Color col = {0,0,0,255};
        SDL_Surface *lab = TTF_RenderUTF8_Blended(font, "Identificador del espacio compartido", col);
        SDL_Texture *tlab = SDL_CreateTextureFromSurface(ren, lab);
        int tw, th; SDL_QueryTexture(tlab, NULL, NULL, &tw, &th);
        SDL_Rect d = { idlabel.x, idlabel.y, tw, th };
        SDL_RenderCopy(ren, tlab, NULL, &d);
        SDL_DestroyTexture(tlab); SDL_FreeSurface(lab);

        // Caja de identificador
        SDL_SetRenderDrawColor(ren, 255,255,255,255);
        SDL_RenderFillRect(ren, &idbox);
        // Contorno de foco
        if (identificador_active) SDL_SetRenderDrawColor(ren, 30,144,255,255); else SDL_SetRenderDrawColor(ren, 200,200,200,255);
        SDL_RenderDrawRect(ren, &idbox);
        if (strlen(identificador) > 0) {
            SDL_Surface *txt = TTF_RenderUTF8_Blended(font, identificador, col);
            SDL_Texture *tt = SDL_CreateTextureFromSurface(ren, txt);
            int tw2, th2; SDL_QueryTexture(tt, NULL, NULL, &tw2, &th2);
            SDL_Rect td = { idbox.x + 6, idbox.y + (idbox.h - th2)/2, tw2, th2 };
            SDL_RenderCopy(ren, tt, NULL, &td);
            SDL_DestroyTexture(tt); SDL_FreeSurface(txt);
        }

        // Etiqueta de cantidad
        SDL_Surface *lab2 = TTF_RenderUTF8_Blended(font, "Cantidad de espacios para almacenar valores", col);
        SDL_Texture *tlab2 = SDL_CreateTextureFromSurface(ren, lab2);
        SDL_QueryTexture(tlab2, NULL, NULL, &tw, &th);
        SDL_Rect d2 = { cantidad_label.x, cantidad_label.y, tw, th };
        SDL_RenderCopy(ren, tlab2, NULL, &d2);
        SDL_DestroyTexture(tlab2); SDL_FreeSurface(lab2);

        // Caja de cantidad
        SDL_SetRenderDrawColor(ren, 255,255,255,255);
        SDL_RenderFillRect(ren, &cantidad_box);
        SDL_SetRenderDrawColor(ren, 200,200,200,255);
        SDL_RenderDrawRect(ren, &cantidad_box);

        // Valor de cantidad
        char numbuf[32]; snprintf(numbuf, sizeof(numbuf), "%d", cantidad);
        SDL_Surface *num = TTF_RenderUTF8_Blended(font, numbuf, col);
        SDL_Texture *tn = SDL_CreateTextureFromSurface(ren, num);
        SDL_QueryTexture(tn, NULL, NULL, &tw, &th);
        SDL_Rect dn = { cantidad_box.x + 6, cantidad_box.y + (cantidad_box.h - th)/2, tw, th };
        SDL_RenderCopy(ren, tn, NULL, &dn);
        SDL_DestroyTexture(tn); SDL_FreeSurface(num);

        // Botones up/down
        SDL_SetRenderDrawColor(ren, 180,180,180,255);
        SDL_RenderFillRect(ren, &up);
        SDL_RenderFillRect(ren, &down);
        // Glifos de flecha
        SDL_Color arrowCol = {40,40,40,255};
        SDL_Surface *arr_up = TTF_RenderUTF8_Blended(font, "▲", arrowCol);
        SDL_Surface *arr_down = TTF_RenderUTF8_Blended(font, "▼", arrowCol);
        if (arr_up && arr_down) {
            SDL_Texture *tar_up = SDL_CreateTextureFromSurface(ren, arr_up);
            SDL_Texture *tar_down = SDL_CreateTextureFromSurface(ren, arr_down);
            int atw, ath;
            SDL_QueryTexture(tar_up, NULL, NULL, &atw, &ath);
            SDL_Rect aud = { up.x + (up.w - atw)/2, up.y + (up.h - ath)/2, atw, ath };
            SDL_QueryTexture(tar_down, NULL, NULL, &atw, &ath);
            SDL_Rect add = { down.x + (down.w - atw)/2, down.y + (down.h - ath)/2, atw, ath };
            SDL_RenderCopy(ren, tar_up, NULL, &aud);
            SDL_RenderCopy(ren, tar_down, NULL, &add);
            SDL_DestroyTexture(tar_up); SDL_DestroyTexture(tar_down);
            SDL_FreeSurface(arr_up); SDL_FreeSurface(arr_down);
        }

        // Etiqueta de clave de 9 bits
        SDL_Surface *lab3 = TTF_RenderUTF8_Blended(font, "Clave de 9 bits (solo 0 y 1)", col);
        SDL_Texture *tlab3 = SDL_CreateTextureFromSurface(ren, lab3);
        SDL_QueryTexture(tlab3, NULL, NULL, &tw, &th);
        SDL_Rect d3 = { mode_rect.x, mode_rect.y - 22, tw, th };
        SDL_RenderCopy(ren, tlab3, NULL, &d3);
        SDL_DestroyTexture(tlab3); SDL_FreeSurface(lab3);

        // Caja de texto de modo
        SDL_SetRenderDrawColor(ren, 255,255,255,255);
        SDL_RenderFillRect(ren, &mode_rect);
        if (mode_active) SDL_SetRenderDrawColor(ren, 30,144,255,255); else SDL_SetRenderDrawColor(ren, 200,200,200,255);
        SDL_RenderDrawRect(ren, &mode_rect);
        // Contenido de la clave
        if (strlen(mode_text) > 0) {
            SDL_Surface *sel = TTF_RenderUTF8_Blended(font, mode_text, col);
            SDL_Texture *tsel = SDL_CreateTextureFromSurface(ren, sel);
            SDL_QueryTexture(tsel, NULL, NULL, &tw, &th);
            SDL_Rect dsel = { mode_rect.x + 6, mode_rect.y + (mode_rect.h - th)/2, tw, th };
            SDL_RenderCopy(ren, tsel, NULL, &dsel);
            SDL_DestroyTexture(tsel); SDL_FreeSurface(sel);
        }

        // Selector de archivo se movió a la página sender

        // Botón "Continuar"
        if (continue_disabled) SDL_SetRenderDrawColor(ren, 120,120,120,255); else SDL_SetRenderDrawColor(ren, 34,139,34,255);
        SDL_RenderFillRect(ren, &continue_btn);
        SDL_SetRenderDrawColor(ren, 255,255,255,255);
        SDL_Surface *cont = TTF_RenderUTF8_Blended(font, "Continuar", (SDL_Color){255,255,255,255});
        SDL_Texture *tcont = SDL_CreateTextureFromSurface(ren, cont);
        SDL_QueryTexture(tcont, NULL, NULL, &tw, &th);
        SDL_Rect dcont = { continue_btn.x + (continue_btn.w - tw)/2, continue_btn.y + (continue_btn.h - th)/2, tw, th };
        SDL_RenderCopy(ren, tcont, NULL, &dcont);
        SDL_DestroyTexture(tcont); SDL_FreeSurface(cont);
    }
}
