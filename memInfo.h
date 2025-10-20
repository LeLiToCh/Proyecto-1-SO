#ifndef MEMINFO_H
#define MEMINFO_H

#include <time.h>
#include <semaphore.h>

struct CharInfo {
    char valor_ascii;   // Valor del caracter
    int indice;         // Posicion donde fue insertado
    time_t timestamp;   // Hora de insercion
};

struct MemoriaCompartida {
    int buffer_size;                // Tamano N del buffer
    int idx_escritura;              // Indice donde escribira el proximo caracter
    int idx_lectura;                // Indice donde leera el proximo caracter

    int idx_archivo_lectura;        // Indice global para la lectura del archivo fuente
    int idx_archivo_escritura;      // Indice global para la escritura del archivo final

    // --- Informacion solicitada ---
    char llave_desencriptar[128];
    char archivo_fuente[256];

    // --- Auditoria ---
    int total_producidos;
    int total_consumidos;

    volatile int shutdown_flag;     // 1 = Apagar, 0 = Correr
    volatile int emisores_activos;
    volatile int receptores_activos;
    int emisores_totales;
    int receptores_totales;


    // --- Buffer (Array flexible) ---
    struct CharInfo buffer[]; 
};


// --- Nombres para recursos IPC ---
#define SEM_MUTEX_NAME_SUFFIX "_mutex"
#define SEM_EMPTY_NAME_SUFFIX "_empty"
#define SEM_FULL_NAME_SUFFIX "_full"
#define SEM_FIN_NAME_SUFFIX "_fin"

#endif // MEMINFO_H