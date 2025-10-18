/* ============================================================================
 * main.c — Prueba integrada del sistema Heavy Process
 * ----------------------------------------------------------------------------
 * Permite inicializar memoria compartida, cargar un archivo y lanzar procesos
 * independientes que usan la API de memoria y procesamiento.
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "processor.h"
#include "memory.h"

int main(void) {
    printf("=== Proyecto I Sistemas Operativos - Prueba Heavy Process ===\n");

    // --- Configuración inicial ---
    char mem_name[128];
    char file_path[256];
    char key_bits[16];
    size_t capacity;
    int modo;
    bool created = false;

    printf("\nSeleccione modo:\n");
    printf("  1) Memoria local (debug)\n");
    printf("  2) Memoria compartida (heavy process)\n> ");
    scanf("%d", &modo);
    getchar(); // limpiar newline

    if (modo == 2) {
        printf("Ingrese nombre lógico de la memoria compartida (ejemplo: mem_ascii): ");
        fgets(mem_name, sizeof(mem_name), stdin);
        mem_name[strcspn(mem_name, "\n")] = '\0'; // quitar \n

        printf("Capacidad del buffer (número de caracteres): ");
        scanf("%zu", &capacity);
        getchar();

        if (!memory_init_shared(mem_name, capacity, &created)) {
            fprintf(stderr, "❌ Error: no se pudo inicializar la memoria compartida '%s'\n", mem_name);
            return 1;
        }
        printf("✔ Memoria compartida '%s' %s (capacidad=%zu)\n",
               mem_name, created ? "creada" : "adjuntada", memory_capacity());
    } else {
        printf("Capacidad del buffer local: ");
        scanf("%zu", &capacity);
        getchar();
        if (!memory_init(capacity)) {
            fprintf(stderr, "❌ Error al inicializar memoria local.\n");
            return 1;
        }
        printf("✔ Memoria local creada (capacidad=%zu)\n", memory_capacity());
    }

    // --- Configurar archivo y clave ---
    printf("\nRuta del archivo a procesar (texto o binario): ");
    fgets(file_path, sizeof(file_path), stdin);
    file_path[strcspn(file_path, "\n")] = '\0';

    printf("Clave XOR (8 o 9 bits, ej. 10101010): ");
    fgets(key_bits, sizeof(key_bits), stdin);
    key_bits[strcspn(key_bits, "\n")] = '\0';

    int auto_opt;
    printf("Modo automático? (1=Sí, 0=Manual): ");
    scanf("%d", &auto_opt);
    getchar();

    bool automatic = (auto_opt != 0);

    // --- Iniciar procesamiento ---
    printf("\nLanzando proceso pesado (heavy process)...\n");

    if (!processor_start_heavy(file_path, key_bits, automatic)) {
        fprintf(stderr, "❌ Error: no se pudo lanzar el proceso de procesamiento.\n");
        memory_shutdown();
        return 1;
    }

    printf("✔ Procesamiento iniciado correctamente.\n");
    printf("Esperando procesamiento...\n");

    // Simulación de espera activa (solo para pruebas)
    printf("Presione ENTER para ver snapshot final.\n");
    getchar();

    memory_debug_print_snapshot();

    printf("\nFinalizando recursos...\n");
    memory_broadcast_terminate();
    memory_shutdown();

    printf("✅ Ejecución finalizada correctamente.\n");
    return 0;
}
