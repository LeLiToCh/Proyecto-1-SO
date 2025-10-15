// Test program for receptor: initialize memory, start receptor thread,
// write encoded chars into memory and then post SDL_QUIT to stop.

#include "receptor.h"
#include "../src/memory.h" // Asegúrate de que la ruta a memory.h sea correcta
#include <stdio.h>
#include <SDL.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	(void)argc; (void)argv;

    printf("[INFO] --- INICIO DE RECEPCION ---\\n");
    printf("Identificador: %s\\n", ""); // Placeholder, as state is not used here
    printf("Cantidad: %zu\\n", memory_capacity());
    printf("Clave: %s\\n", ""); // Placeholder
    
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
		fprintf(stderr, "SDL_Init failed: %s\\n", SDL_GetError());
		return 1;
	}

	// Usamos 64 para la capacidad de la memoria.
	if (!memory_init(64)) {
		fprintf(stderr, "memory_init failed\\n");
		SDL_Quit();
		return 1;
	}

	const char *outpath = "test_output.txt";
	const char *key_bits = "00000000"; // Clave XOR = 0 (sin cambio)
    
    printf("Key_bits: %s\\n", key_bits);
    printf("Archivo: %s\\n", outpath);
    printf("Modo: Automatico\\n");
    printf("-------------------------------\\n");

	if (!receptor_start_async(outpath, key_bits, true)) {
		fprintf(stderr, "receptor_start_async failed\\n");
		memory_shutdown();
		SDL_Quit();
		return 1;
	}

	// CORRECCIÓN 1: Aumentar la pausa. Esto asegura que el hilo Receptor 
    // inicie y se BLOQUEE correctamente en memory_read_entry() (sin busy waiting).
	SDL_Delay(1000); 

	const char *msg = "Hola Jose...\nPrueba de funcionamiento del receptor.\n";
	for (size_t i = 0; i < strlen(msg); ++i) {
        // La clave para la escritura es 0x00 para que coincida con la clave de lectura
		if (!memory_write_entry_with_key((uint8_t)msg[i], 0x00, NULL, NULL)) {
			fprintf(stderr, "memory_write failed for char: %c\\n", msg[i]);
			break;
		}
	}
    
    // Pausa para que el receptor consuma los últimos caracteres.
    SDL_Delay(100);

	// Enviar el evento SDL_QUIT para terminar el hilo Receptor de forma elegante (Finalizador)
	SDL_Event quit_event;
	quit_event.type = SDL_QUIT;
	SDL_PushEvent(&quit_event);

	// CORRECCIÓN 2: Esperar a que el hilo Receptor termine. 
    // Usamos un bucle para esperar a que el Receptor consuma el SDL_QUIT y termine.
    // Esto evita que memory_shutdown() se ejecute mientras el Receptor aún usa los semáforos.
    int attempts = 0;
    while(attempts++ < 100) {
        // Pollea el evento, si SDL_QUIT está en la cola, significa que el hilo Receptor 
        // probablemente aún no lo ha consumido.
        if (SDL_PollEvent(NULL) != SDL_QUIT) {
            // Si el evento no es SDL_QUIT (la cola está limpia), el Receptor terminó
            // o el evento se procesó. Salimos del bucle.
            break;
        }
        SDL_Delay(10); // Pausa mínima para no entrar en busy waiting aquí.
    }
    
    printf("Test finalizado: revisa '%s'\n", outpath);
    
	// Solo después de que el hilo Receptor haya tenido tiempo de terminar, cerramos los recursos.
	memory_shutdown();
	SDL_Quit();
	return 0;
}