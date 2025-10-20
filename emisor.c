#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // Para ftruncate, close
#include <fcntl.h>      // Para O_RDWR
#include <sys/mman.h>   // Para shm_opne, mmap
#include <sys/stat.h>   // Para fstat
#include <sys/wait.h>   // Para wait
#include <semaphore.h>  // Para sem_open, sem_wait, sem_post
#include <errno.h>      // Para EINTR
#include <time.h>       // Para time, strftime
#include <ctype.h>      // Para isprint
#include "memInfo.h"    // Archivo de cabecera

// --- Codigos de color ANSI para la impresion elegante
#define ANSI_COLOR_CYAN     "\x1b[36m"    
#define ANSI_COLOR_YELLOW   "\x1b[33m"
#define ANSI_COLOR_GREEN    "\x1b[32m"
#define ANSI_COLOR_MAGENTA  "\x1b[35m"
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

    const char* colors[] = { ANSI_COLOR_CYAN, ANSI_COLOR_GREEN, ANSI_COLOR_YELLOW, ANSI_COLOR_MAGENTA };
    const char* color = colors[getpid() % 4];   // Elige un color basado en el PID

    /* Use a literal format string to avoid -Wformat-security warnings */
    printf("%s[EMISOR (PID: %d)]%s -> | ", color, getpid(), ANSI_COLOR_RESET);
    printf("Original: " ANSI_COLOR_YELLOW "'%c'" ANSI_COLOR_RESET " | ", original_printable);
    printf("Cifrado: " ANSI_COLOR_GREEN "'%c' (0x%02X)" ANSI_COLOR_RESET " | ", cifrado_printable, (unsigned char)info->valor_ascii);
    /* Print index with matching format and argument */
    printf("Indice: %-4d | ", info->indice);
    printf("Hora: %s |\n", time_str);
}


// Logica principal del emisor - Cada proceso HIJO (creado por fork) ejecutara esta funcion
void emisor_worker(const char* shm_name, const char* modo_ejecucion) {
    // Validar modo
    int modo_manual = 0;
    if (strcmp(modo_ejecucion, "manual") == 0) {
        modo_manual = 1;
        printf (ANSI_COLOR_CYAN "[WORKER (PID: %d)] Modo: Manual\n" ANSI_COLOR_RESET, getpid());
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

    // --- Abrir el archivo guente (cada hijo abre su propia copia) ---
    FILE *archivo_fuente = fopen(memoria->archivo_fuente, "r");
    if (archivo_fuente == NULL) {
        fprintf(stderr, "Error (PID %d) al abrir el archivo fuente: %s\n", getpid(), memoria->archivo_fuente);
        reportar_error_y_salir("fopen");
    }

    unsigned char clave_codificar = memoria->llave_desencriptar;

    if (sem_wait(sem_mutex) == -1) reportar_error_y_salir("sem_wait (mutex register)");
    memoria->emisores_activos++;
    if (sem_post(sem_mutex) == -1) reportar_error_y_salir("sem_post (mutex register)");

    // --- Loop Principal del emisor ---
    for (;;) {
        int char_leido;
        int mi_indice_archivo;

        // --- INICIO SECCION CRITICA (INDICE DE ARCHIVO) ---
        if (sem_wait(sem_mutex) == -1) {
            if (errno == EINTR) continue;
            reportar_error_y_salir("sem_wait (mutex)");
        }
        
        // --- CHEQUEO DE CIERRE ---
        if (memoria->shutdown_flag) {
            sem_post(sem_mutex);
            break;
        }

        mi_indice_archivo = memoria->idx_archivo_lectura;
        memoria->idx_archivo_lectura++;

        if (fseek(archivo_fuente, mi_indice_archivo, SEEK_SET) != 0) {
            sem_post(sem_mutex);
            break; 
        }

        char_leido = fgetc(archivo_fuente);
        
        if (sem_post(sem_mutex) == -1) reportar_error_y_salir("sem_post (mutex)");
        // --- FIN SECCION CRITICA (INDICE DE ARCHIVO)---

        if (char_leido == EOF) break;
        if (char_leido == '\n' || char_leido == '\r') continue;
        if (modo_manual) {
            printf(ANSI_COLOR_YELLOW "[EMISOR HIJO (PID: %d)] Presiones ENTER para insertar '%c'...\n" ANSI_COLOR_RESET, getpid(), (char)char_leido);
            getchar();
        }

        // --- INICIO LOGICA DE BLOQUEO ---
        if (sem_wait(sem_empty) == -1) {
            if (errno == EINTR) continue;
            reportar_error_y_salir("sem_wait (empty)");
        }
        // --- FIN LOGICA DE BLOQUE ---

        // --- INICIO SECCION CRITICA (ESCRITURA DE BUFFER) ---
        if (sem_wait(sem_mutex) == -1) {
            if (errno == EINTR) continue;
            reportar_error_y_salir("sem_wait (mutex)");
        }

        // --- CHEQUEO DE CIERRE (DOBLE) ---
        if (memoria->shutdown_flag) {
            sem_post(sem_mutex);
            sem_post(sem_empty);
            break;
        }

        int indice_escritura_buffer = memoria->idx_escritura;

        struct CharInfo item;
        item.valor_ascii = (char)char_leido ^ clave_codificar;
        item.indice = indice_escritura_buffer;
        item.timestamp = time(NULL);

        memoria->buffer[indice_escritura_buffer] = item;
        memoria->idx_escritura = (indice_escritura_buffer + 1) % memoria->buffer_size;
        memoria->total_producidos++;

        if (sem_post(sem_mutex) == -1) reportar_error_y_salir ("sem_post (mutex)");
        // --- FIN SECCION CRITICA (ESCRITURA DE BUFFER) ---

        // Senalizar que hay un nuevo espacio lleno
        if (sem_post(sem_full) == -1) reportar_error_y_salir("sem_post (full)");

        // Imprimir informacion
        imprimir_produccion(&item, (char)char_leido);
    }

    // --- Limpieza del proceso hijo ---
    printf(ANSI_COLOR_CYAN  "--------------------------------------------------------------------------------------" ANSI_COLOR_RESET "\n");

    if (sem_wait(sem_mutex) == -1) reportar_error_y_salir("sem_wait (mutex unregister)");
    memoria->emisores_activos--;
    int emisores_vivos = memoria->emisores_activos;
    int receptores_vivos = memoria->receptores_activos;
    if (sem_post(sem_mutex) == -1 ) reportar_error_y_salir("sem_post (mutex unregister)");

    if (emisores_vivos == 0 && receptores_vivos == 0) {
        printf(ANSI_COLOR_YELLOW "PID: %d ¡SOY EL ÚLTIMO! Avisando al finalizador.\n" ANSI_COLOR_RESET, getpid());
        if (sem_post(sem_fin) == -1) reportar_error_y_salir("sem_post (fin)");
    }

    fclose(archivo_fuente);
    munmap(memoria, total_size);
    close(shm_fd);
    sem_close(sem_mutex);
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_close(sem_fin);
    exit(EXIT_SUCCESS);
}

// Parsea los argumentos - Proceso PADRE que crea N procesos hijos
int main(int argc, char *argv[]){
    // --- Validar argumentos ---
    if (argc !=  4) {
        fprintf(stderr, "Uso: %s <shm_id> <modo (manual|automatico)> <num_emisores>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* shm_name = argv[1];
    const char* modo_ejecucion = argv[2];
    int num_emisores = atoi(argv[3]);

    if (num_emisores <= 0) {
        fprintf(stderr, "Error: El numero de emisores debe ser 1 o mas.\n");
        exit(EXIT_FAILURE);
    }

    printf(ANSI_COLOR_GREEN "--- Lanzador de Emisores (PID: %d) ---" ANSI_COLOR_RESET, getpid());
    printf("Lanzando %d procesos emisores (heavy process)...\n", num_emisores);

    // --- Imprimir encabezado de la tabla ---
    printf("\n" ANSI_COLOR_CYAN "--------------------------------------------------------------------------------------" ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_CYAN "%-20s | %-12s | %-20s | %-8s | %-10s |\n" ANSI_COLOR_RESET,
           "PROCESO", "ORIGINAL", "CIFRADO (HEX)", "ÍNDICE", "HORA");
    printf(ANSI_COLOR_CYAN "--------------------------------------------------------------------------------------" ANSI_COLOR_RESET "\n");

    // --- Conectar a SHM y Mutex (SÓLO EL PADRE) ---
    char sem_mutex_name[512];
    snprintf(sem_mutex_name, sizeof(sem_mutex_name), "%s%s", shm_name, SEM_MUTEX_NAME_SUFFIX);
    sem_t *sem_mutex = sem_open(sem_mutex_name, 0);
    if (sem_mutex == SEM_FAILED) reportar_error_y_salir("Padre: sem_open (mutex)");

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

    // --- Registrar el total de emisores ---
    if (sem_wait(sem_mutex) == -1) reportar_error_y_salir("Padre: sem_wait (mutex)");
    memoria->emisores_totales += num_emisores;
    if (sem_post(sem_mutex) == -1) reportar_error_y_salir("Padre: sem_post (mutex)");
    
    // Desmapear y cerrar semáforo del padre
    munmap(memoria, total_size);
    sem_close(sem_mutex);

    for (int i = 0; i < num_emisores; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            reportar_error_y_salir("Error en fork()");
        } else if (pid == 0) {
            // --- PROCESO HIJO ---
            // Heavy process

            // Paso de argumentos que el padre parseo
            emisor_worker(shm_name, modo_ejecucion);

            // El hijo termina aqui para no continuar en el bucle 'for' del padre
            exit(EXIT_SUCCESS);
        }

        printf(ANSI_COLOR_GREEN "[PADRE (PID: %d)] Creado emisor hijo con PID: %d\n" ANSI_COLOR_RESET, getpid(), pid);
    }

    // --- Espera del Padre ---
    printf(ANSI_COLOR_GREEN "[PADRE (PID: %d)] Todos los hijos lanzados. Esperando a que terminen...\n" ANSI_COLOR_RESET, getpid());

    int status;
    pid_t wpid;

    while ((wpid = wait(&status)) > 0) {
        printf(ANSI_COLOR_GREEN "[PADRE (PID: %d)] Hijo %d ha terminado. \n" ANSI_COLOR_RESET, getpid(), wpid);
    }

    printf(ANSI_COLOR_GREEN "--- Emisor (PID: %d): todos los emisores han terminado --- \n" ANSI_COLOR_RESET, getpid());

    return EXIT_SUCCESS;
}