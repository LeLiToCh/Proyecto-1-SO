#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#endif

typedef struct {
    MemEntry *buf;
    size_t cap;   // capacity
    size_t head;  // index of oldest element
    size_t tail;  // index one past newest element
    size_t size;  // number of elements
} MemRB;

static MemRB g_mem = {0};
static int g_is_shared = 0;

#ifdef _WIN32
typedef struct {
    size_t cap;
    size_t head;
    size_t tail;
    size_t size;
} SharedHeader;

static HANDLE g_hMap = NULL;
static HANDLE g_hMutex = NULL;
static HANDLE g_hItems = NULL;
static HANDLE g_hSpaces = NULL;
static SharedHeader *g_sh = NULL;
static MemEntry *g_sh_data = NULL;

static void to_wide_sanitized(const char *utf8, wchar_t *out, size_t outc, const wchar_t *suffix) {
    // Convert utf8 to wide; replace backslashes with underscores to avoid object namespace issues
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
static int g_sh_fd = -1;
static SharedHeader *g_sh = NULL;
static MemEntry *g_sh_data = NULL;
static sem_t *g_sem_mutex = NULL;
static sem_t *g_sem_items = NULL;
static sem_t *g_sem_spaces = NULL;
static char g_name_mem[128];
static char g_name_mtx[128];
static char g_name_items[128];
static char g_name_spaces[128];
static void sanitize_name(const char *in, char *out, size_t outc) {
    size_t i=0; for (; in && in[i] && i+1<outc; ++i) {
        char c = in[i];
        if (c=='/' || c=='\\') c = '_';
        out[i] = c;
    }
    out[i] = '\0';
}
#endif

bool memory_init(size_t capacity) {
    g_is_shared = 0;
    if (capacity == 0) capacity = 1; // enforce minimum
    // re-init: free previous buffer
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

void memory_shutdown(void) {
    if (g_is_shared) {
#ifdef _WIN32
        if (g_sh) { UnmapViewOfFile(g_sh); g_sh = NULL; g_sh_data = NULL; }
        if (g_hItems) { CloseHandle(g_hItems); g_hItems = NULL; }
        if (g_hSpaces) { CloseHandle(g_hSpaces); g_hSpaces = NULL; }
        if (g_hMutex) { CloseHandle(g_hMutex); g_hMutex = NULL; }
        if (g_hMap) { CloseHandle(g_hMap); g_hMap = NULL; }
#else
    if (g_sh) { munmap(g_sh, sizeof(SharedHeader) + (g_sh->cap * sizeof(MemEntry))); g_sh = NULL; g_sh_data = NULL; }
        if (g_sh_fd >= 0) { close(g_sh_fd); g_sh_fd = -1; }
        if (g_sem_items) { sem_close(g_sem_items); g_sem_items = NULL; }
        if (g_sem_spaces) { sem_close(g_sem_spaces); g_sem_spaces = NULL; }
        if (g_sem_mutex) { sem_close(g_sem_mutex); g_sem_mutex = NULL; }
#endif
        g_is_shared = 0;
    }
    if (g_mem.buf) { free(g_mem.buf); g_mem = (MemRB){0}; }
}

void memory_clear(void) {
    if (g_is_shared) {
#ifdef _WIN32
        // Reset indices under mutex
        WaitForSingleObject(g_hMutex, INFINITE);
        size_t cap = g_sh->cap;
        g_sh->head = g_sh->tail = 0;
        g_sh->size = 0;
        ReleaseMutex(g_hMutex);
        // Drain items to 0
        while (WaitForSingleObject(g_hItems, 0) == WAIT_OBJECT_0) {}
        // Drain spaces to 0, then set to cap
        int drained = 0;
        while (WaitForSingleObject(g_hSpaces, 0) == WAIT_OBJECT_0) { drained++; }
        if (drained < (int)cap) {
            ReleaseSemaphore(g_hSpaces, (LONG)cap, NULL);
        } else {
            // If drained >= cap, just set back to cap
            ReleaseSemaphore(g_hSpaces, (LONG)cap, NULL);
        }
#else
        // POSIX: lock mutex
        sem_wait(g_sem_mutex);
        size_t cap = g_sh->cap;
        g_sh->head = g_sh->tail = 0;
        g_sh->size = 0;
        sem_post(g_sem_mutex);
        // Drain items
        int v = 0;
        while (sem_trywait(g_sem_items) == 0) {}
        // Drain spaces then set to cap
        int drained_spaces = 0;
        while (sem_trywait(g_sem_spaces) == 0) { drained_spaces++; }
        for (size_t i=0; i<cap; ++i) sem_post(g_sem_spaces);
#endif
    } else {
        g_mem.head = g_mem.tail = 0;
        g_mem.size = 0;
    }
}

size_t memory_capacity(void) { return g_is_shared ? (g_sh ? g_sh->cap : 0) : g_mem.cap; }
size_t memory_size(void) {
    if (!g_is_shared) return g_mem.size;
#ifdef _WIN32
    size_t sz;
    WaitForSingleObject(g_hMutex, INFINITE);
    sz = g_sh->size;
    ReleaseMutex(g_hMutex);
    return sz;
#else
    size_t sz;
    sem_wait(g_sem_mutex);
    sz = g_sh->size;
    sem_post(g_sem_mutex);
    return sz;
#endif
}
bool memory_is_empty(void) { return memory_size() == 0; }
bool memory_is_full(void) { return memory_size() == memory_capacity(); }

static uint64_t now_ms(void) {
#ifdef _WIN32
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli; uli.LowPart = ft.dwLowDateTime; uli.HighPart = ft.dwHighDateTime;
    const uint64_t EPOCH_DIFF = 116444736000000000ULL; // 1601->1970 in 100ns
    uint64_t t100ns = uli.QuadPart - EPOCH_DIFF;
    return t100ns / 10000ULL;
#else
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}

static bool memory_write_core(uint8_t ascii, uint8_t key_used, uint32_t *out_index, uint64_t *out_ts) {
    uint64_t ts = now_ms();
    if (g_is_shared) {
#ifdef _WIN32
        if (!g_hSpaces || !g_hItems || !g_hMutex || !g_sh) return false;
        if (WaitForSingleObject(g_hSpaces, INFINITE) != WAIT_OBJECT_0) return false; // wait for space
        if (WaitForSingleObject(g_hMutex, INFINITE) != WAIT_OBJECT_0) return false; // lock
        uint32_t idx = (uint32_t)g_sh->tail;
        g_sh_data[g_sh->tail].ascii = ascii;
        g_sh_data[g_sh->tail].index = idx;
        g_sh_data[g_sh->tail].timestamp_ms = ts;
        g_sh_data[g_sh->tail].key_used = key_used;
        g_sh->tail = (g_sh->tail + 1) % g_sh->cap;
        g_sh->size++;
        ReleaseMutex(g_hMutex);
        ReleaseSemaphore(g_hItems, 1, NULL);
        if (out_index) *out_index = idx; if (out_ts) *out_ts = ts;
        return true;
#else
        if (!g_sem_spaces || !g_sem_items || !g_sem_mutex || !g_sh) return false;
        sem_wait(g_sem_spaces);
        sem_wait(g_sem_mutex);
        uint32_t idx = (uint32_t)g_sh->tail;
        g_sh_data[g_sh->tail].ascii = ascii;
        g_sh_data[g_sh->tail].index = idx;
        g_sh_data[g_sh->tail].timestamp_ms = ts;
        g_sh_data[g_sh->tail].key_used = key_used;
        g_sh->tail = (g_sh->tail + 1) % g_sh->cap;
        g_sh->size++;
        sem_post(g_sem_mutex);
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

bool memory_write_entry(uint8_t ascii, uint32_t *out_index, uint64_t *out_ts) {
    return memory_write_core(ascii, 0u, out_index, out_ts);
}

bool memory_write_entry_with_key(uint8_t ascii, uint8_t key_used, uint32_t *out_index, uint64_t *out_ts) {
    return memory_write_core(ascii, key_used, out_index, out_ts);
}

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

size_t memory_write(const char *s) {
    if (!s) return 0;
    size_t written = 0;
    for (size_t i = 0; s[i] != '\0'; ++i) {
        if (!memory_write_char(s[i])) break;
        written++;
    }
    return written;
}

bool memory_read_entry(MemEntry *out) {
    if (g_is_shared) {
#ifdef _WIN32
        if (!g_hItems || !g_hSpaces || !g_hMutex || !g_sh) return false;
        if (WaitForSingleObject(g_hItems, INFINITE) != WAIT_OBJECT_0) return false;
        if (WaitForSingleObject(g_hMutex, INFINITE) != WAIT_OBJECT_0) return false;
        if (out) *out = g_sh_data[g_sh->head];
        g_sh->head = (g_sh->head + 1) % g_sh->cap;
        g_sh->size--;
        ReleaseMutex(g_hMutex);
        ReleaseSemaphore(g_hSpaces, 1, NULL);
        return true;
#else
        if (!g_sem_items || !g_sem_spaces || !g_sem_mutex || !g_sh) return false;
        sem_wait(g_sem_items);
        sem_wait(g_sem_mutex);
        if (out) *out = g_sh_data[g_sh->head];
        g_sh->head = (g_sh->head + 1) % g_sh->cap;
        g_sh->size--;
        sem_post(g_sem_mutex);
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

bool memory_read_char(char *out) {
    MemEntry e;
    if (!memory_read_entry(&e)) return false;
    if (out) *out = (char)e.ascii;
    return true;
}

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

bool memory_peek(size_t index, char *out) {
    if (g_is_shared) {
#ifdef _WIN32
        bool ok = false;
        if (WaitForSingleObject(g_hMutex, INFINITE) == WAIT_OBJECT_0) {
            if (index < g_sh->size) {
                size_t pos = (g_sh->head + index) % g_sh->cap;
                if (out) *out = (char)g_sh_data[pos].ascii;
                ok = true;
            }
            ReleaseMutex(g_hMutex);
        }
        return ok;
#else
        bool ok = false;
        sem_wait(g_sem_mutex);
        if (index < g_sh->size) {
            size_t pos = (g_sh->head + index) % g_sh->cap;
            if (out) *out = (char)g_sh_data[pos].ascii;
            ok = true;
        }
        sem_post(g_sem_mutex);
        return ok;
#endif
    } else {
        if (index >= g_mem.size) return false;
        size_t pos = (g_mem.head + index) % g_mem.cap;
        if (out) *out = (char)g_mem.buf[pos].ascii;
        return true;
    }
}

bool memory_init_shared(const char *name, size_t capacity, bool *out_created) {
    if (capacity == 0) capacity = 1;
#ifdef _WIN32
    wchar_t wMap[600], wMtx[600], wItems[600], wSpaces[600];
    to_wide_sanitized(name, wMap, 600, L"_mem");
    to_wide_sanitized(name, wMtx, 600, L"_mtx");
    to_wide_sanitized(name, wItems, 600, L"_items");
    to_wide_sanitized(name, wSpaces, 600, L"_spaces");

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
            // capacity mismatch -> fail
            UnmapViewOfFile(g_sh); g_sh = NULL; g_sh_data = NULL;
            CloseHandle(g_hMap); g_hMap = NULL;
            return false;
        }
    }

    g_hMutex = CreateMutexW(NULL, FALSE, wMtx);
    if (!g_hMutex) { memory_shutdown(); return false; }
    g_hItems = CreateSemaphoreW(NULL, created ? 0 : 0, (LONG)capacity, wItems);
    if (!g_hItems) { memory_shutdown(); return false; }
    g_hSpaces = CreateSemaphoreW(NULL, created ? (LONG)capacity : (LONG)capacity, (LONG)capacity, wSpaces);
    if (!g_hSpaces) { memory_shutdown(); return false; }

    g_is_shared = 1;
    return true;
#else
    // POSIX implementation
    sanitize_name(name ? name : "mem", g_name_mem, sizeof(g_name_mem));
    snprintf(g_name_mtx, sizeof(g_name_mtx), "/%s_mtx", g_name_mem);
    snprintf(g_name_items, sizeof(g_name_items), "/%s_items", g_name_mem);
    snprintf(g_name_spaces, sizeof(g_name_spaces), "/%s_spaces", g_name_mem);
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

    g_sem_mutex = sem_open(g_name_mtx, O_CREAT, 0600, 1);
    if (g_sem_mutex == SEM_FAILED) { memory_shutdown(); return false; }
    g_sem_items = sem_open(g_name_items, O_CREAT, 0600, 0);
    if (g_sem_items == SEM_FAILED) { memory_shutdown(); return false; }
    g_sem_spaces = sem_open(g_name_spaces, O_CREAT, 0600, (unsigned int)capacity);
    if (g_sem_spaces == SEM_FAILED) { memory_shutdown(); return false; }

    g_is_shared = 1;
    return true;
#endif
}

void memory_debug_print_snapshot(void) {
    size_t cap = memory_capacity();
    size_t sz = memory_size();
    printf("[mem] size=%zu/%zu\n", sz, cap);
    if (sz == 0) return;
    if (g_is_shared) {
#ifdef _WIN32
        if (!g_sh) return;
        WaitForSingleObject(g_hMutex, INFINITE);
        size_t cur = g_sh->head;
        for (size_t i = 0; i < g_sh->size; ++i) {
            MemEntry e = g_sh_data[cur];
            printf("  #%02zu slot=%02zu ascii=%3u ts=%llu key=0x%02X\n", i, cur, (unsigned)e.ascii, (unsigned long long)e.timestamp_ms, (unsigned)e.key_used);
            cur = (cur + 1) % g_sh->cap;
        }
        ReleaseMutex(g_hMutex);
#else
        if (!g_sh) return;
        sem_wait(g_sem_mutex);
        size_t cur = g_sh->head;
        for (size_t i = 0; i < g_sh->size; ++i) {
            MemEntry e = g_sh_data[cur];
            printf("  #%02zu slot=%02zu ascii=%3u ts=%llu key=0x%02X\n", i, cur, (unsigned)e.ascii, (unsigned long long)e.timestamp_ms, (unsigned)e.key_used);
            cur = (cur + 1) % g_sh->cap;
        }
        sem_post(g_sem_mutex);
#endif
    } else {
        size_t cur = g_mem.head;
        for (size_t i = 0; i < g_mem.size; ++i) {
            MemEntry e = g_mem.buf[cur];
            printf("  #%02zu slot=%02zu ascii=%3u ts=%llu key=0x%02X\n", i, cur, (unsigned)e.ascii, (unsigned long long)e.timestamp_ms, (unsigned)e.key_used);
            cur = (cur + 1) % g_mem.cap;
        }
    }
}
