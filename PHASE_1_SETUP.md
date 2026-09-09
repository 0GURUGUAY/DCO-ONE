# DCO-ONE Phase 1 - Setup & Verification

## Objective
Verify the connection and basic functionality of:
- Daisy Seed 3 (audio generation)
- ESP32-S3 with Waveshare AMOLED 1.75" display (waveform visualization)

## Phase 1 Implementation

### Part A: Daisy Seed 3 - Sine Wave Generator

**What it does:**
- Initializes the Daisy Seed 3
- Generates a 440 Hz sine wave at 48 kHz sample rate
- Outputs the signal to both audio channels
- Amplitude limited to 30% to prevent clipping

**Files:**
- `daisy/src/main.cpp` - Main audio callback and initialization
- `daisy/src/oscillator.cpp` - Oscillator implementation using DaisySP
- `daisy/include/oscillator.h` - Oscillator class definition
- `daisy/CMakeLists.txt` - Build configuration

**Prerequisites:**
1. Install ARM compiler:
```bash
brew install arm-none-eabi-gcc arm-none-eabi-binutils
```

2. Clone Daisy libraries:
```bash
mkdir -p ~/daisy-dev && cd ~/daisy-dev
git clone https://github.com/electro-smith/libDaisy.git
git clone https://github.com/electro-smith/DaisySP.git

# Set environment variable
echo 'export DAISY_ROOT=~/daisy-dev/libDaisy' >> ~/.zshrc
source ~/.zshrc
```

3. Update CMakeLists.txt paths to match your system:
```cmake
set(DAISY_ROOT "${HOME}/daisy-dev/libDaisy")
set(DAISYSP_DIR "${HOME}/daisy-dev/DaisySP")
```

**Build instructions:**
```bash
cd /Users/maxpatissier/Downloads/DCO-ONE/daisy
mkdir -p build
cd build
cmake ..
make
```

**Upload instructions (DFU mode):**
```bash
# 1. Put Daisy in DFU mode: Hold BOOT button, press RESET, release both
# 2. Upload firmware
make program-dfu
```

### Part B: ESP32-S3 - Oscilloscope Display

**What it does:**
- Receives audio samples via UART from the Daisy (Phase 2 communication)
- Buffers samples for display
- Renders a real-time oscilloscope waveform visualization
- Updates at ~60 FPS

**Files:**
- `esp32/src/main.cpp` - Display rendering and audio reception
- `esp32/platformio.ini` - Project configuration for PlatformIO

**Build & Upload instructions:**
```bash
cd esp32
platformio run --target upload --upload-port /dev/cu.usbserial-<port>
platformio device monitor --port /dev/cu.usbserial-<port>
```

## Hardware Verification Checklist

- [ ] Daisy Seed 3 connected to Mac Mini A4 Pro via USB
- [ ] ESP32-S3 Waveshare AMOLED connected to Mac Mini A4 Pro via USB
- [ ] Verify Daisy board is recognized: `ls /dev/cu.usbserial-*`
- [ ] Verify ESP32-S3 board is recognized: `ls /dev/cu.usbserial-*` or `ls /dev/ttyUSB*`
- [ ] Daisy firmware compiled and uploaded
- [ ] ESP32-S3 firmware compiled and uploaded

## Testing Procedure

### Step 1: Verify Daisy Audio Generation
1. Connect the Daisy Seed 3 to the Mac Mini
2. Upload the firmware
3. Connect audio monitor/oscilloscope to Daisy audio outputs
4. Verify 440 Hz sine wave is present on both channels
5. Check amplitude (~1.5V peak for 0.3 amplitude at line level)

### Step 2: Verify ESP32-S3 Display
1. Connect the ESP32-S3 to the Mac Mini
2. Upload the firmware
3. Verify display powers on (check backlight)
4. Monitor serial output: `platformio device monitor`
5. Check for initialization messages

### Step 3: Phase 2 Preparation
For Phase 2, the Daisy and ESP32-S3 will be connected via UART:
- Daisy TX (D0) → ESP32-S3 RX (GPIO 44)
- Daisy RX (D1) → ESP32-S3 TX (GPIO 43)
- Common GND

## Current Limitations

**This is Phase 1 - minimal viable test**

- No UART communication between Daisy and ESP32-S3 (implemented in Phase 2)
- No actual display driver for Waveshare AMOLED (placeholder code)
- Audio output is through Daisy audio I/O, not USB Audio (Phase 6)
- No parameter control or menus (Phase 8+)
- No sequencer or presets (Phase 11-12)

## Expected Results

### Daisy Output
- Clean 440 Hz sine wave on audio outputs
- No glitches or dropouts
- Stable amplitude and frequency

### ESP32-S3 Output
- Display initializes without errors
- Serial monitor shows successful startup
- Ready for Phase 2 integration

## Next Steps

After Phase 1 verification:
1. Implement Phase 2: Audio input/output on Daisy
2. Add UART communication between processors
3. Integrate display with real waveform data
4. Add physical controls (encoders, buttons)

## Troubleshooting

### ARM compiler not found
```bash
# Verify installation
arm-none-eabi-gcc --version

# If not found, reinstall
brew install arm-none-eabi-gcc arm-none-eabi-binutils
```

### Daisy libraries not found
```bash
# Verify environment variable
echo $DAISY_ROOT

# Should output: /Users/maxpatissier/daisy-dev/libDaisy
# If not set, add to ~/.zshrc:
echo 'export DAISY_ROOT=~/daisy-dev/libDaisy' >> ~/.zshrc
source ~/.zshrc

# Verify directories exist
ls ~/daisy-dev/libDaisy
ls ~/daisy-dev/DaisySP
```

### Daisy doesn't compile
- Verify ARM compiler: `arm-none-eabi-gcc --version`
- Check DAISY_ROOT is set: `echo $DAISY_ROOT`
- Verify paths in CMakeLists.txt match your setup
- Check libDaisy and DaisySP exist: `ls ~/daisy-dev/`

### Daisy upload fails
- Ensure Daisy is in DFU mode (Hold BOOT, press RESET)
- Check USB connection
- Verify `make program-dfu` recognizes the device

### ESP32-S3 upload fails
- Check USB cable connection
- Verify COM port in platformio.ini
- Try different USB port on Mac
- Restart the board (press RESET)

### Display shows nothing
- Verify display is powered (check 3.3V supply)
- Check SPI connections (CS, DC, CLK, MOSI)
- Verify display reset pin is connected
- Run: `platformio device monitor` to check for errors

### Audio not heard
- Check audio cable connections
- Verify Daisy audio codec is initialized
- Check amplitude settings (currently 30%)
- Monitor serial output for errors
