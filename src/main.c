#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>

#include "pages/modo_operacion.h"
#include "pages/inicializador.h"
#include "pages/page_two.h"
#include "pages/nueva_instancia.h"
#include "memory.h"

/* ============================================================================
 * main.c — UI de navegación con SDL2/SDL_ttf
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Crear una ventana y renderer SDL.
 *  - Cargar una fuente TTF desde varios paths candidatos.
 *  - Despachar eventos a páginas (handlers) y renderizar la página activa.
 *
 * Puntos clave de diseño:
 *  - El enum Page modela el estado de navegación; cada handler puede pedir
 *    cambio de página vía un "next" (entero >= 0).
 *  - El bucle principal procesa todos los eventos en cola con SDL_PollEvent,
 *    luego dibuja la escena y presenta con SDL_RenderPresent.
 *  - Se añade un pequeño SDL_Delay(10) para evitar uso intensivo de CPU.
 *  - Al salir, se liberan recursos en orden inverso a su adquisición.
 *
 * Notas:
 *  - La fuente se busca en varios paths típicos además de "font.ttf" local.
 *  - memory_shutdown() se invoca al final por si el backend de memoria fue
 *    inicializado en alguna de las páginas.
 * ========================================================================== */


#define WINDOW_W 800   /* Ancho de la ventana principal en píxeles */
#define WINDOW_H 600   /* Alto de la ventana principal en píxeles */

/* Estado de navegación entre pantallas de la app */
typedef enum { PAGE_MAIN, PAGE_ONE, PAGE_TWO, PAGE_SENDER } Page;

/**
 * @brief Punto de entrada de la aplicación.
 * @param argc Recuento de argumentos de línea de comandos.
 * @param argv Arreglo de argumentos de línea de comandos.
 * @return 0 en caso de finalización correcta; distinto de 0 si falla init.
 *
 * Flujo:
 *  1) Inicializa SDL (vídeo) y SDL_ttf.
 *  2) Crea ventana y renderer acelerado con VSync.
 *  3) Intenta cargar una fuente TTF desde múltiples ubicaciones conocidas.
 *  4) Bucle principal:
 *     - Consume eventos (QUIT y delega al handler de la página activa).
 *     - Dibuja la página activa y presenta.
 *  5) Libera recursos y apaga subsistemas.
 */
int main(int argc, char **argv) {
    /* Inicialización de SDL Video */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    /* Inicialización de SDL_ttf para manejo de fuentes */
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    /* Creación de ventana centrada */
    SDL_Window *win = SDL_CreateWindow("App con 2 botones", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "CreateWindow Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    /* Renderer acelerado + sincronizado con VBlank (reduce tearing/CPU) */
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        fprintf(stderr, "CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    /* Carga de fuente — se intentan varios candidatos comunes del sistema */
    // Load font - expects "font.ttf" in current working directory
    TTF_Font *font = NULL;
    const char *font_candidates[] = {
        "font.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        NULL
    };
    // Try loading from multiple common locations
    for (int i = 0; font_candidates[i] != NULL; ++i) {
        font = TTF_OpenFont(font_candidates[i], 24);
        if (font) {
            printf("[INFO] Using font: %s\n", font_candidates[i]);
            break;
        }
    }
    if (!font) {
        /* Si no hay fuente, no se puede renderizar texto de las páginas */
        fprintf(stderr, "Failed to open font.ttf: %s\n", TTF_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event e;
    Page page = PAGE_MAIN;  /* Página inicial */

    /* ========================= BUCLE PRINCIPAL =========================
     * Estructura:
     *  - Consumir todos los eventos pendientes.
     *  - Delegar a la página actual para que actualice "next" si desea
     *    cambiar de pantalla (navegación explícita).
     *  - Dibujar la pantalla actual y presentar.
     * ================================================================== */
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else {
                int next = -1; /* -1 = sin cambio de página */
                if (page == PAGE_MAIN) page_main_handle_event(&e, ren, &next);
                else if (page == PAGE_ONE) page_one_handle_event(&e, &next);
                else if (page == PAGE_TWO) page_two_handle_event(&e, &next);
                else if (page == PAGE_SENDER) page_sender_handle_event(&e, &next);
                if (next >= 0) page = (Page)next; /* Commit de la navegación */
            }
        }

        /* Limpiar fondo con gris claro antes de renderizar UI */
        // Render
        SDL_SetRenderDrawColor(ren, 220, 220, 220, 255);
        SDL_RenderClear(ren);

        /* Render de la página activa. Cada página define su propia vista. */
        if (page == PAGE_MAIN) {
            page_main_render(ren, font);
        } else if (page == PAGE_ONE) {
            page_one_render(ren, font);
        } else if (page == PAGE_TWO) {
            page_two_render(ren, font);
        } else if (page == PAGE_SENDER) {
            page_sender_render(ren, font);
        }

        /* Presenta el frame y cede un poco de tiempo a la CPU */
        SDL_RenderPresent(ren);
        SDL_Delay(10);
    }

    /* Destrucción ordenada de recursos gráficos y tipográficos */
    TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    /* Liberar backend de memoria en caso de haber sido inicializado */
    // Release memory backend resources if initialized
    memory_shutdown();

    /* Apagado de subsistemas en orden inverso al inicio */
    TTF_Quit();
    SDL_Quit();
    return 0;
}
