// Test program for receptor: initialize memory, start receptor thread,
// write encoded chars into memory and then post SDL_QUIT to stop.

#include "receptor.h"
#include "../memory.h"
#include <stdio.h>
#include <SDL.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	(void)argc; (void)argv;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	if (!memory_init(64)) {
		fprintf(stderr, "memory_init failed\n");
		SDL_Quit();
		return 1;
	}

	const char *outpath = "test_output.txt";
	const char *key_bits = "00000000"; // clave XOR = 0 (sin cambio)

	if (!receptor_start_async(outpath, key_bits, true)) {
		fprintf(stderr, "receptor_start_async failed\n");
		memory_shutdown();
		SDL_Quit();
		return 1;
	}

	SDL_Delay(100);

	const char *msg = "Hola desde test\n";
	for (size_t i = 0; i < strlen(msg); ++i) {
		if (!memory_write_entry_with_key((uint8_t)msg[i], 0x00, NULL, NULL)) {
			fprintf(stderr, "memory_write_entry failed at pos %zu\n", i);
			break;
		}
		SDL_Delay(50);
	}

	SDL_Delay(500);

	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_QUIT;
	SDL_PushEvent(&ev);

	SDL_Delay(200);

	memory_shutdown();
	SDL_Quit();

	printf("Test finalizado: revisa '%s'\n", outpath);
	return 0;
}
