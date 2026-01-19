cmake_minimum_required(VERSION 3.10)

# Nombre del proyecto
project(GB-EMU)

# Usar C++17 (Recomendado para emuladores modernos)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. DIRECTORIOS DE CABECERA (.h)
# Esto permite que tus #include funcionen sin poner rutas kilométricas
include_directories(
    ${CMAKE_SOURCE_DIR}
    core
    core/cpu
    core/cpu/mmu
    core/cartridge
    core/cartridge/IMBC
)

# 2. DEFINICIÓN DE ARCHIVOS FUENTE (.cpp)
# Aquí listamos TODOS los archivos .cpp que se ven en tu imagen
set(SOURCES
    main.cpp
    
    # CPU y MMU
    core/cpu/cpu.cpp
    core/cpu/mmu/mmu.cpp
    
    # Cartridge y MBCs
    core/cartridge/cartridge.cpp
    core/cartridge/IMBC/type_cartridge/MBC1.cpp
    core/cartridge/IMBC/type_cartridge/RomOnly.cpp
)

# 3. CREAR EL EJECUTABLE
add_executable(gb-emu ${SOURCES})