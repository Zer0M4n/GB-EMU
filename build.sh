#!/bin/bash

# Detener el script si hay cualquier error
set -e

# 1. Configurar la ruta de EMSDK
EMSDK_PATH="/home/developer/emsdk"

# 2. Activar el entorno de Emscripten
if [ -f "$EMSDK_PATH/emsdk_env.sh" ]; then
    # Activamos el entorno silenciosamente
    source "$EMSDK_PATH/emsdk_env.sh" > /dev/null 2>&1
else
    echo "❌ Error: No se encontró emsdk_env.sh en $EMSDK_PATH"
    exit 1
fi

# 3. Limpieza y preparación de directorios
BUILD_DIR="build"

echo "--- 🧹 Limpiando build anterior ---"
# Esto es CRITICO para arreglar tu error de caché:
if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 4. Ejecutar la configuración (CMake)
echo "--- 🛠️  Configurando proyecto con emcmake ---"
# Pasamos la ruta del backup como variable, o dejamos que CMakeLists use la suya.
emcmake cmake .. || { echo "❌ Falló la configuración de CMake"; exit 1; }

# 5. Ejecutar la compilación (Make)
echo "--- ⚙️  Compilando a WebAssembly (emmake) ---"
emmake make -j$(nproc) || { echo "❌ Falló la compilación"; exit 1; }

echo "--- ✅ Build finalizado con éxito. ---"
echo "Para probarlo, ejecuta: python3 -m http.server en la carpeta build/"