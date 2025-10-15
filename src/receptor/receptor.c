// RECEPTOR
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define SHM_SIZE 1024

// Lectura automatica
void xor_decrypt_auto(char *data, char *mensaje, int key, int delay) {
    int j = 0;
    for (int i = 0; data[i] != '\0'; i++){
        // solicitar slot
        char character = data[i] ^ key;
        mensaje[j++] = character;
        data[i] = '_';
        // liberar slot
        printf("Leyendo caracter [%d]: \t%c\n", i, character);
        sleep(delay);
    }
    mensaje[j] = '\0';
}

// Lectura manual
void xor_decrypt_manual(char *data, char *mensaje, int key){
    int j = 0;
    for (int i = 0; data[i] != '\0'; i++){
        // solicitar slot
        char character = data[i] ^ key;
        mensaje[j++] = character;
        data[i] = '_';
        // liberar slot
        printf("Leyendo caracter [%d]: \t%c \t", i, character);
        printf("Presione ENTER para leer siguiente caracter.\n");
        getchar();
    }
    mensaje[j] = '\0';
}

int main(int argc, char *argv[]) {
    if (argc < 3){
        fprintf(stderr, "Uso: %s <modo_operacion <clave> [tiempo]\n", argv[0]);
        fprintf(stderr, "Modo: 1=Automatico, 2=Manual\n");
        return 1;
    }

    int modo = atoi(argv[1]);
    int clave = atoi(argv[2]);
    int delay = (argc == 4) ? atoi(argv[3]) : 1;

    printf("=== RECEPTOR ===\n");
    printf("Modo de operacion: %s\n", (modo == 1) ? "Automatico" : "Manual");
    printf("Clave recibida: %d\n", clave);

    pid_t pid = fork();

    if (pid < 0) {
        perror("Error al crear proceso");
        exit(1);
    }

    if (pid == 0) {
        printf("Hijo: PID = %d, PPID (padre) = %d\n", getpid(), getppid());
        key_t key = ftok("sharedfile", 65);
        int shmid = shmget(key, SHM_SIZE, 0666);
        if (shmid == -1) {
            perror("Error al acceder a memoria compartida");
            exit(1);
        }

        char *mem = (char *)shmat(shmid, NULL, 0);
        if (mem == (char *)-1) {
            perror("Error al adjuntar memoria compartida");
            exit(1);
        }

        char mensaje[SHM_SIZE];

        printf("Hijo: descifrando mensaje, iniciando lectura %s...\n", (modo == 1) ? "automática" : "manual");

        if (modo == 1) {
            xor_decrypt_auto(mem, mensaje, clave, delay);
        } else if (modo == 2) {
            xor_decrypt_manual(mem, mensaje, clave);
        } else {
            perror("Modo de operacion: 1=Automatico, 2=Manual");
        }


        printf("Hijo: mensaje recibido y descifrado: %s\n", mensaje);

        exit(0);
    } else {
        printf("Padre: esperando que el hijo termine...\n");
        wait(NULL);
        printf("Padre: proceso hijo finalizado.\n");
    }

    return 0;
}