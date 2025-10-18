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
 *  - processor_start_heavy(): lanza el procesamiento como proceso separado.
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
#include "heavy_process.h"  // <-- NUEVO
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief Convierte una cadena '0'/'1' en una clave de 8 bits.
 * @param bits Cadena binaria; si >8, toma los 8 menos significativos (a la derecha).
 * @return Clave XOR de 8 bits (uint8_t). Si NULL o vacía, devuelve 0.
 */
static uint8_t key_from_bits(const char *bits) {
    if (!bits || !*bits) return 0;
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
 */
bool process_file_into_memory(const char *filepath, const char *key_bits, bool automatic) {
    if (!filepath || !*filepath) { fprintf(stderr, "[processor] filepath vacio\n"); return false; }

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
        uint8_t orig = (uint8_t)c;
        uint8_t enc = orig ^ key;
        uint32_t idx = 0; uint64_t ts = 0;

        if (!memory_write_entry_with_key(enc, key, &idx, &ts)) {
            fprintf(stderr, "[processor] fallo al escribir en memoria\n");
            fclose(f);
            return false;
        }

        char when[32];
        memory_format_timestamp(ts, when, sizeof(when));
        size_t cap = memory_capacity();
        size_t sz  = memory_size();
        ++line_no;
        printf(" %3zu | %-23s | %3u | %3u | 0x%02X | 0x%02X | %2zu/%-2zu\n",
               line_no, when, (unsigned)idx, (unsigned)orig, enc, (unsigned)key, sz, cap);
        fflush(stdout);

        if (!automatic) {
            printf("[manual] Presione Enter para continuar...\n");
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
        }
    }
    fclose(f);
    return true;
}

/* ============================================================================
 * NUEVA IMPLEMENTACIÓN: Heavy Process
 * ========================================================================== */

/**
 * @brief Inicia el procesamiento en un proceso separado (sin hilos).
 * @param filepath Ruta del archivo origen (UTF-8).
 * @param key_bits Cadena binaria para derivar clave XOR de 8 bits.
 * @param automatic true=Automático, false=Manual.
 * @return true si el proceso se lanzó correctamente; false en caso contrario.
 */
bool processor_start_heavy(const char *filepath, const char *key_bits, bool automatic) {
    if (!filepath || !*filepath) return false;

    monitor_processor_started(); // Notificar inicio

    pid_t pid = launch_emisor_heavy("/mem_ascii", filepath, key_bits, automatic);

    int exit_code = 0;
    wait_process(pid, &exit_code);

    printf("\n================= FIN DE PROCESO =================\n");
    memory_debug_print_snapshot();
    monitor_processor_stopped(); // Notificar fin

    return (exit_code == 0);
}
