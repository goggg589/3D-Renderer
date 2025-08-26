# C++ 3D Wireframe Renderer (OBJ)

Tiny C++17 project that loads a Wavefront **.obj** and renders it as a **wireframe** using pure CPU transforms (no OpenGL/Vulkan).

**Targets**
- `render-cli` — headless renderer that writes a PNG.
- `render-qt`  — interactive Qt viewer with orbit/pan/zoom and FPS HUD *(optional; only if Qt6 is installed and enabled)*.
- `render-gui` — optional SFML viewer *(only if `src/apps/render_gui.cpp` exists)*.

---

## Requirements
- C++17 compiler (GCC ≥ 9 / Clang ≥ 10 / MSVC ≥ 2019)
- CMake ≥ 3.15
- SFML 2.6 dev headers  
  - Linux: `libsfml-dev`  
  - macOS (Homebrew): `sfml`  
  - Windows (MSYS2): `mingw-w64-x86_64-sfml`
- **Qt6 Widgets** *(only for `render-qt`)*  
  - Linux: `qt6-base-dev`  
  - macOS (Homebrew): `qt`  
  - Windows (MSYS2): `mingw-w64-x86_64-qt6-base`

---

## Build & Run

> Always run commands from the **project root** (folder with `CMakeLists.txt`, `src/`, `assets/`).

### Linux / WSL (Ubuntu/Debian)

```bash
# install deps
sudo apt update
sudo apt install -y g++ cmake libsfml-dev            # required
sudo apt install -y qt6-base-dev                     # optional (Qt viewer)

# configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_QT=ON

# build everything (or specific targets)
cmake --build build -j
# or: cmake --build build -j --target render-cli
# or: cmake --build build -j --target render-qt
```

**Run**
```bash
# CLI (writes a PNG)
./build/render-cli assets/cube.obj out.png --size 1200 900 --fov 60

# Qt viewer (interactive)
export QT_QPA_PLATFORM=xcb      # WSL tip: avoids Wayland plugin error
./build/render-qt assets/cube.obj
```

### macOS (Homebrew)

```bash
brew install cmake sfml qt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_QT=ON
cmake --build build -j

# run
./build/render-cli assets/cube.obj out.png --size 1200 900 --fov 60
./build/render-qt  assets/cube.obj
```

### Windows (MSYS2 MinGW64 shell)

```bash
pacman -S --needed mingw-w64-x86_64-toolchain                  mingw-w64-x86_64-cmake                  mingw-w64-x86_64-sfml                  mingw-w64-x86_64-qt6-base

cmake -G "MinGW Makefiles" -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_QT=ON
cmake --build build -j

# run
./build/render-cli.exe assets/cube.obj out.png --size 1200 900 --fov 60
./build/render-qt.exe  assets/cube.obj
```

> Visual Studio + vcpkg also works (install `sfml` and optional `qtbase`, then pass the vcpkg toolchain to CMake).

---

## Controls (Qt viewer)
- **Left‑drag**: orbit (yaw/pitch)  
- **Right‑drag**: pan (moves target)  
- **Wheel**: zoom (radius in perspective; scale in orthographic)  
- **O**: toggle perspective/orthographic  
- **R**: reset view  
- **A**: toggle antialias  
- **F**: toggle fast/LOD mode  
- **T**: toggle FPS target (30/60)  
- **ESC**: quit  

A compact HUD shows FPS, edges drawn, AA/LOD status, and projection mode.

> If you removed the SFML viewer, only the Qt controls apply.

---

## CLI usage

```
render-cli <input.obj> <output.png>
           [--eye x y z] [--target x y z]
           [--fov deg] [--size W H]
           [--ortho scale]
```

**Examples**
```bash
# Default camera, 1000x800
./build/render-cli assets/cube.obj out.png

# Set camera by eye and target
./build/render-cli assets/cube.obj out.png --eye 3 2 4 --target 0 0 0 --size 1600 1200 --fov 55

# Orthographic output
./build/render-cli assets/cube.obj ortho.png --ortho 1.2 --size 1200 900
```

---

## Project layout

```
.
├─ CMakeLists.txt
├─ assets/
│  ├─ cube.obj
│  └─ (your other .obj files)
└─ src/
   ├─ core/
   │  ├─ Math.h
   │  ├─ Mesh.h
   │  ├─ Geometry.h
   │  ├─ Camera.h
   │  ├─ ObjLoader.h / .cpp
   │  ├─ Renderer.h  / .cpp
   └─ apps/
      ├─ render_cli.cpp
      ├─ render_qt.cpp        # Qt viewer (requires Qt6)
      └─ render_gui.cpp       # optional SFML viewer — remove this file if unused
```

> If you delete `src/apps/render_gui.cpp`, either remove the `render-gui` target from `CMakeLists.txt` or guard it with `if(EXISTS ...)` so CMake won’t look for it.

---

## Common pitfalls & fixes

- **“The source directory …/build does not appear to contain CMakeLists.txt”**  
  You ran CMake from inside `build/`. Run from the project root:  
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_QT=ON
  ```

- **Qt Wayland error in WSL**:  
  `Failed to create wl_display` or “Could not load the Qt platform plugin 'wayland'” →  
  ```bash
  export QT_QPA_PLATFORM=xcb
  ```
  before running `render-qt`.

- **`QApplication: No such file or directory` / `libQt6Widgets.so.6: cannot open shared object file`**  
  Install Qt dev package and rebuild: `sudo apt install -y qt6-base-dev`.

- **Nothing shows / missing assets**  
  Run from the project root so relative paths like `assets/cube.obj` resolve correctly.

- **Clock skew warnings during build**  
  Benign on WSL/Windows (filesystem time differences). Usually safe to ignore.

---

## Notes
- Right‑handed camera; near‑plane clipping in camera space before projection.
- Orthographic volume is `[-s*aspect, s*aspect] × [-s, s]`, where `s = orthoScale`.
- For very heavy meshes, the Qt viewer adapts LOD to hit your FPS target *(press **T** to toggle 30/60)*.
