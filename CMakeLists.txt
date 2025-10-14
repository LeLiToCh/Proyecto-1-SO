cmake_minimum_required(VERSION 3.10)
project(sdl_app C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Prefer pkg-config when available (good for MSYS2 / Linux)
find_package(PkgConfig QUIET)

set(SDL2_INCLUDE_DIRS "")
set(SDL2_LIBRARIES "")
set(SDL2_TTF_INCLUDE_DIRS "")
set(SDL2_TTF_LIBRARIES "")

if(PkgConfig_FOUND)
  message(STATUS "PkgConfig found; attempting to locate sdl2 and SDL2_ttf via pkg-config")
  pkg_check_modules(SDL2_PKG QUIET sdl2)
  pkg_check_modules(SDL2_TTF_PKG QUIET SDL2_ttf)
  if(SDL2_PKG_FOUND)
    set(SDL2_INCLUDE_DIRS ${SDL2_PKG_INCLUDE_DIRS})
    set(SDL2_LIBRARIES ${SDL2_PKG_LIBRARIES})
    message(STATUS "Found SDL2 via pkg-config: ${SDL2_PKG_VERSION}")
  endif()
  if(SDL2_TTF_PKG_FOUND)
    set(SDL2_TTF_INCLUDE_DIRS ${SDL2_TTF_PKG_INCLUDE_DIRS})
    set(SDL2_TTF_LIBRARIES ${SDL2_TTF_PKG_LIBRARIES})
    message(STATUS "Found SDL2_ttf via pkg-config: ${SDL2_TTF_PKG_VERSION}")
  endif()
endif()

# If pkg-config didn't find SDL2/TTF, try CMake packages
if(NOT SDL2_LIBRARIES)
  find_package(SDL2 CONFIG QUIET)
  if(SDL2_FOUND)
    set(SDL2_LIBRARIES SDL2::SDL2)
    get_target_property(_sdl_inc SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
    if(_sdl_inc)
      set(SDL2_INCLUDE_DIRS ${_sdl_inc})
    endif()
    message(STATUS "Found SDL2 via CMake config package")
  else()
    # Fallback to FindSDL2 module if available
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
      # Some FindSDL2 modules expose SDL2_INCLUDE_DIRS/LIBRARIES variables
      message(STATUS "Found SDL2 via FindSDL2 module")
    endif()
  endif()
endif()

if(NOT SDL2_TTF_LIBRARIES)
  find_package(SDL2_ttf CONFIG QUIET)
  if(SDL2_ttf_FOUND)
    set(SDL2_TTF_LIBRARIES SDL2_ttf::SDL2_ttf)
    get_target_property(_ttf_inc SDL2_ttf::SDL2_ttf INTERFACE_INCLUDE_DIRECTORIES)
    if(_ttf_inc)
      set(SDL2_TTF_INCLUDE_DIRS ${_ttf_inc})
    endif()
    message(STATUS "Found SDL2_ttf via CMake config package")
  else()
    # Fallback: try plain find
    find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h)
    find_library(SDL2_TTF_LIBRARY NAMES SDL2_ttf SDL2_ttf-2.0 SDL2_ttf-2.0.0)
    if(SDL2_TTF_INCLUDE_DIR AND SDL2_TTF_LIBRARY)
      set(SDL2_TTF_INCLUDE_DIRS ${SDL2_TTF_INCLUDE_DIR})
      set(SDL2_TTF_LIBRARIES ${SDL2_TTF_LIBRARY})
      message(STATUS "Found SDL2_ttf by manual search")
    endif()
  endif()
endif()

if(NOT SDL2_LIBRARIES OR NOT SDL2_TTF_LIBRARIES)
  message(STATUS "SDL2_LIBRARIES = ${SDL2_LIBRARIES}")
  message(STATUS "SDL2_TTF_LIBRARIES = ${SDL2_TTF_LIBRARIES}")
  message(FATAL_ERROR "Could not find SDL2 and/or SDL2_ttf. Install via MSYS2 (mingw) or your distro packages. Alternatively set SDL2_INCLUDE_DIRS, SDL2_LIBRARIES, SDL2_TTF_INCLUDE_DIRS, SDL2_TTF_LIBRARIES.")
endif()

# Sources
set(SOURCE_FILES
  src/main.c
  src/memory.c
  src/processor.c
  src/pages/page_main.c
  src/pages/page_one.c
  src/pages/page_two.c
  src/pages/page_sender.c
  src/app_state.c
)

add_executable(sdl_app ${SOURCE_FILES})

target_include_directories(sdl_app PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(sdl_app PRIVATE ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES})

# Windows: link common dialogs for GetOpenFileNameW
if(WIN32)
  target_link_libraries(sdl_app PRIVATE comdlg32)
endif()

# Copy README to build dir as a reminder
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/README.md ${CMAKE_CURRENT_BINARY_DIR}/README.md COPYONLY)
endif()

# After build, copy font.ttf next to the executable (if available)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/font.ttf)
  add_custom_command(TARGET sdl_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/font.ttf
            $<TARGET_FILE_DIR:sdl_app>/font.ttf
  )
endif()

message(STATUS "CMake configured. Ensure 'font.ttf' is in the working directory at runtime.")
