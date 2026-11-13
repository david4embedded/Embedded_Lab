# TinyML gesture test (Windows / MinGW)

This project runs the exported Edge Impulse C++ impulse against IMU CSV files on
Windows. It is an offline baseline for checking prepared samples before wiring
the classifier to the live serial plotter or porting it to the nRF board.

## Requirements

- CMake 3.20 or newer
- MinGW GCC/G++ and `mingw32-make` available on `PATH`

Check that the commands are available in PowerShell:

```powershell
cmake --version
gcc --version
g++ --version
mingw32-make --version
```

## Configure and build

Open PowerShell and change to this project directory:

```powershell
cd C:\Repository\Embedded_Lab\source\app\TinyML#1_Win
```

Configure and build the Debug version:

```powershell
cmake --preset mingw-debug
cmake --build --preset mingw-debug
```

The Debug executable is `build\tinyml_gesture_test.exe`. To build the optimized
Release version instead, run:

```powershell
cmake --preset mingw-release
cmake --build --preset mingw-release
```

The Release executable is `build-release\tinyml_gesture_test.exe`. After the
initial configure, rerun only the `cmake --build --preset ...` command when
source files change.

## Classify a CSV

```powershell
.\build\tinyml_gesture_test.exe ..\nRF54L15L_Sense\scripts\acc_gesture_x_20261106_211753.csv
```

Replace the example CSV path with the file to test. The CSV must have `ax`,
`ay`, and `az` columns. The app uses the exported model's configured input
window: 50 Hz, 50 samples (1 second), and interleaved `ax, ay, az` values. It
classifies complete, non-overlapping windows and reports each class score. A
final incomplete window is ignored.

Use the same acceleration units, axis ordering, and sample rate used to collect
the model's training data.
