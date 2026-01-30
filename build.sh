#!/bin/bash

# 1. Configurar la ruta de EMSDK basada en tu 'pwd'
EMSDK_PATH="/home/developer/emsdk"

# 2. Activar el entorno de Emscripten
if [ -f "$EMSDK_PATH/emsdk_env.sh" ]; then
    # El '>' redirige la salida para que no ensucie tu terminal con mensajes de carga
    source "$EMSDK_PATH/emsdk_env.sh" > /dev/null 2>&1
else
    echo "Error: No se encontró emsdk_env.sh en $EMSDK_PATH"
    exit 1
fi

# 3. Crear y entrar a la carpeta de build
BUILD_DIR="build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 4. Ejecutar la compilación
echo "--- 🛠️  Configurando proyecto con emcmake ---"
emcmake cmake ..

echo "--- ⚙️  Compilando a WebAssembly (emmake) ---"
emmake make -j$(nproc)

echo "--- Build finalizado con éxito en /$BUILD_DIR ---"