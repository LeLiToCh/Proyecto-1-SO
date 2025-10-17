/* ============================================================================
 * processor.c — Lector/encoder de archivo hacia memoria compartida
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Leer un archivo en binario/UTF-8, aplicar XOR de 8 bits por carácter
 *    usando una clave derivada de texto binario, y escribirlo en la memoria
 *    (local o compartida) respetando la sincronización de semáforos.
 *
 * Puntos clave:
 *  - key_from_bits(): toma los 8 bits menos significativos (derecha a izquierda).
 *  - process_file_into_memory(): imprime cabecera y tabla por carácter; en
 *    modo manual, espera Enter entre caracteres.
 *  - processor_start_async(): lanza el procesamiento en un hilo SDL.
 *  - Uso del monitor: registra inicio/fin con monitor_processor_*().
 *
 * Notas:
 *  - En Windows se usa _wfopen para soportar rutas UTF-16 con acentos.
 *  - Se formatea fecha-hora local con memory_format_timestamp().
 *  - Las escrituras bloquean si el buffer está lleno (semaforización en memory.c).
 * ========================================================================== */

#include "processor.h"
#include "memory.h"
#include "monitor.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <SDL.h>

/**
 * @brief Convierte una cadena '0'/'1' en una clave de 8 bits.
 * @param bits Cadena binaria; si >8, toma los 8 menos significativos (a la derecha).
 * @return Clave XOR de 8 bits (uint8_t). Si NULL o vacía, devuelve 0.
 */
static uint8_t key_from_bits(const char *bits) {
    if (!bits || !*bits) return 0;
    // Tomar los últimos 8 bits significativos de derecha a izquierda
    size_t n = strlen(bits);
    uint8_t k = 0;
    int cnt = 0;
    for (int i = (int)n - 1; i >= 0 && cnt < 8; --i, ++cnt) {
        if (bits[i] == '1') k |= (1u << cnt);
    }
    return k;
}

/**
 * @brief Procesa un archivo y escribe sus bytes codificados en memoria.
 * @param filepath Ruta del archivo (UTF-8).
 * @param key_bits Cadena binaria para derivar clave XOR de 8 bits.
 * @param automatic true=flujo continuo; false=espera Enter entre caracteres.
 * @return true en éxito; false ante error (p. ej., al abrir archivo o escribir).
 *
 * Flujo:
 *  - Abre el archivo en modo binario.
 *  - Deriva la clave de 8 bits a partir de key_bits.
 *  - Para cada byte: orig ^ key -> enc; escribe enc en memoria con metadatos.
 *  - Imprime tabla de progreso (idx, hora, in, enc, key, ocupación del buffer).
 */
bool process_file_into_memory(const char *filepath, const char *key_bits, bool automatic) {
    if (!filepath || !*filepath) { fprintf(stderr, "[processor] filepath vacio\n"); return false; }
    // Imprimir todos los datos relevantes
    extern const char* app_state_get_identificador(void);
    extern size_t app_state_get_cantidad(void);
    extern const char* app_state_get_clave(void);
    extern bool app_state_get_automatic(void);
    printf("\n==================================================\n");
    printf(" INICIO DE PROCESO\n");
    printf("--------------------------------------------------\n");
    printf(" Identificador : %s\n", app_state_get_identificador());
    printf(" Cantidad      : %zu\n", app_state_get_cantidad());
    printf(" Archivo       : %s\n", filepath);
    printf(" Modo          : %s\n", automatic ? "Automatico" : "Manual");
    printf(" Semilla bits  : %s\n", key_bits);
    FILE *f = NULL;
#ifdef _WIN32
    // Convertir ruta UTF-8 a UTF-16 y usar _wfopen para soportar acentos
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
    printf(" Clave (8-bit) : 0x%02X\n", key);
    printf("--------------------------------------------------\n");
    printf(" Paso a paso (uno por caracter):\n");
    printf("  #  |        FECHA-HORA       | IDX | IN  | ENC | KEY | MEM\n");
    printf("-----+-------------------------+-----+-----+-----+-----+---------\n");
    int c;
    size_t line_no = 0;
    while ((c = fgetc(f)) != EOF) {
        // Procesar byte original -> XOR -> encolar en memoria
        uint8_t orig = (uint8_t)c;
        uint8_t enc = orig ^ key;
        uint32_t idx = 0; uint64_t ts = 0;
        // Escribir en memoria (bloquea si lleno por semáforos)
        if (!memory_write_entry_with_key(enc, key, &idx, &ts)) {
            fprintf(stderr, "[processor] fallo al escribir en memoria\n");
            fclose(f);
            return false;
        }
        // Fila con hora local formateada
        char when[32];
        memory_format_timestamp(ts, when, sizeof(when));
        size_t cap = memory_capacity();
        size_t sz  = memory_size();
        ++line_no;
        printf(" %3zu | %-23s | %3u | %3u | 0x%02X | 0x%02X | %2zu/%-2zu\n",
               line_no, when, (unsigned)idx, (unsigned)orig, enc, (unsigned)key, sz, cap);
        fflush(stdout);
        if (!automatic) {
            // Esperar Enter para continuar con el siguiente carácter
            printf("[manual] Presione Enter para continuar...\n");
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
        }
    }
    fclose(f);
    return true;
}

/* --------------------------- Hilo de procesamiento ------------------------ */
/* Estructura de argumentos para el hilo SDL del procesador */
typedef struct {
    char path[1024];
    char key[16];
    int automatic; // 0/1
} ProcArgs;

/**
 * @brief Función del hilo: ejecuta el procesamiento y notifica monitor.
 * @param data Puntero a ProcArgs (propiedad transferida; se libera aquí).
 * @return 0 al finalizar.
 */
static int processor_thread(void *data) {
    ProcArgs *a = (ProcArgs*)data;
    process_file_into_memory(a->path, a->key, a->automatic ? true : false);
    // Al terminar, imprimir resumen de memoria
    printf("\n================= FIN DE PROCESO =================\n");
    memory_debug_print_snapshot();
    free(a);
    monitor_processor_stopped();
    return 0;
}

/**
 * @brief Inicia el procesamiento en un hilo SDL.
 * @param filepath Ruta del archivo origen (UTF-8).
 * @param key_bits Cadena binaria para derivar clave XOR de 8 bits.
 * @param automatic true=Automático, false=Manual.
 * @return true si el hilo se creó correctamente; false en caso contrario.
 * @note El hilo se desacopla con SDL_DetachThread; no es joinable.
 */
bool processor_start_async(const char *filepath, const char *key_bits, bool automatic) {
    if (!filepath || !*filepath) return false;
    ProcArgs *a = (ProcArgs*)malloc(sizeof(ProcArgs));
    if (!a) return false;
    a->path[0] = '\0'; a->key[0] = '\0';
    strncat(a->path, filepath, sizeof(a->path)-1);
    strncat(a->key, key_bits ? key_bits : "", sizeof(a->key)-1);
    a->automatic = automatic ? 1 : 0;
    SDL_Thread *th = SDL_CreateThread(processor_thread, "processor_thread", a);
    monitor_processor_started();
    if (!th) { free(a); return false; }
    SDL_DetachThread(th);
    return true;
}
