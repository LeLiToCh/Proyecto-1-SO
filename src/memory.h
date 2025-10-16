#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Simple ASCII memory backend as a ring buffer of entries.
// Each entry stores the encoded ASCII value, the index used, and the insertion timestamp.
// Capacity limits how many entries (characters) are stored simultaneously.

typedef struct {
	uint8_t ascii;      // encoded ASCII value (after XOR)
	uint32_t index;       // position (0..capacity-1) where it was inserted
	uint64_t timestamp_ms;// milliseconds since Unix epoch
	uint8_t key_used;    // optional: key used by the writer (for debugging/trace)
} MemEntry;

// Initialize in-process memory with the given capacity (number of ASCII chars).
// If capacity <= 0, a minimum of 1 is used.
// Returns true on success, false on allocation failure.
bool memory_init(size_t capacity);

// Release internal resources.
void memory_shutdown(void);

// Remove all data but keep capacity.
void memory_clear(void);

// Stats
size_t memory_capacity(void);
size_t memory_size(void);
bool memory_is_empty(void);
bool memory_is_full(void);

// Write one full entry (ascii encoded value). Index and timestamp are filled internally.
// Optionally returns the index and timestamp used.
bool memory_write_entry(uint8_t ascii, uint32_t *out_index, uint64_t *out_ts);

// Extended: write an entry including the writer key for debug/snapshot purposes
bool memory_write_entry_with_key(uint8_t ascii, uint8_t key_used, uint32_t *out_index, uint64_t *out_ts);

// Read one full entry (FIFO). Returns true if read.
bool memory_read_entry(MemEntry *out);

// Convenience wrappers for compatibility with char-based ops.
bool memory_write_char(char c);
size_t memory_write(const char *s);
bool memory_read_char(char *out);

// Read up to max characters. Returns number actually read.
size_t memory_read(char *out, size_t max);

// Peek at character at logical index (0 is the oldest element) without removing it.
// Returns true on success.
bool memory_peek(size_t index, char *out);

// Cross-process shared memory initialization using OS IPC primitives (no busy waiting).
// name: identifier for the shared memory and synchronization objects.
// capacity: number of ASCII characters that can be stored simultaneously.
// out_created (optional): set to true if this call created the shared region, false if attached to an existing one.
// Returns true on success. On Windows uses named file mapping + named semaphore/mutex; on POSIX uses shm_open + sem_open.
bool memory_init_shared(const char *name, size_t capacity, bool *out_created);

// Debug: print a snapshot of the current memory contents from oldest to newest.
// Shows size/capacity and, for each occupied slot: logical order, slot index, ascii, and timestamp.
void memory_debug_print_snapshot(void);

#endif // MEMORY_H
