# **GB-EMU 🎮**

Un emulador de Game Boy (DMG) escrito desde cero en C++17.  
Diseñado con una arquitectura modular para funcionar nativamente en Desktop (Linux/macOS) y en la Web (WebAssembly).

## **📂 Estructura del Proyecto**

* core/: Lógica del emulador (CPU, MMU, Cartridge, Mappers).  
* roms/: Carpeta contenedora de los juegos .gb.  
* emc\_main.cpp: Punto de entrada híbrido (compatible con Web y Desktop).  
* CMakeLists.txt: Sistema de construcción automatizado.

## **🚀 Preparación (Importante)**

Antes de compilar, asegúrate de crear la carpeta de roms y añadir un juego (ej. tetris.gb):

mkdir \-p roms  
\# Copia tu archivo .gb dentro de la carpeta roms/ y asegúrate de que se llame tetris.gb para la prueba por defecto

## **💻 Opción A: Compilar para Desktop (Linux)**

Usa esta opción para desarrollo y depuración en tu máquina local.

### **Requisitos**

* CMake  
* Compilador C++ (g++ o clang)  
* Make

### **Instrucciones**

\# 1\. Crear carpeta de build  
mkdir \-p build && cd build

\# 2\. Configurar y Compilar  
cmake ..  
make \-j16

\# 3\. Ejecutar  
\# Uso: ./gb-emu \<ruta\_al\_rom\>  
\# Si no pasas argumentos, buscará roms/tetris.gb por defecto  
./gb-emu ../roms/tetris.gb

## **🌐 Opción B: Compilar para Web (WASM)**

Usa esta opción para ejecutar el emulador en un navegador web.

### **Requisitos**

* Emscripten SDK (emsdk) activado en tu terminal.

### **Instrucciones**

\# 1\. Crear carpeta de build para web (limpia)  
rm \-rf build\_web  
mkdir \-p build\_web && cd build\_web

\# 2\. Configurar con Emscripten  
\# (El CMakeLists.txt detectará automáticamente el entorno EMSCRIPTEN)  
emcmake cmake ..

\# 3\. Compilar  
emmake make

\# 4\. Probar en servidor local  
python3 \-m http.server

Nota: Abre tu navegador en http://localhost:8000/gb-emu.html.  
La carpeta roms/ se precarga automáticamente en el sistema de archivos virtual del navegador.

## **🧩 Estado del Proyecto**

* \[x\] Carga de ROMs (.gb)  
* \[x\] Soporte MBC1 (Manejo de bancos de memoria)  
* \[x\] Ciclo de CPU Básico (Fetch/Decode/Execute)  
* \[x\] Compatibilidad WebAssembly (WASM)  
* \[ \] Implementación completa de instrucciones (Opcodes)  
* \[ \] PPU (Unidad de procesamiento de píxeles)  
* \[ \] Manejo de inputs y timers

## **🐞 Debugging**

Actualmente, el main tiene un limitador de seguridad de 50 pasos para evitar congelar el navegador durante el desarrollo inicial.  
Para quitarlo o aumentarlo, edita \`emc\_