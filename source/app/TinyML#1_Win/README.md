# TinyML gesture test (Windows / MinGW)

This project runs the exported Edge Impulse C++ impulse against IMU CSV files or
a live nRF serial stream on Windows.

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

## Live classification

Connect the nRF board and close `imu_plot_server` or any other program using
its serial port. Find the board's COM port in Windows Device Manager, then run:

```powershell
.\build\tinyml_gesture_test.exe --live COM5
```

The app opens the port at 115200 baud, starts `imu stream` at the model's
configured 20 ms interval, and parses the board's timestamp/IMU text output.
After collecting the first 50 acceleration samples, it classifies the latest
one-second window every 12 new samples (about 240 ms). Press Ctrl+C to send the
stop key to the board and exit. To use a different serial baud rate:

```powershell
.\build\tinyml_gesture_test.exe --live COM5 --baud 115200
```

The live serial module is isolated in `source/serial_port.hpp` and
`source/serial_port.cpp` so other Windows applications can reuse the COM-port
open, configure, line-read, and write behavior.
