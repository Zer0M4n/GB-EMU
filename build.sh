#!/bin/bash

# Detener el script si hay cualquier error
set -e

# --- FUNCIÓN DE LIMPIEZA ---
# Esta función busca qué proceso usa el puerto 8080 y lo mata.
limpiar_puerto() {
    echo ""
    echo "🧹 Cerrando procesos en el puerto 8080..."
    # fuser -k mata el proceso en ese puerto TCP. 
    # El '|| true' evita que el script falle si no había nada corriendo.
    fuser -k 8080/tcp > /dev/null 2>&1 || true
}

# 1. Configurar la ruta de EMSDK
EMSDK_PATH="/home/developer/emsdk"

# 2. Activar el entorno de Emscripten
if [ -f "$EMSDK_PATH/emsdk_env.sh" ]; then
    source "$EMSDK_PATH/emsdk_env.sh" > /dev/null 2>&1
else
    echo "❌ Error: No se encontró emsdk_env.sh en $EMSDK_PATH"
    exit 1
fi

# 3. Limpieza y preparación de directorios
BUILD_DIR="build" # (O el nombre que prefieras)

echo "--- 🧹 Limpiando build anterior ($BUILD_DIR) ---"
if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 4. Ejecutar la configuración (CMake)
echo "--- 🛠️  Configurando proyecto con emcmake ---"
emcmake cmake .. || { echo "❌ Falló la configuración de CMake"; exit 1; }

# 5. Ejecutar la compilación (Make)
echo "--- ⚙️  Compilando a WebAssembly (emmake) ---"
emmake make -j$(nproc) || { echo "❌ Falló la compilación"; exit 1; }

echo "--- ✅ Build finalizado con éxito. ---"

# ---------------------------------------------------------
# 6. SECCIÓN NUEVA: PREGUNTAR SI EJECUTAR
# ---------------------------------------------------------

echo ""
read -p "🚀 ¿Quieres ejecutar el servidor web ahora? (s/n): " respuesta

if [[ "${respuesta,,}" == "s" ]]; then
    
    HTML_FILE=$(ls *.html | head -n 1)

    if [ -z "$HTML_FILE" ]; then
        echo "⚠️  No encontré ningún archivo .html para ejecutar."
        exit 1
    fi

    echo "--- 🌐 Iniciando servidor emrun en puerto 8080 ---"
    echo "👉 Abre en tu navegador: http://localhost:8080/$HTML_FILE"
    echo "🔴 Presiona Ctrl+C para detener el servidor y limpiar el puerto."
# 1. Limpiamos ANTES de arrancar
    limpiar_puerto
    
    echo "⏳ Esperando liberación del puerto..."
    sleep 2  

    # 2. Configuramos el TRAP
    trap limpiar_puerto EXIT SIGINT

    # 3. Ejecutamos emrun
    emrun --no_browser --port 8080 "$HTML_FILE"

else
    echo "👋 ¡Listo! No se ejecutó el servidor."
    echo "   Cuando quieras correrlo, usa: emrun --no_browser --port 8080 $HTML_FILE"
fi