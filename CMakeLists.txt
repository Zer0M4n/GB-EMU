cmake_minimum_required(VERSION 3.10)
project(GB-EMU)

# Forzamos C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Seguridad: Si intentas compilar esto con g++ normal, dará error y te avisará
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Este proyecto ahora es SOLO WEB. Usa 'emcmake cmake ..'")
endif()

# ==============================================================================
# 1. RUTAS DE CABECERAS (.h)
# ==============================================================================
include_directories(
    ${CMAKE_SOURCE_DIR}
    core
    core/cpu
    core/cpu/ppu
    core/cpu/mmu
    core/cpu/timer
    core/cpu/APU
    core/cartridge
    core/cartridge/IMBC
)

# ==============================================================================
# 2. ARCHIVOS FUENTE
# ==============================================================================
set(CORE_SOURCES
    core/cpu/cpu.cpp
    core/cpu/mmu/mmu.cpp
    core/cpu/ppu/ppu.cpp
    core/cpu/timer/timer.cpp
    core/cpu/APU/apu.cpp
    core/cartridge/cartridge.cpp
    core/cartridge/IMBC/type_cartridge/RomOnly.cpp
    core/cartridge/IMBC/type_cartridge/MBC1.cpp
)

# ==============================================================================
# 3. CONFIGURACIÓN DEL EJECUTABLE
# ==============================================================================
# Cambia esto:
add_executable(gb-emu emc_main.cpp ${CORE_SOURCES})

# Por esto:
add_executable(index emc_main.cpp ${CORE_SOURCES})

# Y asegúrate de que el sufijo siga siendo .html
set_target_properties(index PROPERTIES SUFFIX ".html")

# ==============================================================================
# 4. FLAGS DE EMSCRIPTEN (LINKER)
# ==============================================================================
target_link_options(gb-emu PRIVATE
    "SHELL:-s USE_SDL=0"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s NO_EXIT_RUNTIME=1"
    
    # IMPORTANTE: Habilita el sistema de archivos virtual para cargar ROMs
    "SHELL:-s FORCE_FILESYSTEM=1"

    # Funciones de C++ que JS puede llamar
    # Nota: _load_rom_from_js es la nueva adición crítica
    "SHELL:-s EXPORTED_FUNCTIONS=['_main','_load_rom_from_js','_get_video_buffer','_get_video_buffer_size','_set_button','_get_audio_buffer','_get_audio_samples_available','_fill_audio_buffer','_set_audio_muted']"

    # Métodos del runtime de Emscripten que JS puede usar
    # Nota: 'FS' es necesario para escribir archivos desde el navegador
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','HEAPU8','HEAPF32']" 
    
    # Plantilla HTML personalizada
    "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/src/index.html"

    # OPCIONAL: Precarga de archivos (COMENTADO para permitir carga dinámica pura)
    # Descomenta esto solo si quieres una ROM por defecto y la ruta existe en tu PC
    # "SHELL:--preload-file /home/developer/projects/GB-EMU/roms@/roms"
)
