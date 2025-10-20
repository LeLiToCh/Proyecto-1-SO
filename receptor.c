#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // Para ftruncate, close
#include <fcntl.h>      // Para O_RDWR
#include <sys/mman.h>   // Para shm_opne, mmap
#include <sys/stat.h>   // Para fstat
#include <sys/wait.h>   // Para wait
#include <semaphore.h>  // Para sem_open, sem_wait, sem_post
#include <time.h>       // Para time, strftime
#include <ctype.h>      // Para isprint
#include <errno.h>      // Para errno, EINTR
#include "memInfo.h"    // Archivo de cabecera

// --- Codigos de color ANSI para la impresion elegante
#define ANSI_COLOR_BLUE     "\x1b[34m"    
#define ANSI_COLOR_YELLOW   "\x1b[33m"
#define ANSI_COLOR_GREEN    "\x1b[32m"
#define ANSI_COLOR_RED      "\x1b[31m"
#define ANSI_COLOR_RESET    "\x1b[0m"

// Funcion para imprimir errores y salir
void reportar_error_y_salir(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Imprime de forma tabular y con colores la informacion del caracter producido
void imprimir_produccion(const struct CharInfo* info, char char_original) {
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&info->timestamp));
    char original_printable = isprint(char_original) ? char_original : '?';
    char cifrado_printable = isprint(info->valor_ascii) ? info->valor_ascii : '?';

    const char* colors[] = { ANSI_COLOR_BLUE, ANSI_COLOR_GREEN, ANSI_COLOR_YELLOW, ANSI_COLOR_RED };
    const char* color = colors[getpid() % 4];   // Elige un color basado en el PID

    /* Use a literal format string to avoid -Wformat-security warnings */
    printf("%s[RECEPTOR (PID: %d)]%s -> | ", color, getpid(), ANSI_COLOR_RESET);
    printf("Original: " ANSI_COLOR_YELLOW "'%c'" ANSI_COLOR_RESET " | ", original_printable);
    printf("Cifrado: " ANSI_COLOR_GREEN "'%c' (0x%02X)" ANSI_COLOR_RESET " | ", cifrado_printable, (unsigned char)info->valor_ascii);
    /* Print index with matching format and argument */
    printf("Indice: %-4d | ", info->indice);
    printf("Hora: %s |\n", time_str);
}

void receptor_worker(const char* shm_name, const char* modo_ejecucion, const char* archivo_salida_nombre) {
    // Validar modo
    int modo_manual = 0;
    if (strcmp(modo_ejecucion, "manual") == 0) {
        modo_manual = 1;
        printf (ANSI_COLOR_BLUE "[WORKER (PID: %d)] Modo: Manual\n" ANSI_COLOR_RESET, getpid());
    } else if (strcmp(modo_ejecucion, "automatico") != 0) {
        fprintf(stderr, "[ERROR (PID %d)]: Modo debe ser 'manual' o 'automatico'.\n", getpid());
        exit(EXIT_FAILURE);
    }

    // --- Generar Nombres de Semaforos ---
    char sem_mutex_name[512], sem_empty_name[512], sem_full_name[512], sem_fin_name[512];
    int r;
    r = snprintf(sem_mutex_name, sizeof(sem_mutex_name), "%s%s", shm_name, SEM_MUTEX_NAME_SUFFIX);
    if (r < 0 || (size_t)r >= sizeof(sem_mutex_name)) reportar_error_y_salir("sem name snprintf (mutex) truncated");
    r = snprintf(sem_empty_name, sizeof(sem_empty_name), "%s%s", shm_name, SEM_EMPTY_NAME_SUFFIX);
    if (r < 0 || (size_t)r >= sizeof(sem_empty_name)) reportar_error_y_salir("sem name snprintf (empty) truncated");
    r = snprintf(sem_full_name, sizeof(sem_full_name), "%s%s", shm_name, SEM_FULL_NAME_SUFFIX);
    if (r < 0 || (size_t)r >= sizeof(sem_full_name)) reportar_error_y_salir("sem name snprintf (full) truncated");
    r = snprintf(sem_fin_name, sizeof(sem_fin_name), "%s%s", shm_name, SEM_FIN_NAME_SUFFIX);
    if (r < 0 || (size_t)r >= sizeof(sem_fin_name)) reportar_error_y_salir("sem name snprintf (fin) truncated");

    // --- Conectar a los Recursos IPC ---
    sem_t *sem_mutex = sem_open(sem_mutex_name, 0);
    if (sem_mutex == SEM_FAILED) reportar_error_y_salir("Error en sem_open (mutex)");
    sem_t *sem_empty = sem_open(sem_empty_name, 0);
    if (sem_empty == SEM_FAILED) reportar_error_y_salir("Error en sem_open (empty)");
    sem_t *sem_full = sem_open(sem_full_name, 0);
    if (sem_full == SEM_FAILED) reportar_error_y_salir("Error en sem_open (full)");
    sem_t *sem_fin = sem_open(sem_fin_name, 0);
    if (sem_fin == SEM_FAILED) reportar_error_y_salir("Error en sem_open (fin)");

    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) reportar_error_y_salir("Error en shm_open");

    struct stat shm_stat;
    if (fstat(shm_fd, &shm_stat) == -1) reportar_error_y_salir("fstat");
    size_t total_size = shm_stat.st_size;

    struct MemoriaCompartida *memoria = (struct MemoriaCompartida *)mmap(
        NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0
    );

    if (memoria == MAP_FAILED) reportar_error_y_salir("mmap");

    // --- Abrir el archivo (cada hijo abre su propia copia) ---
    FILE *archivo_salida = fopen(archivo_salida_nombre, "r+");
    if (archivo_salida == NULL) {
        fprintf(stderr, "Error (PID %d) al abrir el archivo salida: %s\n", getpid(), archivo_salida_nombre);
        reportar_error_y_salir("fopen");
    }

    unsigned char clave_decodificar = memoria->llave_desencriptar;

    if (sem_wait(sem_mutex) == -1) reportar_error_y_salir("sem_wait (mutex register)");
    memoria->receptores_activos++;
    if (sem_post(sem_mutex) == -1) reportar_error_y_salir("sem_post (mutex register)");

    // --- Loop Principal del receptor ---
    for (;;) {
        // --- BLOQUE ---
        if (sem_wait(sem_full) == -1) {
            if (errno == EINTR) continue;
            reportar_error_y_salir("sem_wait (full)");
        }
        
        if (modo_manual) {
            printf(ANSI_COLOR_YELLOW "[RECEPTOR HIJO (PID: %d)] Presione ENTER para consumir item...\n" ANSI_COLOR_RESET, getpid());
            getchar();
        }

        // --- INICIO SECCION CRITICA (LECTURA DE BUFFER) ---
        if (sem_wait(sem_mutex) == -1) {
            if (errno == EINTR) continue;
            reportar_error_y_salir("sem_wait (mutex)");
        }

        if (memoria->shutdown_flag) {
            sem_post(sem_mutex);
            sem_post(sem_full);
            break;
        }

        int indice_lectura_buffer = memoria->idx_lectura;
        struct CharInfo item = memoria->buffer[indice_lectura_buffer];
        memoria->idx_lectura = (indice_lectura_buffer + 1) % memoria->buffer_size;

        int mi_indice_archivo_salida = memoria->idx_archivo_escritura;
        memoria->idx_archivo_escritura++;
        
        memoria->total_consumidos++;

        if (sem_post(sem_mutex) == -1) reportar_error_y_salir("sem_post (mutex)");
        // --- FIN SECCION CRITICA (LECTURA DE BUFFER) ---

        // Senalizar espacio vacio
        if (sem_post(sem_empty) == -1) reportar_error_y_salir("sem_post (empty)");

        // Decodificar el Item (fuera de la seccion critica)
        char cahr_decodificado = item.valor_ascii ^ clave_decodificar;

        if (fseek(archivo_salida, mi_indice_archivo_salida, SEEK_SET) != 0) {
            reportar_error_y_salir("fseek (archivo salida)");
        }

        if (fputc(cahr_decodificado, archivo_salida) == EOF) {
            reportar_error_y_salir("fputc (archivo salida)");
        }

        fflush(archivo_salida);

        imprimir_produccion(&item, cahr_decodificado);
    }

    // --- Limpieza del proceso hijo ---
    printf(ANSI_COLOR_BLUE  "--------------------------------------------------------------------------------------" ANSI_COLOR_RESET "\n");

    if (sem_wait(sem_mutex) == -1) reportar_error_y_salir("sem_wait (mutex unregister)");
    memoria->receptores_activos--;
    int emisores_vivos = memoria->emisores_activos;
    int receptores_vivos = memoria->receptores_activos;
    if (sem_post(sem_mutex) == -1 ) reportar_error_y_salir("sem_post (mutex unregister)");

    if (emisores_vivos == 0 && receptores_vivos == 0) {
        printf(ANSI_COLOR_YELLOW "PID: %d ¡SOY EL ÚLTIMO! Avisando al finalizador.\n" ANSI_COLOR_RESET, getpid());
        if (sem_post(sem_fin) == -1) reportar_error_y_salir("sem_post (fin)");
    }

    fclose(archivo_salida);
    munmap(memoria, total_size);
    close(shm_fd);
    sem_close(sem_mutex);
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_close(sem_fin);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    // --- Validar argumentos ---
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <shm_id> <modo (manual|automatico)> <num_receptores>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* shm_name = argv[1];
    const char* modo_ejecucion = argv[2];
    int num_receptores = atoi(argv[3]);

    const char* dir_salida = "files";
    const char* archivo_salida_nombre = "files/output.txt";

    if (mkdir(dir_salida, 0777) == -1) {
        if(errno != EEXIST) {
            reportar_error_y_salir("mkdir");
        }
    }

    FILE *fp = fopen(archivo_salida_nombre, "w");
    if (fp == NULL) {
        reportar_error_y_salir("fopen (truncar en main)");
    }
    fclose(fp);

    if (num_receptores <= 0) {
        fprintf(stderr, "Error: El numero de receptores debe ser 1 o mas.\n");
        exit(EXIT_FAILURE);
    }

    printf(ANSI_COLOR_GREEN "--- Lanzador de Receptores (PID: %d) ---" ANSI_COLOR_RESET, getpid());
    printf("Lanzando %d procesos receptores (heavy process)...\n", num_receptores);

    // --- Imprimir encabezado de la tabla ---
    printf("\n" ANSI_COLOR_BLUE "--------------------------------------------------------------------------------------" ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_BLUE "%-20s | %-12s | %-20s | %-8s | %-10s |\n" ANSI_COLOR_RESET,
           "PROCESO", "ORIGINAL", "CIFRADO (HEX)", "ÍNDICE", "HORA");
    printf(ANSI_COLOR_BLUE "--------------------------------------------------------------------------------------" ANSI_COLOR_RESET "\n");

    // --- Conectar a SHM y Mutex (SÓLO EL PADRE) ---
    char sem_mutex_name[512];
    snprintf(sem_mutex_name, sizeof(sem_mutex_name), "%s%s", shm_name, SEM_MUTEX_NAME_SUFFIX);
    sem_t *sem_mutex = sem_open(sem_mutex_name, 0);
    // ... (Error handling) ...
    
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) reportar_error_y_salir("Padre: shm_open");

    struct stat shm_stat;
    if (fstat(shm_fd, &shm_stat) == -1) reportar_error_y_salir("fstat");
    size_t total_size = shm_stat.st_size;
    
    struct MemoriaCompartida *memoria = (struct MemoriaCompartida *)mmap(
        NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0
    );
    if (memoria == MAP_FAILED) reportar_error_y_salir("Padre: mmap");
    close(shm_fd);
    
    // --- Registrar el total de receptores ---
    if (sem_wait(sem_mutex) == -1) reportar_error_y_salir("Padre: sem_wait (mutex)");
    memoria->receptores_totales += num_receptores;
    if (sem_post(sem_mutex) == -1) reportar_error_y_salir("Padre: sem_post (mutex)");
    
    munmap(memoria, total_size);
    sem_close(sem_mutex);
    
    for (int i = 0; i < num_receptores; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            reportar_error_y_salir("Error en fork()");
        } else if (pid == 0) {
            // --- PROCESO HIJO ---
            // Heavy process

            // Paso de argumentos que el padre parseo
            receptor_worker(shm_name, modo_ejecucion, archivo_salida_nombre);

            // El hijo termina aqui para no continuar en el bucle 'for' del padre
            exit(EXIT_SUCCESS);
        }
        
        printf(ANSI_COLOR_GREEN "[PADRE (PID: %d)] Creado receptor hijo con PID: %d\n" ANSI_COLOR_RESET, getpid(), pid);
    }

    printf(ANSI_COLOR_GREEN "[PADRE (PID: %d)] Todos los hijos lanzados. Esperando a que terminen...\n" ANSI_COLOR_RESET, getpid());
    printf(ANSI_COLOR_GREEN "(El padre y los hijos se bloquearán esperando datos. Use Ctrl+C para terminar)\n" ANSI_COLOR_RESET);

    for (int i = 0; i < num_receptores; i++) {
        int status;
        pid_t wpid = wait(&status); // Llamada BLOQUEANTE
        if (wpid > 0) {
            printf(ANSI_COLOR_GREEN "[PADRE (PID: %d)] Hijo %d ha terminado. \n" ANSI_COLOR_RESET, getpid(), wpid);
        }
    }

    printf(ANSI_COLOR_GREEN "--- Receptor (PID: %d): todos los receptores han terminado --- \n" ANSI_COLOR_RESET, getpid());

    return EXIT_SUCCESS;
}