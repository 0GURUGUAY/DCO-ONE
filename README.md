# DCO-ONE Phase 1 - Hardware Verification

This is the Phase 1 implementation of DCO-ONE: a dual-processor audio instrument based on **Daisy Seed 3** and **ESP32-S3 with Waveshare AMOLED 1.75" display**.

## Phase 1 Goals

✓ Verify Daisy Seed 3 connection to Mac Mini A4 Pro  
✓ Verify ESP32-S3 Waveshare AMOLED connection to Mac Mini A4 Pro  
✓ Generate a stable 440 Hz sine wave on the Daisy  
✓ Display waveform visualization on the ESP32-S3 display  

## Quick Start

### 0. Install Required Tools

```bash
# Install ARM compiler
brew install arm-none-eabi-gcc arm-none-eabi-binutils

# Clone Daisy libraries
mkdir -p ~/daisy-dev && cd ~/daisy-dev
git clone https://github.com/electro-smith/libDaisy.git
git clone https://github.com/electro-smith/DaisySP.git

# Set environment variable (add to ~/.zshrc or ~/.bash_profile)
echo 'export DAISY_ROOT=~/daisy-dev/libDaisy' >> ~/.zshrc
source ~/.zshrc

# Verify installation
arm-none-eabi-gcc --version
```

### 1. Check Hardware Connection

```bash
make check-hardware
```

This verifies:
- ✓ Both boards are recognized by the OS
- ✓ Required development tools are installed
- ✓ ARM compiler is available
- ✓ PlatformIO is installed for ESP32-S3

### 2. Build All Firmware

```bash
make build-all
```

This compiles:
- Daisy Seed 3 firmware (sine wave generator)
- ESP32-S3 firmware (oscilloscope display)

### 3. Upload Firmware

**Daisy Seed 3** (put board in DFU mode first):
```bash
make upload-daisy
```

**ESP32-S3** (standard USB upload):
```bash
make upload-esp32
```

### 4. Verify Operation

Monitor the outputs:
```bash
# Monitor Daisy
make monitor-daisy

# Monitor ESP32-S3
make monitor-esp32
```

## Project Structure

```
DCO-ONE/
├── daisy/                    # Daisy Seed 3 firmware
│   ├── src/
│   │   ├── main.cpp         # Audio callback & initialization
│   │   └── oscillator.cpp   # Sine wave generator
│   ├── include/
│   │   └── oscillator.h     # Oscillator class
│   └── CMakeLists.txt       # Build configuration
│
├── esp32/                    # ESP32-S3 firmware
│   ├── src/
│   │   └── main.cpp         # Display & audio reception
│   └── platformio.ini       # PlatformIO config
│
├── shared/                   # (Future) Shared definitions
├── backup/                   # Backup files
│
├── PHASE_1_SETUP.md         # Detailed setup instructions
├── PROJECT_ARCHITECTURE_V2.md # Full project specification
├── Makefile                  # Build automation
├── check_hardware.sh         # Diagnostic script
└── README.md                 # This file
```

## Hardware Requirements

### Daisy Seed 3
- USB connection to Mac Mini A4 Pro
- Audio outputs (for verification)
- DFU mode capability for uploading firmware

### ESP32-S3 Waveshare AMOLED
- Waveshare ESP32-S3 Touch AMOLED 1.75" display module
- USB connection to Mac Mini A4 Pro
- 466×466 pixel circular OLED display

### Mac Mini A4 Pro (Development Host)
- macOS with USB ports
- Daisy development toolchain
- PlatformIO installed
- ARM GCC compiler

## Current Phase 1 Capabilities

| Feature | Status | Notes |
|---------|--------|-------|
| Daisy boots correctly | ✓ | Hardware initialization |
| 440 Hz sine wave generation | ✓ | Using DaisySP oscillator |
| Audio I/O functional | ✓ | 48 kHz, 24-bit stereo |
| ESP32-S3 boots correctly | ✓ | Basic initialization |
| Display driver present | ◐ | Placeholder implementation |
| Waveform visualization | ◐ | Display driver needed |
| UART communication | ✗ | Phase 2 feature |
| USB Audio | ✗ | Phase 6 feature |
| USB MIDI | ✗ | Phase 5 feature |

## Next Phase

**Phase 2**: Audio input/output and inter-processor communication via UART

See [PROJECT_ARCHITECTURE_V2.md](PROJECT_ARCHITECTURE_V2.md) for the full development roadmap.

## Troubleshooting

See [PHASE_1_SETUP.md](PHASE_1_SETUP.md#troubleshooting) for detailed troubleshooting.

### Quick Checks

1. **Board not detected?**
   ```bash
   ls /dev/cu.usbserial-*
   ```

2. **Build fails?**
   - Verify Daisy toolchain: `arm-none-eabi-gcc --version`
   - Verify PlatformIO: `platformio --version`

3. **Upload fails?**
   - Daisy: Put board in DFU mode (hold BOOT, press RESET)
   - ESP32-S3: Standard USB connection should work

## Development Notes

- This project uses **libDaisy** and **DaisySP** for audio DSP
- The Daisy Seed 3 is the **audio master**; real-time safety is critical
- The ESP32-S3 is the **display slave**; it can be disconnected without affecting audio
- All code follows the architectural rules defined in PROJECT_ARCHITECTURE_V2.md

## Resources

- [Daisy Seed 3 Documentation](https://docs.daisy.audio/hardware/Seed3/)
- [Waveshare AMOLED Documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75)
- [libDaisy Repository](https://github.com/electro-smith/libDaisy)
- [DaisySP Repository](https://github.com/electro-smith/DaisySP)
- [PlatformIO Documentation](https://docs.platformio.org/)
