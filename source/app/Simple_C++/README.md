# Windows C++ App (CMake + GCC)

A minimal, hardware-free C++ desktop application used for host-side
development and debugging. It builds with **GCC (Strawberry Perl toolchain)**
via **CMake presets** — no MSVC required.

## Layout
```
Windows_C++/
├── CMakeLists.txt        # C++17, GCC warnings, debug symbols
├── CMakePresets.json     # gcc-debug (Makefiles) / gcc-debug-ninja
├── src/main.cpp          # entry point (Hello, World!)
└── .vscode/              # tasks, launch (GDB), IntelliSense
```

## Requirements
- CMake 3.20+
- GCC/G++ and GDB at `C:/Strawberry/c/bin` (gcc, g++, gdb, mingw32-make, ninja)
- VS Code C/C++ extension (+ CMake Tools)

## Build settings
- **C++17**, GNU extensions off (`CMAKE_CXX_EXTENSIONS OFF`)
- GCC warnings: `-Wall -Wextra -Wpedantic`
- Debug build: `-g3 -O0` for full symbols and no optimization
- `compile_commands.json` exported for IntelliSense / clangd

## Build (command line)
```sh
cmake --preset gcc-debug          # configure -> build/
cmake --build --preset gcc-debug  # -> build/main_app.exe
./build/main_app.exe              # prints: Hello, World!
```
A Ninja variant is available via the `gcc-debug-ninja` preset (`build-ninja/`).

## Build (VS Code)
1. Open this folder in VS Code.
2. `Ctrl+Shift+B` runs the default build task (CMake configure + build).

## Debug (no hardware)
1. Press `F5` and select **CMake + GCC: Debug**.
2. VS Code builds first, then launches GDB.
3. Set breakpoints, step, and inspect variables — runs entirely on the host.
