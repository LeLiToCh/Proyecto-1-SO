#include "finalizador.h" // Incluye la interfaz del Finalizador
#include "receptor.h"
#include "processor.h" // Se incluye aunque el hilo principal actúe como Processor
#include "../src/memory.h" // Ajusta la ruta a memory.h si es necesario
#include <stdio.h>
#include <SDL.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	(void)argc; (void)argv;

    size_t memory_cap = 64;
    size_t total_chars_written = 0;
    const char *outpath = "test_output.txt";
    const char *key_bits = "00000000"; 
    // Mensaje con una longitud superior al buffer para probar la sincronización de escritura/lectura.
    const char *msg = "Esta es la primera parte del mensaje para el Finalizador. Debe asegurar que todos los caracteres sean transferidos y que el sistema se cierre elegantemente al final de la ejecucion.";
    
    printf("[TEST] Inicializando sistema de comunicacion.\n");

	// 1. Inicialización de SDL (para eventos y threads)
	if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
		fprintf(stderr, "[TEST] SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	// 2. Inicialización de la Memoria Compartida (In-Process en este caso)
	if (!memory_init(memory_cap)) {
		fprintf(stderr, "[TEST] memory_init failed\n");
		SDL_Quit();
		return 1;
	}

    // 3. INICIO DEL RECEPTOR (Hilo Consumidor Asíncrono)
	if (!receptor_start_async(key_bits, true)) {
		fprintf(stderr, "[TEST] receptor_start_async failed\n");
		memory_shutdown();
		SDL_Quit();
		return 1;
	}

    // Pausa necesaria para asegurar que el Hilo Receptor esté BLOCKED en memory_read_entry (Sincronización inicial)
	SDL_Delay(500); 
    
    // 4. ESCRITURA DE DATOS (Hilo Principal actuando como Emisor/Processor)
    printf("[TEST] Escribiendo %zu caracteres en la memoria compartida.\n", strlen(msg));
	
    total_chars_written = strlen(msg);
	for (size_t i = 0; i < total_chars_written; ++i) {
        // Se escribe con clave 0x00 (sin cifrado)
		if (!memory_write_entry_with_key((uint8_t)msg[i], 0x00, NULL, NULL)) {
			fprintf(stderr, "[TEST] memory_write failed at char %zu (%c). Terminando escritura prematuramente.\n", i, msg[i]);
            // Si falla la escritura (ej. error de IPC), registramos los escritos hasta ahora.
            total_chars_written = i; 
			break;
		}
	}
    
    // Pausa final para que el Receptor consuma la mayoría de los caracteres,
    // pero dejando algunos para que el Finalizador los registre como "pendientes".
    SDL_Delay(200);
    
    // 5. LLAMADA AL FINALIZADOR (Orquestación del Apagado y Estadísticas)
    printf("[TEST] Escritura finalizada. Llamando al Finalizador para el apagado general.\n");
	
    // El Finalizador recibe la cantidad de caracteres que el Emisor (hilo principal)
    // intentó escribir para el cálculo de estadísticas.
    finalizador_shutdown_system(total_chars_written);

	printf("[TEST] Programa de prueba terminado.\n");
	return 0;
}