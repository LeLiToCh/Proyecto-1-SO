#include "processor.h"
#include "memory.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <SDL.h>

static uint8_t key_from_bits(const char *bits) {
    if (!bits || !*bits) return 0;
    // Take last 8 significant bits from right to left
    size_t n = strlen(bits);
    uint8_t k = 0;
    int cnt = 0;
    for (int i = (int)n - 1; i >= 0 && cnt < 8; --i, ++cnt) {
        if (bits[i] == '1') k |= (1u << cnt);
    }
    return k;
}

bool process_file_into_memory(const char *filepath, const char *key_bits, bool automatic) {
    if (!filepath || !*filepath) { fprintf(stderr, "[processor] filepath vacio\n"); return false; }
    // Imprimir todos los datos relevantes
    extern const char* app_state_get_identificador(void);
    extern size_t app_state_get_cantidad(void);
    extern const char* app_state_get_clave(void);
    extern bool app_state_get_automatic(void);
    printf("[INFO] --- INICIO DE PROCESO ---\n");
    printf("Identificador: %s\n", app_state_get_identificador());
    printf("Cantidad: %zu\n", app_state_get_cantidad());
    printf("Clave: %s\n", app_state_get_clave());
    printf("Semilla (key_bits): %s\n", key_bits);
    printf("Archivo: %s\n", filepath);
    printf("Modo: %s\n", automatic ? "Automatico" : "Manual");
    printf("-------------------------------\n");
    FILE *f = NULL;
#ifdef _WIN32
    // Convert UTF-8 path to UTF-16 and use _wfopen to handle accents
    int wlen = MultiByteToWideChar(CP_UTF8, 0, filepath, -1, NULL, 0);
    if (wlen > 0) {
        wchar_t *wpath = (wchar_t*)malloc(wlen * sizeof(wchar_t));
        if (wpath) {
            MultiByteToWideChar(CP_UTF8, 0, filepath, -1, wpath, wlen);
            f = _wfopen(wpath, L"rb");
            free(wpath);
        }
    }
    if (!f)
#endif
    {
        f = fopen(filepath, "rb");
    }
    if (!f) { perror("[processor] fopen"); return false; }
    uint8_t key = key_from_bits(key_bits);
    printf("[processor] Clave (8-bit) usada: 0x%02X\n", key);
    // Print table header once (add KEY to identify origin per row)
    printf("%-6s %-6s %-6s %-12s %-6s\n", "ASCII", "INDEX", "HORA", "ENCODING", "KEY");
    int c;
    while ((c = fgetc(f)) != EOF) {
        // Only process printable and common whitespace; others can be skipped or stored as-is
        uint8_t orig = (uint8_t)c;
        uint8_t enc = orig ^ key;
        uint32_t idx = 0; uint64_t ts = 0;
        // Write into memory (blocks if full due to semaphores)
        if (!memory_write_entry_with_key(enc, key, &idx, &ts)) {
            fprintf(stderr, "[processor] fallo al escribir en memoria\n");
            fclose(f);
            return false;
        }
    // Pretty print row under header, including the constant KEY used by this writer
    printf("%-6u %-6u %-6llu 0x%02X   0x%02X\n", (unsigned)orig, (unsigned)idx, (unsigned long long)ts, enc, (unsigned)key);
        // Imprimir snapshot de memoria después de cada escritura
        memory_debug_print_snapshot();
        fflush(stdout);
        if (!automatic) {
            // Wait for Enter to proceder a next character
            printf("[manual] Presione Enter para continuar...\n");
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
        }
    }
    fclose(f);
    return true;
}

typedef struct {
    char path[1024];
    char key[16];
    int automatic; // 0/1
} ProcArgs;

static int processor_thread(void *data) {
    ProcArgs *a = (ProcArgs*)data;
    process_file_into_memory(a->path, a->key, a->automatic ? true : false);
    // Al terminar, imprimir resumen de memoria
    printf("[INFO] --- FIN DE PROCESO ---\n");
    memory_debug_print_snapshot();
    free(a);
    return 0;
}

bool processor_start_async(const char *filepath, const char *key_bits, bool automatic) {
    if (!filepath || !*filepath) return false;
    ProcArgs *a = (ProcArgs*)malloc(sizeof(ProcArgs));
    if (!a) return false;
    a->path[0] = '\0'; a->key[0] = '\0';
    strncat(a->path, filepath, sizeof(a->path)-1);
    strncat(a->key, key_bits ? key_bits : "", sizeof(a->key)-1);
    a->automatic = automatic ? 1 : 0;
    SDL_Thread *th = SDL_CreateThread(processor_thread, "processor_thread", a);
    if (!th) { free(a); return false; }
    SDL_DetachThread(th);
    return true;
}
