#include "receptor.h"
#include "memory.h" // Cambiar la ruta al sacar de receptor
#include "monitor.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <SDL.h>

static uint8_t key_from_bits(const char *bits) {
    if (!bits || !*bits) return 0;
    // Toma los ultimos 8 bits significativos de derecha a izquierda
    size_t n = strlen(bits);
    uint8_t k = 0;
    int cnt = 0;

    for (int i = (int)n - 1; i >= 0 && cnt < 8; --i, ++cnt){
        if (bits[i] == '1') k |= (1u << cnt);
    }

    return k;
}

static void print_decoded_entry(const MemEntry *e, uint8_t key) {
    uint8_t decoded_ascci = e->ascii ^ key;
    char decoded_char = (char)decoded_ascci;

    // Convertir timestamp a formato legible
    time_t sec = e->timestamp_ms / 1000;
    struct tm *tm_info = localtime(&sec);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    // Impresion elegante (formato con colores y alineado)
    // El formato es: Caracter, Fecha y Hora de ingreso, Posicion (indice)
    printf("\x1b[32m] \x1b[0m%-10c \x1b[32m| \x1b[0m%s \x1b[32m| \x1b[0m%-10u \x1b[0m",
        decoded_char, time_str, e->index);

    // Mostrar el texto reconstruido en tiempo real
    printf(" -> Archivo reconstruido: \x1b[36m%c\x1b[0m\n", decoded_char);

    //memory_debug_print_snapshot();
    fflush(stdout);
}

// Logica principal del receptor
bool process_memory_to_output(const char *filepath, const char *key_bits, bool automatic) {
    FILE *f = NULL;
#ifdef _WIN32
    // Convertir ruta UTF-8 a UTF-16 y usar _wfopen para manejar acentos
    int wlen = MultiByteToWideChar(CP_UTF8, 0, filepath, -1, NULL, 0);
    if (wlen > 0) {
        wchar_t *wpath = (wchar_t *)malloc(wlen * sizeof(wchar_t));
        if (wpath) {
            MultiByteToWideChar(CP_UTF8, 0, filepath, -1, wpath, wlen);
            f = _wfopen(wpath, L"wb");
            free(wpath);
        }
    }
    if(!f)
#endif
    {
        f = fopen(filepath, "wb");
    }
    if (!f) {
        perror("[receptor] Error al abrir archivo de salida");
        return false;
    }
    
    // Imprimir todos los datos relevantes del RECEPTOR
    extern const char *app_state_get_identificador(void);
    extern size_t app_state_get_cantidad(void);
    extern const char *app_state_get_clave(void);
    extern bool app_state_get_automatic(void);

    printf("[INFO] --- INICIO DE RECEPCION ---\n");
    printf("Identificador: %s\n", app_state_get_identificador());
    printf("Cantidad: %zu\n", app_state_get_cantidad());
    printf("Clave: %s\n", app_state_get_clave());
    printf("Key_bits: %s\n", key_bits);
    printf("Archivo: %s\n", filepath);
    printf("Modo: %s\n", automatic ? "Automatico" : "Manual");
    printf("-------------------------------\n");

    uint8_t key = key_from_bits(key_bits);
    printf("[receptor] Clave (8-bit) usada: 0x%02X\n", key);
    // Encabezado de la tabla
    printf("\x1b[32m---------------------------------------------------------\x1b[0m\n");
    printf("\x1b[32m| \x1b[0m%-6s \x1b[32m| \x1b[0m%-6s \x1b[32m| \x1b[0m%-12s \x1b[32m| \x1b[0m%-6s \x1b[32m| \x1b[0m%-6s \x1b[32m|\x1b[0m\n", 
           "CHAR", "INDEX", "HORA", "DECOD", "KEY");
    printf("\x1b[32m---------------------------------------------------------\x1b[0m\n");

    int event_status = 0;
    while(event_status != SDL_QUIT) {
        MemEntry e;
        if (memory_read_entry(&e)) {
            print_decoded_entry(&e, key);
            // Escribir el caracter decodificado en el archivo
            char decoded_char = (char)(e.ascii ^ key);
            fputc(decoded_char, f);
            fflush(f);

            if (!automatic) {
                printf("Presione Enter para leer el siguiente caracter...\n");
                while ((event_status = getchar()) != '\n' && event_status != EOF);
            }
        } else {
            // Si no hay datos, esperar un poco antes de intentar de nuevo
            event_status = SDL_PollEvent(NULL);

            if (event_status == SDL_QUIT) {
                printf("[receptor] Evento SDL_QUIT recibido, terminando...\n");
            } else {
                SDL_Delay(50);
            }
        }

        if (event_status != SDL_QUIT) {
            event_status = SDL_PollEvent(NULL);
        }
    }

    printf("\n[receptor] Cerrando archivo de salida y finalizando...\n");
    fclose(f);
    return true;
}

typedef struct {
    char path[1024];
    char key[16];
    int automatic; // 0/1
} ReceptorArgs;

static int receptor_thread(void *data){
    ReceptorArgs *a = (ReceptorArgs *)data;
    process_memory_to_output(a->path, a->key, a->automatic ? true : false);
    
    // Al terminar, imprimir resumen de memoria
    printf("[INFO] --- FIN DE RECEPCION ---\n");
    memory_debug_print_snapshot();
    free(a);
    monitor_receptor_stopped();
    return 0;
}

bool receptor_start_async(const char *key_bits, bool automatic) {
    const char *output_filename = "output.txt";
    
    ReceptorArgs *a = (ReceptorArgs *)malloc(sizeof(ReceptorArgs));
    if (!a) return false;

    strncpy(a->path, output_filename, sizeof(a->path)-1);
    a->path[sizeof(a->path)-1] = '\0';

    strncpy(a->key, key_bits ? key_bits : "", sizeof(a->key)-1);
    a->key[sizeof(a->key)-1] = '\0';
    a->automatic = automatic ? 1 : 0;

    SDL_Thread *th = SDL_CreateThread(receptor_thread, "receptor_thread", a);
    monitor_receptor_started();
    if (!th) {
        free(a);
        fprintf(stderr, "[receptor] SDL_CreateThread failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}