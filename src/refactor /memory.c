/* ============================================================================
 * memory.c — Buffer circular en memoria (local y compartida) con semáforos
 * ----------------------------------------------------------------------------
 * Propósito:
 *  - Proveer un ring buffer de entradas MemEntry con dos modos:
 *      1) Local (heap del proceso).
 *      2) Compartido entre procesos (Windows: FileMapping + semáforos Win32 /
 *         POSIX: shm_open + mmap + semáforos POSIX).
 *  - Sincronizar productores/consumidores mediante semáforos:
 *      * control (exclusión mutua binaria),
 *      * items (contador de elementos),
 *      * spaces (contador de espacios libres),
 *      * full (bandera binaria cuando el buffer se llena por completo),
 *      * term (difusión de terminación).
 *
 * Invariantes:
 *  - 0 <= size <= cap; head apunta al más antiguo; tail al siguiente libre.
 *  - En modo compartido, toda actualización de índices/size se hace bajo
 *    el semáforo de control para mantener coherencia.
 *
 * Notas:
 *  - La API expone operaciones de escritura/lectura bloqueantes para items/spaces.
 *  - Las funciones *_shared crean/adjuntan los objetos de IPC y marcan g_is_shared.
 *  - La marca 'full' es binaria: se activa al transicionar a “lleno” y se drena
 *    al leer cuando estaba lleno.
 * ========================================================================== */

#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#endif

/* --------------------- Estructuras internas (modo local) ------------------ */
/* Ring buffer local: índices mod cap */
typedef struct {
    MemEntry *buf;
    size_t cap;   // capacidad
    size_t head;  // índice del elemento más antiguo
    size_t tail;  // índice del siguiente libre
    size_t size;  // número de elementos
} MemRB;

/* Instancia global para modo local */
static MemRB g_mem = {0};
/* Flag global: 0 = local, 1 = compartido (usar structs/IPC específicos) */
static int g_is_shared = 0;

/* ---------------------- Estructuras/handles (Windows) --------------------- */
#ifdef _WIN32
/* Cabecera común en shm: índices y capacidad */
typedef struct {
    size_t cap;
    size_t head;
    size_t tail;
    size_t size;
} SharedHeader;

/* Objetos de kernel para memoria y sincronización (Win32) */
static HANDLE g_hMap = NULL;
static HANDLE g_hControl = NULL; // semáforo binario usado como control (reemplaza mutex)
static HANDLE g_hItems = NULL;
static HANDLE g_hSpaces = NULL;
static HANDLE g_hFull = NULL;    // señalizado cuando el buffer se llena (binario)
static HANDLE g_hTerm = NULL;    // semáforo de difusión de terminación
static SharedHeader *g_sh = NULL;
static MemEntry *g_sh_data = NULL;

/**
 * @brief Convierte un nombre UTF-8 a wide y sanea caracteres para objetos de kernel.
 * @param utf8   Nombre base (puede ser NULL).
 * @param out    Buffer wide de salida.
 * @param outc   Capacidad del buffer wide.
 * @param suffix Sufijo wide a concatenar (p. ej., L"_mem").
 * @note Reemplaza '\\', '/', ':' por '_' para evitar problemas de espacio de nombres.
 */
static void to_wide_sanitized(const char *utf8, wchar_t *out, size_t outc, const wchar_t *suffix) {
    // Convertir a wide y sanear: evitar separadores problemáticos en nombres de objeto
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8 ? utf8 : "", -1, NULL, 0);
    wchar_t tmp[512];
    if (needed <= 0 || needed >= (int)(sizeof(tmp)/sizeof(tmp[0]))) {
        wcsncpy(tmp, L"mem", sizeof(tmp)/sizeof(tmp[0]) - 1);
        tmp[sizeof(tmp)/sizeof(tmp[0]) - 1] = L'\0';
    } else {
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, tmp, (int)(sizeof(tmp)/sizeof(tmp[0])));
        for (wchar_t *p = tmp; *p; ++p) {
            if (*p == L'\\' || *p == L'/' || *p == L':') *p = L'_';
        }
    }
    _snwprintf(out, (int)outc, L"%ls%ls", tmp, suffix ? suffix : L"");
    out[outc-1] = L'\0';
}
#else
/* --------------------------- POSIX (Linux/macOS) -------------------------- */
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
typedef struct {
    size_t cap;
    size_t head;
    size_t tail;
    size_t size;
} SharedHeader;
/* Descriptores y nombres de objetos POSIX */
static int g_sh_fd = -1;
static SharedHeader *g_sh = NULL;
static MemEntry *g_sh_data = NULL;
static sem_t *g_sem_ctrl = NULL;  // semáforo de control (binario)
static sem_t *g_sem_items = NULL; // contador de elementos disponibles
static sem_t *g_sem_spaces = NULL;// contador de espacios libres
static sem_t *g_sem_full = NULL;  // >0 cuando el buffer está lleno
static sem_t *g_sem_term = NULL;  // difusión de terminación
static char g_name_mem[128];
static char g_name_ctrl[128];
static char g_name_items[128];
static char g_name_spaces[128];
static char g_name_full[128];
static char g_name_term[128];

/**
 * @brief Sanea un nombre para recursos POSIX, reemplazando separadores.
 * @param in   Nombre de entrada.
 * @param out  Buffer de salida.
 * @param outc Capacidad de salida.
 */
static void sanitize_name(const char *in, char *out, size_t outc) {
    size_t i=0; for (; in && in[i] && i+1<outc; ++i) {
        char c = in[i];
        if (c=='/' || c=='\\') c = '_';
        out[i] = c;
    }
    out[i] = '\0';
}
#endif

/**
 * @brief Inicializa el buffer de memoria en modo local (no compartido).
 * @param capacity Capacidad deseada (mínimo 1).
 * @return true si el buffer quedó listo; false ante fallo de reserva.
 * @note Si ya existía un buffer local, se libera y se crea uno nuevo.
 */
bool memory_init(size_t capacity) {
    g_is_shared = 0;
    if (capacity == 0) capacity = 1; // forzar mínimo
    // reinicialización: liberar buffer previo
    if (g_mem.buf) {
        free(g_mem.buf);
        g_mem = (MemRB){0};
    }
    g_mem.buf = (MemEntry*)calloc(capacity, sizeof(MemEntry));
    if (!g_mem.buf) {
        g_mem.cap = g_mem.head = g_mem.tail = g_mem.size = 0;
        return false;
    }
    g_mem.cap = capacity;
    g_mem.head = g_mem.tail = g_mem.size = 0;
    return true;
}

/**
 * @brief Libera todos los recursos (locales o compartidos) y reinicia estado global.
 * @post Después de llamar, g_is_shared=0 y no hay memoria asignada.
 */
void memory_shutdown(void) {
    if (g_is_shared) {
#ifdef _WIN32
        if (g_sh) { UnmapViewOfFile(g_sh); g_sh = NULL; g_sh_data = NULL; }
        if (g_hItems) { CloseHandle(g_hItems); g_hItems = NULL; }
        if (g_hSpaces) { CloseHandle(g_hSpaces); g_hSpaces = NULL; }
        if (g_hControl) { CloseHandle(g_hControl); g_hControl = NULL; }
        if (g_hFull) { CloseHandle(g_hFull); g_hFull = NULL; }
        if (g_hTerm) { CloseHandle(g_hTerm); g_hTerm = NULL; }
        if (g_hMap) { CloseHandle(g_hMap); g_hMap = NULL; }
#else
        if (g_sh) { munmap(g_sh, sizeof(SharedHeader) + (g_sh->cap * sizeof(MemEntry))); g_sh = NULL; g_sh_data = NULL; }
        if (g_sh_fd >= 0) { close(g_sh_fd); g_sh_fd = -1; }
        if (g_sem_items) { sem_close(g_sem_items); g_sem_items = NULL; }
        if (g_sem_spaces) { sem_close(g_sem_spaces); g_sem_spaces = NULL; }
        if (g_sem_ctrl) { sem_close(g_sem_ctrl); g_sem_ctrl = NULL; }
        if (g_sem_full) { sem_close(g_sem_full); g_sem_full = NULL; }
        if (g_sem_term) { sem_close(g_sem_term); g_sem_term = NULL; }
#endif
        g_is_shared = 0;
    }
    if (g_mem.buf) { free(g_mem.buf); g_mem = (MemRB){0}; }
}

/**
 * @brief Limpia el contenido del buffer (resetea índices y contadores).
 * @note En modo compartido, también reestablece los contadores de semáforo
 *       para reflejar “buffer vacío”.
 */
void memory_clear(void) {
    if (g_is_shared) {
#ifdef _WIN32
    // Resetear índices bajo el semáforo de control
    WaitForSingleObject(g_hControl, INFINITE);
        size_t cap = g_sh->cap;
        g_sh->head = g_sh->tail = 0;
        g_sh->size = 0;
    ReleaseSemaphore(g_hControl, 1, NULL);
        // Vaciar 'items' hasta 0
        while (WaitForSingleObject(g_hItems, 0) == WAIT_OBJECT_0) {}
        // Vaciar 'spaces' hasta 0, luego llevarlo a 'cap'
        int drained = 0;
        while (WaitForSingleObject(g_hSpaces, 0) == WAIT_OBJECT_0) { drained++; }
        if (drained < (int)cap) {
            ReleaseSemaphore(g_hSpaces, (LONG)cap, NULL);
        } else {
            // Si drained >= cap, igualmente reestablecer a 'cap'
            ReleaseSemaphore(g_hSpaces, (LONG)cap, NULL);
        }
    // Vaciar 'full' hasta 0
    while (WaitForSingleObject(g_hFull, 0) == WAIT_OBJECT_0) {}
#else
    // POSIX: tomar semáforo de control
    sem_wait(g_sem_ctrl);
        size_t cap = g_sh->cap;
        g_sh->head = g_sh->tail = 0;
        g_sh->size = 0;
    sem_post(g_sem_ctrl);
        // Vaciar 'items'
        while (sem_trywait(g_sem_items) == 0) {}
        // Vaciar 'spaces' y luego publicar 'cap' veces
        int drained_spaces = 0;
        while (sem_trywait(g_sem_spaces) == 0) { drained_spaces++; }
        for (size_t i=0; i<cap; ++i) sem_post(g_sem_spaces);
    // Vaciar 'full'
    while (sem_trywait(g_sem_full) == 0) {}
#endif
    } else {
        g_mem.head = g_mem.tail = 0;
        g_mem.size = 0;
    }
}

/** @brief Devuelve la capacidad del buffer actual. */
size_t memory_capacity(void) { return g_is_shared ? (g_sh ? g_sh->cap : 0) : g_mem.cap; }

/**
 * @brief Devuelve el número de elementos actualmente almacenados.
 * @note En modo compartido se lee bajo el semáforo de control para consistencia.
 */
size_t memory_size(void) {
    if (!g_is_shared) return g_mem.size;
#ifdef _WIN32
    size_t sz;
    WaitForSingleObject(g_hControl, INFINITE);
    sz = g_sh->size;
    ReleaseSemaphore(g_hControl, 1, NULL);
    return sz;
#else
    size_t sz;
    sem_wait(g_sem_ctrl);
    sz = g_sh->size;
    sem_post(g_sem_ctrl);
    return sz;
#endif
}

/** @brief Indica si el buffer está vacío. */
bool memory_is_empty(void) { return memory_size() == 0; }
/** @brief Indica si el buffer está lleno. */
bool memory_is_full(void) { return memory_size() == memory_capacity(); }

/**
 * @brief Obtiene timestamp actual en ms desde epoch (UTC).
 * @return Milisegundos desde 1970-01-01 00:00:00 UTC.
 */
static uint64_t now_ms(void) {
#ifdef _WIN32
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli; uli.LowPart = ft.dwLowDateTime; uli.HighPart = ft.dwHighDateTime;
    const uint64_t EPOCH_DIFF = 116444736000000000ULL; // 1601->1970 en pasos de 100ns
    uint64_t t100ns = uli.QuadPart - EPOCH_DIFF;
    return t100ns / 10000ULL;
#else
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}

/**
 * @brief Formatea un timestamp (ms) a cadena local "YYYY-MM-DD HH:MM:SS".
 * @param ts_ms Timestamp en milisegundos.
 * @param out   Buffer de salida.
 * @param outsz Capacidad del buffer de salida.
 */
void memory_format_timestamp(uint64_t ts_ms, char *out, size_t outsz) {
    if (!out || outsz == 0) return;
    out[0] = '\0';
#ifdef _WIN32
    // Separar a segundos y convertir a hora local (SYSTEMTIME)
    time_t sec = (time_t)(ts_ms / 1000ULL);
    struct tm lt;
    localtime_s(&lt, &sec);
    // Formato: YYYY-MM-DD HH:MM:SS
    snprintf(out, outsz, "%04d-%02d-%02d %02d:%02d:%02d",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
             lt.tm_hour, lt.tm_min, lt.tm_sec);
#else
    time_t sec = (time_t)(ts_ms / 1000ULL);
    struct tm lt;
    localtime_r(&sec, &lt);
    snprintf(out, outsz, "%04d-%02d-%02d %02d:%02d:%02d",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
             lt.tm_hour, lt.tm_min, lt.tm_sec);
#endif
}

/**
 * @brief Núcleo de escritura: inserta una entrada con ascii y key_used.
 * @param ascii     Byte a almacenar.
 * @param key_used  Clave asociada (metadato).
 * @param out_index (Opc) devuelve índice asignado.
 * @param out_ts    (Opc) devuelve timestamp de escritura.
 * @return true si la escritura se realizó; false en caso de error/estado no válido.
 * @note En compartido, sincroniza con spaces/items/ctrl y avisa 'full' al llenarse.
 */
static bool memory_write_core(uint8_t ascii, uint8_t key_used, uint32_t *out_index, uint64_t *out_ts) {
    uint64_t ts = now_ms();
    if (g_is_shared) {
#ifdef _WIN32
        if (!g_hSpaces || !g_hItems || !g_hControl || !g_sh) return false;
        if (WaitForSingleObject(g_hSpaces, INFINITE) != WAIT_OBJECT_0) return false; // esperar espacio libre
        if (WaitForSingleObject(g_hControl, INFINITE) != WAIT_OBJECT_0) return false; // entrar a sección crítica
        uint32_t idx = (uint32_t)g_sh->tail;
        g_sh_data[g_sh->tail].ascii = ascii;
        g_sh_data[g_sh->tail].index = idx;
        g_sh_data[g_sh->tail].timestamp_ms = ts;
        g_sh_data[g_sh->tail].key_used = key_used;
        g_sh->tail = (g_sh->tail + 1) % g_sh->cap;
        g_sh->size++;
        // Si pasó a “lleno”, señalizar 'full' (binario)
        if (g_sh->size == g_sh->cap) {
            ReleaseSemaphore(g_hFull, 1, NULL);
        }
        ReleaseSemaphore(g_hControl, 1, NULL);
        ReleaseSemaphore(g_hItems, 1, NULL);
        if (out_index) *out_index = idx; if (out_ts) *out_ts = ts;
        return true;
#else
        if (!g_sem_spaces || !g_sem_items || !g_sem_ctrl || !g_sh) return false;
        sem_wait(g_sem_spaces);
        sem_wait(g_sem_ctrl);
        uint32_t idx = (uint32_t)g_sh->tail;
        g_sh_data[g_sh->tail].ascii = ascii;
        g_sh_data[g_sh->tail].index = idx;
        g_sh_data[g_sh->tail].timestamp_ms = ts;
        g_sh_data[g_sh->tail].key_used = key_used;
        g_sh->tail = (g_sh->tail + 1) % g_sh->cap;
        g_sh->size++;
        if (g_sh->size == g_sh->cap) {
            sem_post(g_sem_full);
        }
        sem_post(g_sem_ctrl);
        sem_post(g_sem_items);
        if (out_index) *out_index = idx; if (out_ts) *out_ts = ts;
        return true;
#endif
    } else {
        if (g_mem.cap == 0) return false;
        if (g_mem.size == g_mem.cap) return false;
        uint32_t idx = (uint32_t)g_mem.tail;
        g_mem.buf[g_mem.tail].ascii = ascii;
        g_mem.buf[g_mem.tail].index = idx;
        g_mem.buf[g_mem.tail].timestamp_ms = ts;
        g_mem.buf[g_mem.tail].key_used = key_used;
        g_mem.tail = (g_mem.tail + 1) % g_mem.cap;
        g_mem.size++;
        if (out_index) *out_index = idx; if (out_ts) *out_ts = ts;
        return true;
    }
}

/** @brief Escribe una entrada con ascii y key_used=0. */
bool memory_write_entry(uint8_t ascii, uint32_t *out_index, uint64_t *out_ts) {
    return memory_write_core(ascii, 0u, out_index, out_ts);
}

/** @brief Escribe una entrada con ascii y clave asociada. */
bool memory_write_entry_with_key(uint8_t ascii, uint8_t key_used, uint32_t *out_index, uint64_t *out_ts) {
    return memory_write_core(ascii, key_used, out_index, out_ts);
}

/** @brief Escribe un carácter (conversión a uint8_t implícita). */
bool memory_write_char(char c) {
    if (g_is_shared) {
#ifdef _WIN32
        return memory_write_entry((uint8_t)c, NULL, NULL);
#else
        return memory_write_entry((uint8_t)c, NULL, NULL);
#endif
    } else {
        return memory_write_entry((uint8_t)c, NULL, NULL);
    }
}

/**
 * @brief Escribe una cadena completa hasta '\0' o llenarse el buffer.
 * @param s Cadena a escribir (UTF-8 opaco a nivel de bytes).
 * @return Número de bytes escritos.
 */
size_t memory_write(const char *s) {
    if (!s) return 0;
    size_t written = 0;
    for (size_t i = 0; s[i] != '\0'; ++i) {
        if (!memory_write_char(s[i])) break;
        written++;
    }
    return written;
}

/**
 * @brief Lee una entrada (bloqueante sobre items en compartido).
 * @param out (Opc) Devuelve la entrada leída.
 * @return true si hubo lectura; false si no hay datos o estado no válido.
 * @note Gestiona la bandera 'full' al salir del estado “lleno”.
 */
bool memory_read_entry(MemEntry *out) {
    if (g_is_shared) {
#ifdef _WIN32
        if (!g_hItems || !g_hSpaces || !g_hControl || !g_sh) return false;
        if (WaitForSingleObject(g_hItems, INFINITE) != WAIT_OBJECT_0) return false;
        if (WaitForSingleObject(g_hControl, INFINITE) != WAIT_OBJECT_0) return false;
        bool was_full = (g_sh->size == g_sh->cap);
        if (out) *out = g_sh_data[g_sh->head];
        g_sh->head = (g_sh->head + 1) % g_sh->cap;
        g_sh->size--;
        if (was_full) {
            // Drenar 'full' de vuelta a 0
            while (WaitForSingleObject(g_hFull, 0) == WAIT_OBJECT_0) {}
        }
        ReleaseSemaphore(g_hControl, 1, NULL);
        ReleaseSemaphore(g_hSpaces, 1, NULL);
        return true;
#else
        if (!g_sem_items || !g_sem_spaces || !g_sem_ctrl || !g_sh) return false;
        sem_wait(g_sem_items);
        sem_wait(g_sem_ctrl);
        int was_full = (g_sh->size == g_sh->cap);
        if (out) *out = g_sh_data[g_sh->head];
        g_sh->head = (g_sh->head + 1) % g_sh->cap;
        g_sh->size--;
        if (was_full) {
            while (sem_trywait(g_sem_full) == 0) {}
        }
        sem_post(g_sem_ctrl);
        sem_post(g_sem_spaces);
        return true;
#endif
    } else {
        if (g_mem.size == 0) return false;
        if (out) *out = g_mem.buf[g_mem.head];
        g_mem.head = (g_mem.head + 1) % g_mem.cap;
        g_mem.size--;
        return true;
    }
}

/** @brief Lee un solo carácter (atajo sobre memory_read_entry). */
bool memory_read_char(char *out) {
    MemEntry e;
    if (!memory_read_entry(&e)) return false;
    if (out) *out = (char)e.ascii;
    return true;
}

/**
 * @brief Lee hasta 'max' caracteres en el buffer de salida.
 * @param out Buffer destino (puede ser NULL para solo consumir).
 * @param max Máximo de bytes a leer.
 * @return Cantidad realmente leída.
 */
size_t memory_read(char *out, size_t max) {
    if (max == 0) return 0;
    size_t n = 0;
    if (g_is_shared) {
        for (; n < max; ++n) {
            char c;
            if (!memory_read_char(&c)) break;
            if (out) out[n] = c;
        }
        return n;
    } else {
        while (n < max && g_mem.size > 0) {
            char c;
            if (!memory_read_char(&c)) break;
            if (out) out[n] = c; n++;
        }
        return n;
    }
}

/**
 * @brief Inspecciona sin consumir el elemento en posición relativa 'index'.
 * @param index Índice relativo al head (0 = más antiguo).
 * @param out   (Opc) Devuelve el carácter si existe.
 * @return true si existe ese índice; false si está fuera de rango.
 * @note En compartido se protege con semáforo de control.
 */
bool memory_peek(size_t index, char *out) {
    if (g_is_shared) {
#ifdef _WIN32
        bool ok = false;
        if (WaitForSingleObject(g_hControl, INFINITE) == WAIT_OBJECT_0) {
            if (index < g_sh->size) {
                size_t pos = (g_sh->head + index) % g_sh->cap;
                if (out) *out = (char)g_sh_data[pos].ascii;
                ok = true;
            }
            ReleaseSemaphore(g_hControl, 1, NULL);
        }
        return ok;
#else
        bool ok = false;
        sem_wait(g_sem_ctrl);
        if (index < g_sh->size) {
            size_t pos = (g_sh->head + index) % g_sh->cap;
            if (out) *out = (char)g_sh_data[pos].ascii;
            ok = true;
        }
        sem_post(g_sem_ctrl);
        return ok;
#endif
    } else {
        if (index >= g_mem.size) return false;
        size_t pos = (g_mem.head + index) % g_mem.cap;
        if (out) *out = (char)g_mem.buf[pos].ascii;
        return true;
    }
}

/**
 * @brief Inicializa/adjunta el backend compartido con nombre lógico.
 * @param name        Nombre base para objetos (mapa/semáforos).
 * @param capacity    Capacidad del buffer (mínimo 1).
 * @param out_created (Opc) true si se creó; false si se adjuntó existente.
 * @return true si se configuró correctamente; false ante error.
 * @note En Windows usa CreateFileMapping/MapViewOfFile; en POSIX usa shm_open/mmap.
 */
bool memory_init_shared(const char *name, size_t capacity, bool *out_created) {
    if (capacity == 0) capacity = 1;
#ifdef _WIN32
    wchar_t wMap[600], wCtrl[600], wItems[600], wSpaces[600], wFull[600], wTerm[600];
    to_wide_sanitized(name, wMap, 600, L"_mem");
    to_wide_sanitized(name, wCtrl, 600, L"_ctrl");
    to_wide_sanitized(name, wItems, 600, L"_items");
    to_wide_sanitized(name, wSpaces, 600, L"_spaces");
    to_wide_sanitized(name, wFull, 600, L"_full");
    to_wide_sanitized(name, wTerm, 600, L"_term");

    SIZE_T mapSize = sizeof(SharedHeader) + capacity * sizeof(MemEntry);
    g_hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)mapSize, wMap);
    if (!g_hMap) return false;
    DWORD err = GetLastError();
    bool created = (err != ERROR_ALREADY_EXISTS);
    if (out_created) *out_created = created;
    g_sh = (SharedHeader*)MapViewOfFile(g_hMap, FILE_MAP_ALL_ACCESS, 0, 0, mapSize);
    if (!g_sh) { CloseHandle(g_hMap); g_hMap = NULL; return false; }
    g_sh_data = (MemEntry*)((unsigned char*)g_sh + sizeof(SharedHeader));
    if (created) {
        g_sh->cap = capacity;
        g_sh->head = g_sh->tail = g_sh->size = 0;
    } else {
        if (g_sh->cap != capacity) {
            // discrepancia de capacidad -> fallar
            UnmapViewOfFile(g_sh); g_sh = NULL; g_sh_data = NULL;
            CloseHandle(g_hMap); g_hMap = NULL;
            return false;
        }
    }

    g_hControl = CreateSemaphoreW(NULL, 1, 1, wCtrl);
    if (!g_hControl) { memory_shutdown(); return false; }
    g_hItems = CreateSemaphoreW(NULL, created ? 0 : 0, (LONG)capacity, wItems);
    if (!g_hItems) { memory_shutdown(); return false; }
    g_hSpaces = CreateSemaphoreW(NULL, created ? (LONG)capacity : (LONG)capacity, (LONG)capacity, wSpaces);
    if (!g_hSpaces) { memory_shutdown(); return false; }
    g_hFull = CreateSemaphoreW(NULL, 0, 1, wFull);
    if (!g_hFull) { memory_shutdown(); return false; }
    g_hTerm = CreateSemaphoreW(NULL, 0, 0x7fffffff, wTerm);
    if (!g_hTerm) { memory_shutdown(); return false; }

    g_is_shared = 1;
    return true;
#else
    // Implementación POSIX
    sanitize_name(name ? name : "mem", g_name_mem, sizeof(g_name_mem));
    snprintf(g_name_ctrl, sizeof(g_name_ctrl), "/%s_ctrl", g_name_mem);
    snprintf(g_name_items, sizeof(g_name_items), "/%s_items", g_name_mem);
    snprintf(g_name_spaces, sizeof(g_name_spaces), "/%s_spaces", g_name_mem);
    snprintf(g_name_full, sizeof(g_name_full), "/%s_full", g_name_mem);
    snprintf(g_name_term, sizeof(g_name_term), "/%s_term", g_name_mem);
    char shm_name[128]; snprintf(shm_name, sizeof(shm_name), "/%s_mem", g_name_mem);

    g_sh_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (g_sh_fd < 0) return false;
    size_t mapSize = sizeof(SharedHeader) + capacity * sizeof(MemEntry);
    if (ftruncate(g_sh_fd, (off_t)mapSize) != 0) { close(g_sh_fd); g_sh_fd = -1; return false; }
    void *p = mmap(NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, g_sh_fd, 0);
    if (!p || p == MAP_FAILED) { close(g_sh_fd); g_sh_fd = -1; return false; }
    g_sh = (SharedHeader*)p;
    g_sh_data = (MemEntry*)((unsigned char*)p + sizeof(SharedHeader));

    bool created = (g_sh->cap == 0);
    if (out_created) *out_created = created;
    if (created) {
        g_sh->cap = capacity;
        g_sh->head = g_sh->tail = g_sh->size = 0;
    } else if (g_sh->cap != capacity) {
        munmap(p, mapSize); g_sh = NULL; g_sh_data = NULL; close(g_sh_fd); g_sh_fd = -1; return false;
    }

    g_sem_ctrl  = sem_open(g_name_ctrl,  O_CREAT, 0600, 1);
    if (g_sem_ctrl == SEM_FAILED) { memory_shutdown(); return false; }
    g_sem_items = sem_open(g_name_items, O_CREAT, 0600, 0);
    if (g_sem_items == SEM_FAILED) { memory_shutdown(); return false; }
    g_sem_spaces= sem_open(g_name_spaces,O_CREAT, 0600, (unsigned int)capacity);
    if (g_sem_spaces== SEM_FAILED) { memory_shutdown(); return false; }
    g_sem_full  = sem_open(g_name_full,  O_CREAT, 0600, 0);
    if (g_sem_full == SEM_FAILED) { memory_shutdown(); return false; }
    g_sem_term  = sem_open(g_name_term,  O_CREAT, 0600, 0);
    if (g_sem_term == SEM_FAILED) { memory_shutdown(); return false; }

    g_is_shared = 1;
    return true;
#endif
}

/**
 * @brief Imprime un snapshot del contenido del buffer (debug).
 * @note En compartido, protege el recorrido con semáforo de control.
 */
void memory_debug_print_snapshot(void) {
    size_t cap = memory_capacity();
    size_t sz = memory_size();
    printf("[mem] size=%zu/%zu\n", sz, cap);
    if (sz == 0) return;
    if (g_is_shared) {
#ifdef _WIN32
        if (!g_sh) return;
        WaitForSingleObject(g_hControl, INFINITE);
        size_t cur = g_sh->head;
        for (size_t i = 0; i < g_sh->size; ++i) {
            MemEntry e = g_sh_data[cur];
            char when[32]; memory_format_timestamp(e.timestamp_ms, when, sizeof(when));
            printf("  #%02zu slot=%02zu ascii=%3u time=%s key=0x%02X\n", i, cur, (unsigned)e.ascii, when, (unsigned)e.key_used);
            cur = (cur + 1) % g_sh->cap;
        }
        ReleaseSemaphore(g_hControl, 1, NULL);
#else
        if (!g_sh) return;
        sem_wait(g_sem_ctrl);
        size_t cur = g_sh->head;
        for (size_t i = 0; i < g_sh->size; ++i) {
            MemEntry e = g_sh_data[cur];
            char when[32]; memory_format_timestamp(e.timestamp_ms, when, sizeof(when));
            printf("  #%02zu slot=%02zu ascii=%3u time=%s key=0x%02X\n", i, cur, (unsigned)e.ascii, when, (unsigned)e.key_used);
            cur = (cur + 1) % g_sh->cap;
        }
        sem_post(g_sem_ctrl);
#endif
    } else {
        size_t cur = g_mem.head;
        for (size_t i = 0; i < g_mem.size; ++i) {
            MemEntry e = g_mem.buf[cur];
            char when[32]; memory_format_timestamp(e.timestamp_ms, when, sizeof(when));
            printf("  #%02zu slot=%02zu ascii=%3u time=%s key=0x%02X\n", i, cur, (unsigned)e.ascii, when, (unsigned)e.key_used);
            cur = (cur + 1) % g_mem.cap;
        }
    }
}

/* ----------------------------- Terminación -------------------------------- */
/**
 * @brief Difunde una señal de terminación para despertar potenciales esperadores.
 * @note Implementada como múltiples posts sobre el semáforo 'term'.
 */
void memory_broadcast_terminate(void) {
    if (!g_is_shared) return;
#ifdef _WIN32
    if (g_hTerm) {
        // Publicación masiva para despertar potenciales esperadores
        ReleaseSemaphore(g_hTerm, 0x10000, NULL);
    }
#else
    if (g_sem_term) {
        for (int i=0; i<65535; ++i) sem_post(g_sem_term);
    }
#endif
}

/**
 * @brief Consulta no bloqueante: ¿se notificó terminación?
 * @return true si se detectó la señal (se reinyecta para que sea idempotente).
 */
bool memory_termination_notified(void) {
    if (!g_is_shared) return false;
#ifdef _WIN32
    if (!g_hTerm) return false;
    DWORD r = WaitForSingleObject(g_hTerm, 0);
    if (r == WAIT_OBJECT_0) { ReleaseSemaphore(g_hTerm, 1, NULL); return true; }
    return false;
#else
    if (!g_sem_term) return false;
    if (sem_trywait(g_sem_term) == 0) { sem_post(g_sem_term); return true; }
    return false;
#endif
}
