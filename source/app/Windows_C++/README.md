# Windows C++ App (CMake + GCC)

A minimal, hardware-free C++ desktop application used for host-side
development and debugging. It builds with **GCC (Strawberry Perl toolchain)**
via **CMake presets** — no MSVC required.

## Layout
```
Windows_C++/
├── CMakeLists.txt        # C++17, GCC warnings, debug symbols
├── CMakePresets.json     # gcc-debug (Makefiles) / gcc-debug-ninja
├── include/Greeter.h     # sample class + free function
├── src/Greeter.cpp
├── src/main.cpp          # entry point
└── .vscode/              # tasks, launch (GDB), IntelliSense
```

## Requirements
- CMake 3.20+
- GCC/G++ and GDB at `C:/Strawberry/c/bin` (gcc, g++, gdb, mingw32-make, ninja)
- VS Code C/C++ extension (+ CMake Tools)

## Build (command line)
```sh
cmake --preset gcc-debug          # configure -> build/
cmake --build --preset gcc-debug  # -> build/main_app.exe
./build/main_app.exe
```
A Ninja variant is available via the `gcc-debug-ninja` preset (`build-ninja/`).

## Build (VS Code)
1. Open this folder in VS Code.
2. `Ctrl+Shift+B` runs the default build task (CMake configure + build).

## Debug (no hardware)
1. Press `F5` and select **CMake + GCC: Debug**.
2. VS Code builds first, then launches GDB.
3. Set breakpoints, step, and inspect variables — runs entirely on the host.
