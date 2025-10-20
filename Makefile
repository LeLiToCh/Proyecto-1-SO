# Compilador de C
CC := gcc

# Banderas del compilador:
# -Wall: Activa todas las advertencias (warnings) comunes.
# -g: Incluye información de depuración (debugging) en los ejecutables.
CFLAGS := -Wall -g

# Banderas del enlazador (Linker):
# -lrt: Enlaza la biblioteca de tiempo real (para shm_open).
# -lpthread: Enlaza la biblioteca de POSIX threads (para sem_open).
LDFLAGS := -lrt -lpthread

# Directorio donde se guardarán los ejecutables compilados.
BUILD_DIR := build

# Lista de todos los programas ejecutables que queremos crear.
TARGETS := inicializador emisor receptor finalizador

# Genera una lista completa de las rutas de los ejecutables en el directorio de build.
EXECUTABLES := $(addprefix $(BUILD_DIR)/, $(TARGETS))

# --- Reglas del Makefile ---
# La primera regla es la que se ejecuta por defecto al llamar a 'make'.
.PHONY: all
all: $(EXECUTABLES)
	@echo "\nTodos los programas han sido compilados exitosamente."
	@echo "Los ejecutables se encuentran en el directorio: $(BUILD_DIR)/"
	@echo "Para limpiar el proyecto, ejecute: make clean"

# Regla de Patrón Genérica:
$(BUILD_DIR)/%: %.c memInfo.h
	@echo "Compilando $< -> $@"
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# Regla 'clean':
.PHONY: clean
clean:
	@echo "🧹 Limpiando el directorio de compilación..."
	@rm -rf $(BUILD_DIR)
	@echo "Limpieza completada."