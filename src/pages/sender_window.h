// Simple sender window helper API
#pragma once
#include <stdbool.h>

// Start a standalone sender window that allows selecting a file and processing it.
// ident: base identifier for shared memory (will be used to create a unique region),
// cantidad: number of pages/size hint, clave: 8-char binary key, automatic: automatic mode
void sender_window_start_async(const char *identificador, int cantidad, const char *clave, bool automatic);
