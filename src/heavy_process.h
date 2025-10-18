
#ifndef HEAVY_PROCESS_H
#define HEAVY_PROCESS_H

#include <stdbool.h>
#ifdef _WIN32
#include <process.h>
typedef int pid_t;
#else
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Lanza ./proc_inicializador --create --buffer <cap> --name <name>
pid_t launch_inicializador_heavy(const char *name, int buffer_capacity);

// Lanza ./proc_emisor --input <file> --key-bits <bits> [--auto] --name <name>
pid_t launch_emisor_heavy(const char *name, const char *input_path, const char *key_bits, bool automatic);

// Lanza ./proc_receptor --key-bits <bits> [--auto] [--out <file>] --name <name>
pid_t launch_receptor_heavy(const char *name, const char *key_bits, bool automatic, const char *out_path);

// Lanza ./proc_finalizador [--total <N>] --name <name>
pid_t launch_finalizador_heavy(const char *name, int total_chars_written);

// Espera a que termine el proceso y devuelve exit code (0 si ok).
int wait_process(pid_t pid, int *out_exit_code);

#ifdef __cplusplus
}
#endif
#endif
