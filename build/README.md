App simple en C usando SDL2 + SDL2_ttf

Requisitos
- Linux: installa `libsdl2-dev` y `libsdl2-ttf-dev` (ej: Debian/Ubuntu: `sudo apt install libsdl2-dev libsdl2-ttf-dev`) o usa `pkg-config`.
- Windows: se recomienda MSYS2 con `pacman -Syu` y luego `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf`
- Una fuente TrueType llamada `font.ttf` en la carpeta del proyecto (puedes copiar una desde tu sistema, por ejemplo `C:\Windows\Fonts\arial.ttf` en Windows).

Compilar (Linux)
```sh
make
```

Compilar (MSYS2/Windows)
```sh
# en entorno mingw64
make
```

Ejecutar
```sh
./sdl_app
```

Qué hace
- Muestra una ventana con dos botones: "Pagina 1" y "Pagina 2".
- Cada botón abre una página en blanco (ligeramente diferente) con un botón "Volver" que regresa al menú principal.

Notas
- Si `sdl2-config` no está disponible en Windows, modifica el `Makefile` para usar `pkg-config --cflags --libs sdl2` o el conjunto de librerías de MSYS2.
- Si quieres crear ejecutables nativos de Windows sin MSYS2, podemos proporcionar un proyecto MinGW o Visual Studio más adelante.
