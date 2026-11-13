# TinyML gesture test (Windows / MinGW)

This project runs the exported Edge Impulse C++ impulse against IMU CSV files on
Windows. It is an offline baseline for checking prepared samples before wiring
the classifier to the live serial plotter or porting it to the nRF board.

## Requirements

- MinGW GCC/G++ and `mingw32-make` available on `PATH`

## Build

Run `build.bat` from Explorer or a terminal, or from PowerShell:

```powershell
.\build.bat
```

The executable is written to `build\tinyml_gesture_test.exe`.

## Classify a CSV

```powershell
.\build\tinyml_gesture_test.exe ..\nRF54L15L_Sense\scripts\acc_gesture_x_20261106_211753.csv
```

The CSV must have `ax`, `ay`, and `az` columns. The app uses the exported model's
configured input window: 50 Hz, 50 samples (1 second), and interleaved `ax, ay,
az` values. It classifies complete, non-overlapping windows and reports each
class score. A final incomplete window is ignored.

Use the same acceleration units, axis ordering, and sample rate used to collect
the model's training data.
