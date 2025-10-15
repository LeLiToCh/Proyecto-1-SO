// TEST EMISOR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>


#define SHM_SIZE 1024

void xor_encrypt(char *data, int key) {
    for (int i = 0; data[i] != '\0'; i++) {
        data[i] ^= key;
    }
}

int main() {
    key_t key = ftok("sharedfile", 65);
    int shmid = shmget (key, SHM_SIZE, 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("Error al crear memoria compartida");
        exit(1);
    }

    char *mem = (char *)shmat(shmid, NULL, 0);

    char mensaje[100];
    int clave;

    printf("=== EMISOR ===\n");
    printf("Ingrese un mensaje a enviar: ");
    fgets(mensaje, sizeof(mensaje), stdin);
    mensaje[strcspn(mensaje, "\n")] = '\0';

    printf("Ingrese la clave numerica para XOR: ");
    scanf("%d", &clave);

    xor_encrypt(mensaje, clave);
    strcpy(mem, mensaje);

    printf("Mensaje cifrado guardado en memoria compartida.\n");

    shmdt(mem);
    return 0;

}