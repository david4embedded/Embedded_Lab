* NOTE
  * Hardware check application for Seeed Studio XIAO nRF54L15 Sense, built on nRF Connect SDK v3.2.4.
  * Checks the on-board LED, user button, 6-axis IMU and PDM microphone and prints a PASS/FAIL summary at boot.
    Live IMU/mic readings are available through the shell (`imu`, `mic` commands).
  * The board DTS describes the IMU as `st,lsm6dso` (WHO_AM_I = 0x6C), but the Sense board carries an LSM6DS3TR-C (WHO_AM_I = 0x6A), so the Zephyr lsm6dso driver fails to initialize.
    This app reads the IMU registers over I2C directly (`src/Imu.cpp`) instead of using the sensor driver.

* What is checked
  | Item   | Pin / Bus              | Check                                                        |
  |--------|------------------------|--------------------------------------------------------------|
  | LED    | P2.00                  | Blinks 3 times at boot, then heartbeat (toggle every 500 ms) |
  | Button | P0.00 (USR)            | LED stays on while pressed                                   |
  | IMU    | i2c30 (P0.04/P0.03) 0x6A | WHO_AM_I, accel [mg], gyro [mdps], temperature [C]         |
  | Mic    | pdm20 (P1.12/P1.13)    | 16 kHz mono capture, peak/RMS level                          |

* Steps to build through nRF Connect extension in VS Code
  1. Open this folder with VS Code.
  2. Click on the nRF Connect extension.
  3. Add build configuration under `APPLICATIONS` menu on the left.
  4. Choose `xiao_nrf54l15/nrf54l15/cpuapp` as the board target and the default settings for the others.
  5. Generate and build.
  6. Flash from the command line (see below). The `Flash` action of the extension passes `-i <serial>` to west, which the OpenOCD runner of this board rejects (`openocd doesn't support --dev-id option`).

* Steps to set up the toolchain environment in PowerShell
  * `west` on the system PATH and the system `ZEPHYR_BASE` may point to another workspace (e.g. `C:\Development\bl-os-workspace`).
    Run the following in the PowerShell window before building/flashing. It only affects the current window.
  ```powershell
  $T = "C:\ncs\toolchains\fd21892d0f"
  $env:PATH = "$T;$T\mingw64\bin;$T\bin;$T\opt\bin;$T\opt\bin\Scripts;$T\opt\nanopb\generator-bin;$T\nrfutil\bin;$T\opt\zephyr-sdk\arm-zephyr-eabi\bin;$env:PATH"
  $env:Path = "C:\openocd\OpenOCD-20260302-0.12.0\bin;$env:Path"
  $env:PYTHONPATH = "$T\opt\bin;$T\opt\bin\Lib;$T\opt\bin\Lib\site-packages"
  $env:NRFUTIL_HOME = "$T\nrfutil\home"
  $env:ZEPHYR_TOOLCHAIN_VARIANT = "zephyr"
  $env:ZEPHYR_SDK_INSTALL_DIR = "$T\opt\zephyr-sdk"
  $env:ZEPHYR_BASE = "C:\ncs\v3.2.4\zephyr"
  cd C:\ncs\v3.2.4
  ```
  * The values are taken from `C:\ncs\toolchains\fd21892d0f\environment.json` (toolchain for NCS v3.2.4).
  * `cd C:\ncs\v3.2.4` lets west find the NCS workspace (`.west`).

* Steps to build from the command line
  ```
  west build -p always -b xiao_nrf54l15/nrf54l15/cpuapp -d [source directory]\build [source directory]
  ```

* Steps to flash from the command line
  * The board has no J-Link. The on-board SAMD11 works as a CMSIS-DAP probe (VID:PID `2886:0066`), and the default runner is OpenOCD.
    `nrfutil device` / `nrfjprog` may not detect the board.
  * `openocd` must be on the PATH. The NCS toolchain does not include it
  * On Windows, run `scripts\flash.bat` from any directory to set up the toolchain environment and flash this application's `build` directory. Extra `west flash` options can be passed through, for example:
  ```
  scripts\flash.bat --cmd-pre-init "adapter serial 716B57F9"
  ```
  * With only one board connected:
  ```
  west flash -d [build directory]
  ```
  * With several probes connected, select the probe by its serial number through an OpenOCD command (`-i` / `--dev-id` is not supported by the OpenOCD runner):
  ```
  west flash -d [build directory] --cmd-pre-init "adapter serial [serial#]"
  ```
  * The probe serial number is printed by OpenOCD during flashing (`CMSIS-DAP: Serial# = 716B57F9`). `716B57F9` is the serial of the board used during development; replace it with your own.
  * A successful flash ends with lines like:
  ```
  downloaded 44972 bytes in 0.844738s (51.990 KiB/s)
  shutdown command invoked
  ```
  * To reset the board without flashing:
  ```
  openocd -f C:\ncs\v3.2.4\zephyr\boards\seeed\xiao_nrf54l15\support\openocd.cfg -c "init; reset run; shutdown"
  ```

* Console
  * USB CDC ACM serial port (e.g. COM5), 115200 8N1.
  * Expected boot output:
  ```
  ---------------- Self test -------------------
  [PASS] LED    (P2.00)
  [PASS] Button (P0.00)
  [PASS] IMU    WHO_AM_I=0x6A (LSM6DS3TR-C), ret=0
  [PASS] Mic    PDM 16 kHz mono, ret=0, peak=12
  ----------------------------------------------
  ```

* Shell commands (`src/AppShell.cpp`)
  | Command                  | Description                                                                        |
  |--------------------------|------------------------------------------------------------------------------------|
  | `imu info`               | WHO_AM_I and chip name                                                             |
  | `imu read`               | One accel/gyro/temperature sample                                                  |
  | `imu stream <ms>`        | Stream samples every `<ms>` (10 ~ 60000) until ESC is pressed                      |
  | `mic monitor [ms]`       | Print the sound level every `[ms]` (100 ~ 10000, multiple of 100, default 100) until ESC is pressed |
  * Streaming runs on a separate thread while the shell is in bypass mode (same approach as `net ping`). Every key input goes to the stream; only a single ESC stops it. Arrow/function keys (ESC [ ... sequences) are ignored.
  * To view live IMU graphs in a browser instead of Tera Term, close Tera Term, install the Python dependency with `py -m pip install pyserial`, then run `py scripts\imu_plot_server.py --serial-port COM5`. Replace `COM5` with the board's serial port. Open `http://127.0.0.1:8000`, set the **Sample interval (ms)** (10 ~ 60000), and press **Start stream**; the selected interval is sent to the board. Press **Stop stream** to pause it. **Clear graph** clears the displayed history without stopping the stream. Ctrl+C in the terminal stops the server. The initial interval is 20 ms; use `--interval 50` to change it. Graphs default to a 10-second fixed window; use the **Show accumulated** button to switch to accumulated history, or adjust the window size in the UI. To keep drawing responsive, graphs redraw at up to 30 frames per second even when samples arrive faster. Use `--window-seconds 20` to set a different initial window or `--http-port 8080` to change the browser port.
  * `imu stream` example. With the board lying flat, accel Z should read about 1000 mg.
  ```
  uart:~$ imu stream 100
  Streaming IMU every 100 ms. Press ESC to stop.
    time[ms] |  ax[mg]  ay[mg]  az[mg] | gx[mdps] gy[mdps] gz[mdps] | temp[C]
           0 |     -32      -8    1056 |      122    -2362      700 |  19.50
  ```
  * `mic monitor` shows a level meter in the terminal.
    Raw PCM (16 kHz x 16 bit = 32 KB/s) does not fit into the 115200 baud UART (about 11.5 KB/s), so the level is computed on the board per 100 ms block.
    * `peak`: largest absolute sample after DC removal (0 ~ 32767)
    * `rms`: RMS amplitude after DC removal; this follows perceived loudness better than peak
    * `rms[dBFS]`: RMS in dB relative to full scale (0 dBFS = 32768). A quiet room reads about -80 dBFS.
    * `level`: one `#` per 2 dB from -90 dBFS to -20 dBFS. `CLIP` is shown when peak reaches 32000, `FLAT` when all samples are identical (no data from the mic).
  ```
  uart:~$ mic monitor
    time[ms] |  peak |   rms | rms[dBFS] | level
         100 |    12 |     3 |     -80.8 | ####...............................
  ```
  * If the mic level does not change while talking near the board, change `MIC_CHANNEL` in `src/Mic.cpp` to `PDM_CHAN_RIGHT`.
