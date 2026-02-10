# GB-EMU (WebAssembly Edition) 🎮

A **Game Boy (DMG)** emulator written from scratch in **C++17**, specifically compiled for the Web using **Emscripten (WebAssembly - WASM)**.

> ⚠️ **WORK IN PROGRESS**: This project is under active development. It is incomplete and intended for **educational and testing purposes only**.

---

## 📂 Project Structure

```text
GB-EMU/
├── core/               # Emulator logic (CPU, MMU, Cartridge, Mappers)
├── emc_main.cpp        # Main entry point for the Web version
├── CMakeLists.txt      # Build configuration
├── build.sh            # Automated build helper script
└── roms/               # (Not included) User-provided ROM files
```

> 📌 **Note:** This repository does **NOT** include ROM files. You must provide your own.

---

## ⚙️ Configuration (CRITICAL STEP)

Because this project runs inside a browser sandbox, ROMs must be **manually injected into Emscripten’s virtual file system at build time**.

---

### 🧱 Step 1: Prepare your ROM

1. Create a folder named `roms` in the project root (if it doesn’t exist).
2. Place your Game Boy ROM file inside it.

Example:

```text
roms/
└── your_game.gb
```

---

### 🗺️ Step 2: Modify `CMakeLists.txt` (Virtual Path Mapping)

You must tell Emscripten:

* Where the ROMs are located on your real machine
* Where they should appear inside the browser

Open `CMakeLists.txt` and locate the `target_link_options` section. Modify the preload line:

```cmake
"SHELL:--preload-file /YOUR/REAL/PATH/TO/roms@/roms"
```

#### Explanation:

* **Left side (before `@`)** → Absolute path on your computer
  Example:

  ```text
  /home/user/GB-EMU/roms
  ```

* **Right side (after `@`)** → Virtual path inside the browser
  👉 **Keep this as** `/roms`

---

### 🎮 Step 3: Modify `emc_main.cpp` (ROM Loading)

Update the ROM path so it matches the filename inside the virtual file system:

```cpp
// Change "your_game.gb" to the actual ROM filename
std::string romPath = "roms/your_game.gb";
```

---

## 🚀 Build Instructions

### 🔧 Requirements

* **Emscripten SDK (emsdk)** installed and activated in your terminal.

---

### ✅ Option A: Automated Build (Recommended)

The project includes a helper script that:

* Cleans previous builds
* Configures CMake with Emscripten
* Compiles the project
* Optionally launches a local web server

```bash
# 1. Give execution permissions (only once)
chmod +x build.sh

# 2. Run the script
./build.sh
```

At the end of the process, the script will ask whether you want to start the web server automatically.

---

### 🛠️ Option B: Manual Compilation

If you prefer running each step manually:

```bash
# 1. Create a clean build directory
rm -rf build_web
mkdir -p build_web && cd build_web

# 2. Configure with Emscripten
emcmake cmake ..

# 3. Compile
emmake make -j$(nproc)
```

---

## ▶️ Running the Emulator

⚠️ **Do NOT open the generated HTML file directly**. You must use a local web server due to WASM/CORS security restrictions.

---

### 🔹 Via Script

If you used `./build.sh`, simply type:

```text
s + Enter
```

when prompted.

---

### 🔹 Manual

#### Using `emrun` (Recommended)

```bash
emrun --no_browser --port 8888 gb-emu.html
```

#### Using Python

```bash
python3 -m http.server 8888
```

---

### 🌐 Access

Open your browser at:

```text
http://localhost:8888/gb-emu.html
```

---

## 🧩 Project Status

* [x] ROM loading via Virtual File System
* [x] Basic emulator architecture
* [x] WebAssembly compilation pipeline
* [ ] Complete CPU instruction set
* [ ] PPU (Graphics & Rendering) — *In progress*
* [ ] Timers & Interrupts
* [ ] Joypad input handling

---

## 📚 Project Goals

This project aims to:

* Learn **Game Boy (DMG) architecture**
* Explore **low-level emulation concepts**
* Experiment with **C++ + WebAssembly**
* Build a full emulator **from scratch**, without external emulation libraries
---
## Example
 


https://github.com/user-attachments/assets/88e74923-9da6-4370-b4ec-90aa76692b20


💡 *Issues, pull requests, and feedback are welcome.*
---

## ⚖️ Legal Disclaimer

This project is an **independent, unofficial emulator** developed for **educational purposes only**.

- This repository **does NOT include** any copyrighted ROMs, BIOS files, or proprietary assets.
- Users must provide their **own legally obtained Game Boy ROMs**.
- Any ROM filenames shown in examples (e.g. `your_game.gb`) are **placeholders only**.
- This project is **not affiliated with, endorsed by, or associated with Nintendo**.

All trademarks and registered trademarks are the property of their respective owners.
