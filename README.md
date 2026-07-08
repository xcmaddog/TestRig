# CREATE Lab Inkjet Test Rig

Qt6/C++ GUI for the inkjet test rig.  Runs on a Raspberry Pi connected to a
**Galil DMC-4080-C022** motion controller and a **JetForge** (Added Scientific)
Xaar printhead controller, with optional stroboscopic imaging via an
**IDS UI-3370CP** camera and a **Teensy 4.0** strobe driver.

---

## Hardware summary

| Component | Interface | Notes |
|-----------|-----------|-------|
| Galil DMC-4080-C022 | Ethernet (192.168.42.x) | Axes A (X carriage) and D (reservoir height) |
| JetForge (Added Scientific) | USB serial | Xaar 128-nozzle printhead controller |
| Air solenoid valve | Galil DO1 or extended I/O bit 30 | Quick purge |
| IDS UI-3370CP camera | USB3 | Stroboscopic imaging (optional) |
| Strobe Teensy 4.0 | USB serial | Custom PCB; firmware in `Teensy/` |

---

## Setting up a fresh Raspberry Pi

The following assumes **Raspberry Pi OS (Bookworm / trixie-based)** on a
Pi 4 or Pi 5, freshly flashed and SSH-accessible.

### 1 — Update the system

```bash
sudo apt update && sudo apt full-upgrade -y
```

### 2 — Install build tools and Qt6

```bash
sudo apt install -y \
    git cmake ninja-build \
    qt6-base-dev qt6-serialport-dev \
    libgl-dev
```

### 3 — Install gclib (Galil motion library)

Galil publishes a Debian APT repository, but it uses **SHA1 signatures**
which Debian trixie rejects by default.  Work around this:

```bash
# 3a — Add Galil's repo with the required override flags.
#      (The file likely already exists; check before adding it.)
sudo tee /etc/apt/sources.list.d/galil-release.list <<'EOF'
deb [trusted=yes arch=arm64] http://apt.galil.com/galil stable main
EOF

# 3b — Install
sudo apt update
sudo apt install -y gclib gcapsd
```

> **If apt complains about GPG / SHA1:** the `trusted=yes` flag above
> bypasses the signature check entirely.  This is acceptable on a
> lab-internal machine.  You can also import Galil's key manually if you
> prefer stricter checking — see Galil's online documentation.

Verify the install:

```bash
ls /usr/include/gclib.h      # should exist
ls /usr/lib/libgclib.so      # or /usr/lib/aarch64-linux-gnu/libgclib.so
```

### 4 — Assign a static IP on eth0

The DMC-4080 ships with a default IP of `192.168.0.42`.  It can be
reconfigured, but the simplest approach is to put the Pi on the same
`192.168.42.x` subnet (or whichever subnet the controller is on).

```bash
# Replace 192.168.42.10 with an address not already in use on that subnet.
sudo nmcli connection modify "Wired connection 1" \
    ipv4.method manual \
    ipv4.addresses "192.168.42.10/24" \
    ipv4.gateway "" \
    ipv4.dns ""

sudo nmcli connection up "Wired connection 1"
```

Test reachability:

```bash
ping 192.168.42.100   # the controller's IP
```

If the controller was given a different IP during commissioning, update the
address in the GUI's **Galil Connection** bar before clicking Connect.

### 5 — Configure the Galil axes (first-time only)

The DMC-4080 ships with motor type set to **servo** (`MT=-1`).  The FSL30
linear stages use stepper motors.  If `program.dmc` has not yet been
uploaded to the controller, you need to configure this manually once.

Connect via telnet:

```bash
telnet 192.168.42.100
```

Then send:

```
MTA=2   (X-axis: stepper, active-low step)
MTD=2   (D-axis: stepper, active-low step)
SHA     (enable X motor)
SHD     (enable D motor)
```

Once `program.dmc` is uploaded (see §8), `#AUTO` does this at every power-up
and the manual step is no longer needed.

> **Axis letter note:** The Galil accepts both the axis letter (`A`, `D`) and
> the axis name (`X`).  This project uses the letter form throughout.  `SHA`
> and `SHX` are equivalent; `SHD` has no name alias.

### 6 — Install IDS peak (optional — camera support)

Skip this section if you are not using the IDS camera.  The GUI compiles and
runs without IDS peak; the Camera tab reports "camera unavailable" at runtime.

Download the IDS peak installer for **Linux ARM64** from
https://en.ids-imaging.com/ids-peak.html.

```bash
# Unpack and run the installer (name will differ by version):
chmod +x ids_peak_*_arm64.run
sudo ./ids_peak_*_arm64.run
```

The SDK installs to `/opt/ids/peak` by default.  If you choose a different
path, pass it to CMake:

```bash
cmake -DIDS_PEAK_ROOT=/your/path ..
```

After installing, check that the camera is recognised:

```bash
lsusb | grep IDS
```

### 7 — Clone and build

```bash
git clone https://github.com/YOUR-ORG/TestRig.git
cd TestRig
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja
```

The resulting binary is `build/TestRig`.

To build **without** IDS peak (camera stub only):

```bash
cmake .. -DWITH_IDS_PEAK=OFF -G Ninja
ninja
```

### 8 — Upload program.dmc to the controller

The `program.dmc` file configures the controller at power-up.  Upload it
once after commissioning; the controller stores it in non-volatile memory.

**From the GUI:** (not yet implemented — use Galil tools for now)

**From Galil Composer** (Windows only):  File → Download → select `program.dmc`.

**From gcapsd / telnet:**

```bash
# Use gclib's program download utility if available:
gcapsd -a 192.168.42.100 -d program.dmc

# Or paste the contents line-by-line into telnet.
```

After uploading, reset the controller (`RS` in telnet or power-cycle) and
verify the motors energise automatically.

---

## Running the GUI

```bash
./build/TestRig
```

### Typical startup sequence

1. **Connect to Galil** — enter the controller IP in the connection bar and
   click **Connect**.  The status dot turns green.  Both axis motor-enable
   buttons update to reflect the motor state.

2. **Home the axes** (optional but recommended) — click **Home (Rev Limit →
   Zero)** on each axis in the Axes tab.  The carriage jogs slowly toward its
   reverse limit switch; when the limit trips, position is defined as 0 mm.

3. **Connect to JetForge** — go to the Printhead tab, enter the serial port
   (e.g. `/dev/ttyACM0`), and click **Connect**.  See the note below about
   identifying which Teensy is which.

4. **Power on the printhead** — click **Power On**.

5. **Load a bitmap** — click Browse and select a `.bmp` file.  The image is
   converted to a bitstream and sent to the printhead.

6. **Print** — use the **Test Jet** button for a single encoder-triggered
   pass at a predefined location, or the jog controls to position manually
   and then trigger a pass.

### Identifying the two Teensy devices

Two Teensy devices appear on USB: the **JetForge** (Added Scientific Xaar
printhead controller) and the **strobe Teensy** (custom PCB for the LED
flash).  Both enumerate as PJRC serial ports.

To tell them apart:
- Plug in one at a time and note which port appears (`ls /dev/ttyACM*`).
- Or: send a command in the JetForge's serial command field and watch which
  port responds.

The **JetForge** goes in the **Printhead tab** port field.
The **strobe Teensy** goes in the **Camera/Strobe tab** Teensy Connection section.

---

## Solenoid valve wiring

The quick-purge solenoid is controlled by a Galil digital output.  Two
options are supported:

| Option | Bit | Notes |
|--------|-----|-------|
| **DO1** (try first) | `SOLENOID_DO1_BIT = 1` | Galil high-power optoisolated output |
| **Extended I/O** (fallback) | `SOLENOID_EXTIO_BIT = 30` | Requires external MOSFET circuit, same as the custom printer |

If DO1 does not actuate the solenoid, check the **"Use extended I/O"**
checkbox in the Quick Purge panel and wire up the MOSFET circuit to extended
I/O bit 30.

To change the bit numbers, edit the `#define` values in `src/printer.h`:

```cpp
#define SOLENOID_DO1_BIT   1
#define SOLENOID_EXTIO_BIT 30
```

---

## JetForge / MJ_START and MJ_DIR bits

The JetForge receives a print-start trigger and a direction signal from the
Galil's extended I/O.  The default bit assignments (matching the custom
printer's wiring) are in `src/printer.h`:

```cpp
#define MJ_START_BIT  23   // pin 18
#define MJ_DIR_BIT    22   // pin 32
```

**Verify these against the test rig's actual wiring before the first print
pass.**  If the bits are wrong, the printhead will fire at the wrong time or
not at all.

---

## Axis calibration

Both axes use the same FUYU FSL30 linear stage with a Nema 14 stepper and
a 2 mm pitch leadscrew at 256 microsteps:

```
MICROSTEPS_PER_MM = 200 steps/rev × 256 microsteps × (1 rev / 2 mm) = 25 600 cnt/mm
```

When the X carriage is replaced with the longer stage used on the custom
printer, update `X_CNTS_PER_MM` in `src/printer.h` if the new stage has a
different pitch or microstepping setting.  The D-axis constant
(`D_CNTS_PER_MM`) is independent and does not need to change.

---

## Troubleshooting

### Motor does not move after `SHA` / `SHD`
- Confirm the `#AUTO` program has been uploaded and the controller was reset.
- Check `MT` in telnet: `MG _MTA` should return `2`.  If it returns `-1`,
  `MTX=2` has not been applied — upload `program.dmc` and reset.
- The controller needs `SH` before it will accept `BG`.  If the GUI connects
  before `#AUTO` finishes (rare), click **Enable Motor** manually in the
  Axes tab.

### gclib `GOpen` fails
- Confirm the static IP is set on `eth0` and the Pi can ping the controller.
- Confirm `gclib` and `gcapsd` are installed (`dpkg -l gclib gcapsd`).
- The DMC-4080-C022 default port is 23 (telnet).  If your network blocks 23,
  configure the controller to use a different port via Galil Composer.

### Camera tab says "unavailable"
- Install IDS peak for ARM64 (see §6).
- Check `lsusb` shows the camera.
- Run `ids_peak_cockpit` to verify the camera opens with the IDS tools before
  trying the GUI.

### Two Teensys appear on the same port
- This cannot happen — each USB device gets its own `/dev/ttyACMx` node.
  Re-scan with the Rescan button in the Teensy Connection group after plugging
  in or unplugging.

---

## Project structure

```
TestRig/
├── CMakeLists.txt
├── program.dmc               DMC controller startup program
├── README.md
└── src/
    ├── printer.h/cpp         Axis enum, constants, CMD namespace
    ├── galilcontroller.h/cpp Multi-axis gclib wrapper
    ├── asyncserialdevice.h/cpp  Serial base class (from custom printer)
    ├── mfjdrv.h              JetDrive command definitions (reference)
    ├── mjdriver.h/cpp        Added Scientific (JetForge) serial driver
    ├── printheadwidget.h/cpp Printhead control tab
    ├── cameracontroller.h/cpp  IDS peak wrapper (pimpl)
    ├── cameraworker.h/cpp    Acquisition thread
    ├── camerawidget.h/cpp    Camera settings UI
    ├── liveviewwidget.h/cpp  Live frame display
    ├── strobewidget.h/cpp    Start/Stop acquisition
    ├── arduinocontroller.h/cpp  Teensy serial protocol
    ├── arduinowidget.h/cpp   Strobe timing UI
    └── mainwindow.h/cpp      Main window + Axes tab
```
