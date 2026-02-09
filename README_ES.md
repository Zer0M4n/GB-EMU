# GB-EMU (Edición WebAssembly) 🎮

Un emulador de **Game Boy (DMG)** escrito desde cero en **C++17**, compilado específicamente para la Web usando **Emscripten (WebAssembly - WASM)**.

> ⚠️ **EN DESARROLLO**: Este proyecto está en fase de construcción activa. Está incompleto y su propósito es **educativo y experimental**.

---

## 📂 Estructura del Proyecto

```text
GB-EMU/
├── core/               # Lógica principal del emulador (CPU, MMU, Cartridge, Mappers)
├── emc_main.cpp        # Punto de entrada para la versión Web (Emscripten)
├── CMakeLists.txt      # Configuración de compilación
├── build.sh            # Script de automatización de compilación
└── roms/               # (Ignorado por git) ROMs del usuario
```

> 📌 **Nota:** Este repositorio **NO incluye ROMs**. Debes agregar las tuyas propias.

---

## ⚙️ Configuración de ROMs (PASO CRÍTICO)

Debido a que el emulador se ejecuta dentro del navegador, las ROMs deben ser **inyectadas en el sistema de archivos virtual de Emscripten durante la compilación**.

### 🧱 Paso 1: Preparar tu ROM

1. Crea una carpeta llamada `roms` en la raíz del proyecto (si no existe).
2. Copia tu archivo de juego dentro de ella.

Ejemplo:

```text
roms/
└── mi_juego.gb
```

---

### 🗺️ Paso 2: Modificar `CMakeLists.txt` (Ruta Virtual)

Debes indicarle a Emscripten:

* Dónde están las ROMs en tu sistema real
* En qué ruta existirán dentro del navegador

Busca la sección `target_link_options` y modifica la línea de *preload*:

```cmake
"SHELL:--preload-file /TU/RUTA/REAL/HACIA/roms@/roms"
```

#### Explicación:

* **Antes del `@`** → Ruta absoluta en tu PC
  Ejemplo:

  ```text
  /home/usuario/GB-EMU/roms
  ```

* **Después del `@`** → Ruta virtual dentro del navegador
  👉 **Déjala como** `/roms`

---

### 🎮 Paso 3: Modificar `emc_main.cpp` (Carga del Juego)

Actualiza la ruta del archivo ROM para que coincida con el nombre real de tu juego:

```cpp
// Cambia "mi_juego.gb" por el nombre real de tu ROM
std::string romPath = "roms/mi_juego.gb";
```

---

## 🚀 Instrucciones de Compilación

### 🔧 Requisitos

* **Emscripten SDK (emsdk)** correctamente instalado y activado en tu terminal.

---

### ✅ Opción A: Compilación Automática (Recomendada)

El proyecto incluye un script que:

* Limpia el build
* Configura CMake con Emscripten
* Compila el proyecto
* (Opcionalmente) inicia un servidor web

```bash
# 1. Dar permisos de ejecución (solo una vez)
chmod +x build.sh

# 2. Ejecutar el script
./build.sh
```

Al finalizar, el script te preguntará si deseas iniciar el servidor web automáticamente.

---

### 🛠️ Opción B: Compilación Manual

Si prefieres ejecutar los pasos manualmente:

```bash
# 1. Crear un directorio de build limpio
rm -rf build_web
mkdir -p build_web && cd build_web

# 2. Configurar con Emscripten
emcmake cmake ..

# 3. Compilar
emmake make -j$(nproc)
```

---

## ▶️ Ejecutar el Emulador

⚠️ **No abras el archivo HTML con doble clic**. Debes usar un servidor local debido a las políticas de seguridad de WASM/CORS.

### 🔹 Vía Script

Si usaste `./build.sh`, simplemente escribe:

```text
s + Enter
```

cuando el script lo solicite.

---

### 🔹 Manual

#### Usando `emrun` (Recomendado)

```bash
emrun --no_browser --port 8888 gb-emu.html
```

#### Usando Python

```bash
python3 -m http.server 8888
```

---

### 🌐 Acceso

Abre tu navegador en:

```
http://localhost:8888/gb-emu.html
```

---

## 🧩 Estado del Proyecto

* [x] Carga de ROMs (Sistema de Archivos Virtual)
* [x] Arquitectura base del emulador
* [x] Pipeline de compilación WebAssembly
* [ ] Set completo de instrucciones de CPU
* [ ] PPU (Gráficos y Renderizado) — *En progreso*
* [ ] Timers e Interrupciones
* [ ] Entrada de controles (Joypad)

---

## 📚 Objetivo del Proyecto

Este emulador tiene como finalidad:

* Aprender arquitectura del **Game Boy (DMG)**
* Profundizar en **emulación a bajo nivel**
* Experimentar con **C++ + WebAssembly**
* Construir un emulador **desde cero**, sin librerías externas

---


💡 *Pull Requests, ideas y feedback son bienvenidos.*

## ⚖️ Aviso Legal

Este proyecto es un **emulador independiente y no oficial**, desarrollado **únicamente con fines educativos**.

- Este repositorio **NO incluye** ROMs, archivos BIOS ni otros recursos protegidos por derechos de autor.
- Los usuarios deben proporcionar sus **propias ROMs de Game Boy obtenidas legalmente**.
- Cualquier nombre de archivo de ROM mostrado en los ejemplos (por ejemplo, `your_game.gb`) es **solo un marcador de posición**.
- Este proyecto **no está afiliado, patrocinado ni asociado con Nintendo**.

Todas las marcas comerciales y marcas registradas son propiedad de sus respectivos dueños.
