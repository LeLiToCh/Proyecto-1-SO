# Makefile para sdl_app
# Proyecto-1-SO - Aplicación SDL2 multiplataforma

# --- CONFIGURACIÓN DEL COMPILADOR ---
CC = gcc
CSTD = -std=c99
CFLAGS_BASE = -Wall -Wextra -pedantic
CFLAGS_DEBUG = $(CFLAGS_BASE) -g -O0 -DDEBUG
CFLAGS_RELEASE = $(CFLAGS_BASE) -O2 -DNDEBUG

# --- CONFIGURACIÓN DE DIRECTORIOS ---
SRC_DIR = src
PAGES_DIR = $(SRC_DIR)/pages
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

# --- CONFIGURACIÓN ESPECÍFICA DEL SISTEMA OPERATIVO ---
ifeq ($(OS),Windows_NT)
    # --- CONFIGURACIÓN PARA WINDOWS (MSYS2 / MinGW) ---
    DETECTED_OS := Windows
    EXE_EXT = .exe
    RM = rm -f
    MKDIR = mkdir -p
    
    # Intentar detectar MSYS2 automáticamente
    ifdef MSYSTEM
        MSYS2_PREFIX = /$(shell echo $(MSYSTEM) | tr '[:upper:]' '[:lower:]')
        INCLUDES = -I$(MSYS2_PREFIX)/include/SDL2
        LIBS = -L$(MSYS2_PREFIX)/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lcomdlg32
    else
        # Fallback para instalación estándar de MSYS2
        INCLUDES = -IC:/msys64/mingw64/include/SDL2
        LIBS = -LC:/msys64/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lcomdlg32
    endif
else
    # --- CONFIGURACIÓN PARA LINUX / UNIX ---
    DETECTED_OS := $(shell uname -s)
    EXE_EXT =
    RM = rm -f
    MKDIR = mkdir -p
    
    # Usar pkg-config para obtener flags de SDL2
    INCLUDES = $(shell pkg-config --cflags sdl2 2>/dev/null || sdl2-config --cflags 2>/dev/null)
    LIBS = $(shell pkg-config --libs sdl2 SDL2_ttf 2>/dev/null || echo "$(shell sdl2-config --libs 2>/dev/null) -lSDL2_ttf")
endif

# --- ARCHIVOS FUENTE Y OBJETOS ---
SRCS = $(SRC_DIR)/main.c \
       $(PAGES_DIR)/page_main.c \
       $(PAGES_DIR)/page_one.c \
       $(PAGES_DIR)/page_two.c

HEADERS = $(PAGES_DIR)/page_main.h \
          $(PAGES_DIR)/page_one.h \
          $(PAGES_DIR)/page_two.h

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

# --- NOMBRE DEL EJECUTABLE ---
TARGET = sdl_app$(EXE_EXT)
TARGET_PATH = $(BIN_DIR)/$(TARGET)

# --- CONFIGURACIÓN POR DEFECTO ---
BUILD_TYPE ?= release
ifeq ($(BUILD_TYPE),debug)
    CFLAGS = $(CFLAGS_DEBUG)
else
    CFLAGS = $(CFLAGS_RELEASE)
endif

# Agregar includes y src directory
CFLAGS += $(INCLUDES) -I$(SRC_DIR)

# --- OBJETIVOS ---
.PHONY: all clean debug release install help info

# Objetivo por defecto
all: $(TARGET_PATH)

# Compilación en modo debug
debug:
	@$(MAKE) BUILD_TYPE=debug $(TARGET_PATH)

# Compilación en modo release
release:
	@$(MAKE) BUILD_TYPE=release $(TARGET_PATH)

# Enlazar el ejecutable
$(TARGET_PATH): $(OBJS) | $(BIN_DIR)
	@echo "Enlazando $(TARGET)..."
	$(CC) $(OBJS) -o $@ $(LIBS)
	@echo "✓ Compilación exitosa: $@"

# Compilar archivos objeto con generación de dependencias
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR) $(OBJ_DIR)/pages
	@echo "Compilando $<..."
	$(CC) $(CSTD) $(CFLAGS) -MMD -MP -c $< -o $@

# Incluir dependencias generadas automáticamente
-include $(DEPS)

# Crear directorios necesarios
$(OBJ_DIR):
	$(MKDIR) $(OBJ_DIR)

$(OBJ_DIR)/pages:
	$(MKDIR) $(OBJ_DIR)/pages

$(BIN_DIR):
	$(MKDIR) $(BIN_DIR)

# Limpiar archivos generados
clean:
	@echo "Limpiando archivos generados..."
	$(RM) -r $(BUILD_DIR)/obj $(BUILD_DIR)/bin
	@echo "✓ Limpieza completada"

# Limpiar todo incluyendo directorio build
distclean: clean
	@echo "Limpieza completa..."
	$(RM) -r $(BUILD_DIR)
	@echo "✓ Limpieza completa terminada"

# Instalar (copiar ejecutable al directorio actual para facilidad de uso)
install: $(TARGET_PATH)
	@echo "Instalando $(TARGET)..."
	cp $(TARGET_PATH) ./$(TARGET)
	@echo "✓ $(TARGET) instalado en el directorio actual"

# Mostrar información del sistema y configuración
info:
	@echo "=== INFORMACIÓN DE CONFIGURACIÓN ==="
	@echo "Sistema detectado: $(DETECTED_OS)"
	@echo "Compilador: $(CC)"
	@echo "Modo de compilación: $(BUILD_TYPE)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "INCLUDES: $(INCLUDES)"
	@echo "LIBS: $(LIBS)"
	@echo "Ejecutable: $(TARGET_PATH)"
	@echo "Archivos fuente:"
	@$(foreach src,$(SRCS),echo "  - $(src)";)

# Mostrar ayuda
help:
	@echo "=== MAKEFILE PARA SDL_APP ==="
	@echo "Objetivos disponibles:"
	@echo "  all        - Compilar el proyecto (modo release por defecto)"
	@echo "  debug      - Compilar en modo debug (-g -O0)"
	@echo "  release    - Compilar en modo release (-O2)"
	@echo "  clean      - Limpiar archivos objeto y ejecutables"
	@echo "  distclean  - Limpiar todo incluyendo directorio build"
	@echo "  install    - Copiar ejecutable al directorio actual"
	@echo "  info       - Mostrar información de configuración"
	@echo "  help       - Mostrar esta ayuda"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_TYPE=[debug|release] - Tipo de compilación (default: release)"
	@echo ""
	@echo "Ejemplos:"
	@echo "  make              # Compilar en modo release"
	@echo "  make debug        # Compilar en modo debug"
	@echo "  make clean        # Limpiar archivos generados"
	@echo "  make info         # Ver configuración actual"
