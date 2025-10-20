#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>    // Para ftruncate, close
#include <fcntl.h>     // Para O_CREAT, O_RDWR
#include <sys/mman.h>  // Para shm_opne, mmap
#include <sys/stat.h>  // Para modos (0666)
#include <semaphore.h> // Para sem_open, sem_close
#include "memInfo.h"   // Archivo de cabecera

// Funcion para imprimir errores y salir
void reportar_error_y_salir(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Funcion auxiliar para leer una linea de forma segura y quitar el newline
void leer_linea(char *buffer, int size) {
    if (fgets(buffer, size, stdin) != NULL) {
        buffer[strcspn(buffer, "\n")] = 0;  // Quitar el salto de linea (newline) al final
    } else {
        reportar_error_y_salir("Error leyendo la entrada");
    }
}

int main(int argc, char *argv[]) {
    // --- Declarar variables para almacenar la entrada ---
    char shm_name[256];
    char buffer_size_str[50]; // Buffer temporar para leer el numero
    char source_file[256];
    int buffer_size;
    char llave_str[10];
    int llave_num;

    // --- Solicitar Parametros al Usuario ---
    printf("--- Configuracion del Inicializador ---\n");

    // 1. Identificador SHM
    printf("Ingrese el identificador del espacio compartido: ");
    fflush(stdout);     // Aseguramos que el prompt se muestre
    leer_linea(shm_name, sizeof(shm_name));

    // 2. Tamano del buffer
    printf("Ingrese la cantidad de espacios del buffer: ");
    fflush(stdout);
    leer_linea(buffer_size_str, sizeof(buffer_size_str));
    buffer_size = atoi(buffer_size_str);    // Convertir string a entero

    // 3. Llave
    printf("Ingrese la llave para desencriptar: ");
    fflush(stdout);
    leer_linea(llave_str, sizeof(llave_str));
    llave_num = atoi(llave_str);

    if (llave_num < 0 || llave_num > 255){
        fprintf (stderr, "La llave debe ser un numero de 8 bits [0, 255]");
        exit(EXIT_FAILURE);
    }

    // 4. Archivo fuente
    printf("Ingrese el nombre del archivo fuente: ");
    fflush(stdout);
    leer_linea(source_file, sizeof(source_file));

    if (buffer_size <= 0) {
        fprintf(stderr, "El tamano del buffer debe ser mayor que 0.\n");
        exit(EXIT_FAILURE);
    }

    // Generar nombres para los semaforos basados en el ID de la memoria
    char sem_mutex_name[512], sem_empty_name[512], sem_full_name[512], sem_fin_name[512];

    int needed;
    needed = snprintf(sem_mutex_name, sizeof(sem_mutex_name), "%s%s", shm_name, SEM_MUTEX_NAME_SUFFIX);
    if (needed < 0 || needed >= (int)sizeof(sem_mutex_name)) {
        fprintf(stderr, "Error: sem_mutex_name truncation or encoding error (needed=%d, size=%zu)\n", needed, sizeof(sem_mutex_name));
        exit(EXIT_FAILURE);
    }

    needed = snprintf(sem_empty_name, sizeof(sem_empty_name), "%s%s", shm_name, SEM_EMPTY_NAME_SUFFIX);
    if (needed < 0 || needed >= (int)sizeof(sem_empty_name)) {
        fprintf(stderr, "Error: sem_empty_name truncation or encoding error (needed=%d, size=%zu)\n", needed, sizeof(sem_empty_name));
        exit(EXIT_FAILURE);
    }

    needed = snprintf(sem_full_name, sizeof(sem_full_name), "%s%s", shm_name, SEM_FULL_NAME_SUFFIX);
    if (needed < 0 || needed >= (int)sizeof(sem_full_name)) {
        fprintf(stderr, "Error: sem_full_name truncation or encoding error (needed=%d, size=%zu)\n", needed, sizeof(sem_full_name));
        exit(EXIT_FAILURE);
    }

    needed = snprintf(sem_fin_name, sizeof(sem_fin_name), "%s%s", shm_name, SEM_FIN_NAME_SUFFIX);
    if (needed < 0 || needed >= (int)sizeof(sem_fin_name)) {
        fprintf(stderr, "Error: sem_fin_name truncation or encoding error (needed=%d, size=%zu)\n", needed, sizeof(sem_full_name));
        exit(EXIT_FAILURE);
    }

    printf("\n--------------------------------\n");
    printf("--- Resumen de Configuracion ---\n");
    printf("--------------------------------\n");
    printf("Iniciando recursos con ID base: %s\n", shm_name);
    printf("\t -> Buffer size: %d\n", buffer_size);
    printf("\t -> Llave: %d\n", llave_num);
    printf("\t -> Archivo: %s\n", source_file);
    printf("--------------------------------\n");

    // --- Limpiar recursos antiguos ---
    shm_unlink(shm_name);
    sem_unlink(sem_mutex_name);
    sem_unlink(sem_empty_name);
    sem_unlink(sem_full_name);
    sem_unlink(sem_fin_name);

    // --- Crear Memoria Compartida (SHM) ---
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) reportar_error_y_salir("Error en shm_open");

    size_t total_size = sizeof(struct MemoriaCompartida) + (buffer_size * sizeof(struct CharInfo));

    if (ftruncate(shm_fd, total_size) == -1) reportar_error_y_salir("Error en ftruncate");

    struct MemoriaCompartida *memoria = (struct MemoriaCompartida *)mmap(
        NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0
    );

    if (memoria == MAP_FAILED) reportar_error_y_salir("Error en mmap");

    // --- Crear Semaforos ---
    sem_t *sem_mutex = sem_open(sem_mutex_name, O_CREAT, 0666, 1);
    if (sem_mutex == SEM_FAILED) reportar_error_y_salir("Error en sem_open (mutex)");

    sem_t *sem_empty = sem_open(sem_empty_name, O_CREAT, 0666, buffer_size);
    if (sem_empty == SEM_FAILED) reportar_error_y_salir("Error en sem_open (empty)");

    sem_t *sem_full = sem_open(sem_full_name, O_CREAT, 0666, 0);
    if (sem_full == SEM_FAILED) reportar_error_y_salir("Error en sem_open (full)");

    sem_t *sem_fin = sem_open(sem_fin_name, O_CREAT, 0666, 0);
    if (sem_fin == SEM_FAILED) reportar_error_y_salir("Error en sem_open (full)");

    // --- Inicializar Valores en Memoria Compartida ---
    printf("Inicializando estructura de memoria compartida...\n");
    memoria->buffer_size = buffer_size;
    memoria->idx_escritura = 0;
    memoria->idx_lectura = 0;
    memoria->idx_archivo_lectura = 0;
    memoria->idx_archivo_escritura = 0;
    memoria->total_producidos = 0;
    memoria->total_consumidos = 0;
    memoria->shutdown_flag = 0;
    memoria->emisores_activos = 0;
    memoria->receptores_activos = 0;
    memoria->emisores_totales = 0;
    memoria->receptores_totales = 0;
    memoria->llave_desencriptar = (unsigned char)llave_num;
    strncpy(memoria->archivo_fuente, source_file, sizeof(memoria->archivo_fuente) - 1);

    memset(memoria->buffer, 0, buffer_size * sizeof(struct CharInfo));

    // --- Limpieza del proceso inicializador ---
    sem_close(sem_mutex);
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_close(sem_fin);

    munmap(memoria, total_size);
    close(shm_fd);

    printf("Inicializacion completa. Los recursos IPC estan listos.\n");

    return EXIT_SUCCESS;
}