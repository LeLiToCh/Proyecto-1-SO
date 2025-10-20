#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>     // Para signal() y SIGINT
#include "memInfo.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static volatile sig_atomic_t shutdown_solicitado = 0;

void reportar_error_y_salir(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Manejador de la señal Ctrl+C (SIGINT)
void manejador_sigint(int sig) {
    // Escribimos en la variable global para avisar al main loop
    shutdown_solicitado = 1;
}

int main (int argc, char *argv[]){
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <shm_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* shm_name = argv[1];

    printf("Iniciando Finalizador (PID: %d) para SHM: %s\n", getpid(), shm_name);

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

    signal(SIGINT, manejador_sigint);

    printf(ANSI_COLOR_GREEN "Finalizador listo. Presione Ctrl+C para iniciar el cierre elegante.\n" ANSI_COLOR_RESET);

    for ( ; shutdown_solicitado == 0 ; ) {
        pause(); // Duerme el proceso, no gasta CPU.
    }

    printf(ANSI_COLOR_RED "\n¡Señal Ctrl+C recibida! Iniciando cierre elegante...\n" ANSI_COLOR_RESET);

    // 1. Activar la bandera de cierre
    if (sem_wait(sem_mutex) == -1) reportar_error_y_salir("sem_wait (mutex)");
    memoria->shutdown_flag = 1;
    int total_procesos_esperados = memoria->emisores_totales + memoria->receptores_totales;
    if (sem_post(sem_mutex) == -1) reportar_error_y_salir("sem_post (mutex)");

    printf("Avisando a %d procesos (emisores y receptores)...\n", total_procesos_esperados);

    // 2. Despertar a TODOS los procesos dormidos
    // (Posteamos N veces para asegurarnos de que todos se despierten y vean la bandera)
    for (int i = 0; i < total_procesos_esperados; i++) {
        if (sem_post(sem_empty) == -1) reportar_error_y_salir("sem_post (spam empty)");
        if (sem_post(sem_full) == -1) reportar_error_y_salir("sem_post (spam full)");
    }

    // 3. Esperar a que el ÚLTIMO proceso nos avise (SIN BUSY WAITING)
    printf("Esperando a que el último proceso termine...\n");
    if (sem_wait(sem_fin) == -1) reportar_error_y_salir("sem_wait (fin)");
    
    printf(ANSI_COLOR_GREEN "\n¡Todos los procesos han terminado!\n" ANSI_COLOR_RESET);

    // --- Mostrar Estadísticas ---
    printf("===============================================\n");
    printf(ANSI_COLOR_YELLOW "      ESTADÍSTICAS FINALES DEL SISTEMA\n" ANSI_COLOR_RESET);
    printf("===============================================\n");
    printf("Memoria Compartida ID: \t%s\n", shm_name);
    printf("Tamaño Total de Memoria: \t%ld bytes\n", total_size);
    printf("-----------------------------------------------\n");
    printf("Caracteres Producidos (Total): \t%d\n", memoria->total_producidos);
    printf("Caracteres Consumidos (Total): \t%d\n", memoria->total_consumidos);
    printf("Caracteres en Buffer (Final): \t%d\n", memoria->total_producidos - memoria->total_consumidos);
    printf("-----------------------------------------------\n");
    printf("Emisores (Vivos / Totales): \t%d / %d\n", memoria->emisores_activos, memoria->emisores_totales);
    printf("Receptores (Vivos / Totales): \t%d / %d\n", memoria->receptores_activos, memoria->receptores_totales);
    printf("===============================================\n");

    // --- Limpieza Final de Recursos ---
    printf("Limpiando recursos IPC del sistema...\n");
    munmap(memoria, total_size);
    close(shm_fd);
    
    sem_close(sem_mutex);
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_close(sem_fin);

    // ¡El finalizador es el responsable de borrar todo!
    shm_unlink(shm_name);
    sem_unlink(sem_mutex_name);
    sem_unlink(sem_empty_name);
    sem_unlink(sem_full_name);
    sem_unlink(sem_fin_name);

    printf(ANSI_COLOR_GREEN "Sistema finalizado limpiamente. ¡Adiós!\n" ANSI_COLOR_RESET);
    return EXIT_SUCCESS;

}