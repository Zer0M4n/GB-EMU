cmake_minimum_required(VERSION 3.10)

# Nombre del proyecto
project(GB-EMU)

# Agrega los archivos fuente del núcleo de la CPU
set(CPU_SOURCES
    core/cpu/cpu.cpp
    core/cpu/mmu/mmu.cpp
    core/cpu/instructions/intructions.cpp
)

# Crear el ejecutable (Nombre del ejecutable + archivos fuente)
add_executable(app main.cpp ${CPU_SOURCES})

# Incluye los directorios necesarios
target_include_directories(app PRIVATE core/cpu core/cpu/mmu core/cpu/instructions)