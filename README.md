# DCO-ONE Phase 1 - Hardware Verification

This is the Phase 1 implementation of DCO-ONE: a dual-processor audio instrument based on **Daisy Seed 3** and **ESP32-S3 with Waveshare AMOLED 1.75" display**.

## Phase 1 Goals

✓ Verify Daisy Seed 3 connection to Mac Mini A4 Pro  
✓ Verify ESP32-S3 Waveshare AMOLED connection to Mac Mini A4 Pro  
✓ Generate a stable 440 Hz sine wave on the Daisy  
✓ Display waveform visualization on the ESP32-S3 display  

## Quick Start

### AMOLED Interface (September 2026)

The 466 x 466 interface keeps the existing encoders and FreeSans fonts, with a
restrained menu ring and contextual summaries for PLAY, OSC/FM, VCF, ENV1/2,
LFO1/2 and MATRIX. OSC curves illustrate the selected waveform; they are not
an oscilloscope measurement. The PLAY trace uses the received audio samples.

The display requires **both updated firmwares and a direct 3.3 V UART link**:
Daisy D13 (TX) -> ESP32 GPIO44 (RX), Daisy D14 (RX) <- ESP32 GPIO43 (TX),
and common GND. Do not connect the power rails when both boards have their own
supply. The link uses 921600 baud, 8N1, in both directions: display and SD
requests go to the ESP32, SD replies return to the Daisy. D29/D30 remain
reserved for USB. The return wire is required for SD even if the display works.

No Mac USB bridge is needed for the display or MIDI. The Daisy firmware now
exposes **native USB MIDI IN / OUT** as **DCO-ONE MIDI** on its own USB connector
using libDaisy's class-compliant MIDI device. Flash the updated Daisy firmware, press RESET,
and connect it directly to the Mac. No Python process, IAC bus or extra driver
is required. The ESP32 USB connector remains separate for power/flashing/logs.

In Ableton's MIDI preferences, select the newly detected Daisy MIDI port:
enable **Track** on the input to receive sequencer notes, and **Sync** only
when Ableton should follow the Daisy clock. Monitor/arm the receiving track.
To play the synth from Ableton, enable **Track** on the output and send to
the channel selected in the synth's MIDI > MIDI Channel menu. Sequencer output
remains on channel 1. Do not route received sequencer notes back to the Daisy,
as that would create a MIDI feedback loop.

Input supports Note On/Off, velocity and CC 123 (All Notes Off). Output includes
sequencer notes, MIDI Clock (24 PPQN), Start and Stop. Clock input, pitch bend,
mod wheel and aftertouch are not implemented by this change. A USB keyboard
must be routed through a computer/USB MIDI host; the Daisy is a USB device,
not a host for a directly attached keyboard.

USB CDC serial logging on the Daisy is disabled because the same connector
now enumerates as MIDI, not a serial port. The old bridge scripts are retained
for older CDC firmwares only; do not launch them with this firmware. Display
and SD transfers continue over the existing UART wiring.

Build and flash (hold BOOT, press RESET, then release BOOT before DFU):

```bash
cmake --build daisy/build -j4
arm-none-eabi-objcopy -O binary daisy/build/dco_one_phase1 /tmp/dco_one_usb_midi.bin
dfu-util -a 0 -D /tmp/dco_one_usb_midi.bin -s 0x08000000
```

Press RESET after flashing. To validate on hardware, check that the Daisy
appears in Audio MIDI Setup, receive sequencer notes/clock in Ableton, then
send Note On/Off on the configured input channel and verify sound/release.
These enumeration, delivery and timing checks require the physical board.
Software regression check: `sh tools/test_usb_midi.sh` (mocked USB transport).

The Daisy sends `OSC,W=<wave>,A=<fm amount>,R=<ratio>,F=<fine>,M=<mod wave>` in
its round-robin status stream. UART payloads are limited to 254 bytes plus a
newline. Immediate transport/status updates preserve the periodic stream position.
Without this frame, the OSC summary shows `EN ATTENTE` instead of invented values.
Coarse, Fine, Pulse, Sub-Osc and Hard still have empty DSP callbacks; the OSC
summary marks these selections `INACTIF`. This UI update does not implement them.

Build and validate from the repository root:

```bash
pio run -d esp32
cmake --build daisy/build -j4
sh tools/test_display_uart.sh
.venv/bin/python tools/preview_display.py
```

The preview tool requires `clang++` and the PlatformIO-installed fonts. It runs
the firmware drawing functions in a host rasterizer, checks text overlap and
circular-screen clipping for its test scenes, exercises first-level menus, and
checks OSC forwarding and status scheduling. PNGs are written to
`build/display-preview/`. This does not test hardware refresh timing, indexed
palette conversion, or physical touch/encoder operation.

### SD Patterns / Presets

Insert a FAT32 microSD in the ESP32's onboard TF reader. SDMMC uses 1-bit mode:
CLK GPIO2, CMD GPIO1, D0 GPIO3. The firmware never formats the card. It creates
`/DCO-ONE/PATTERNS/001.dco` through `128.dco` as needed. Files contain the sound
parameters, menu selections, all 32 pattern steps (OFF/NOTE/ARP/CHORD/FIXED,
degree and fixed transpose), and MIDI gate length. Version 3 is a 640-byte
little-endian record with CRC32; incompatible or corrupt files are rejected.
Only the current pattern and an in-flight transfer are held in RAM, not a
128-pattern bank.

PRESETS shows the current slot and live sound summary, including unsaved edits.
`PATTERNS SD`, `TRANSFERT SD`, `SD ABSENTE` and `ECHEC SD` report storage state.
`SD ABSENTE` also covers an unavailable return UART or an unmountable card.
Startup detection and retry run every five seconds outside numeric editing.

LOAD lists only valid saved slots. The MENU encoder selects a slot; pressing
it asynchronously loads the sound, opens the rhythmic wheel and starts playback
after validation. An empty bank displays `AUCUN PATTERN`. SAVE lists all 128 slots:
occupied slots use bold white text, empty slots use light gray. Confirmation
replaces the selected slot; HOME cancels selection before confirmation. A save
captures the sound at confirmation time. Failed reads preserve playback; failed
writes do not mark a slot occupied. UART transfers use 64-byte chunks with request
IDs, acknowledgements and a two-second timeout, without blocking the sequencer.

Both firmwares exchange the compact `PRST,` state frame over UART.
Its 32 hexadecimal digits encode occupancy for slots 1 through 128, four
slots per digit, least-significant bit first. Writes are verified in a `.tmp`
file before renaming, keeping the previous `.bak` as recovery if the main file
is unreadable. Avoid removing power or the card during a transfer: FAT metadata
is not guaranteed power-fail atomic. Reboot the ESP32 after swapping cards.

Existing QSPI preset slots are left untouched, but are not automatically migrated
or listed in the SD bank. The previous live-settings QSPI recovery remains active;
slot identity is not restored at boot. Explicit SAVE/LOAD now use only SD.

Focused checks from the repository root:

```bash
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined tools/test_sd_patterns.cpp -o /tmp/dco_test_sd_patterns
/tmp/dco_test_sd_patterns
sh tools/test_display_uart.sh
sh tools/test_sequencer_audio.sh
sh tools/test_usb_midi.sh
sh tools/test_remote_control.sh
node tools/test_remote_web.mjs
.venv/bin/python3 tools/preview_display.py
```

Hardware acceptance: save an unused slot, change a step and gate length, reload
and verify both are restored; reboot both boards and load the same slot again.
Then test with no SD: SAVE must show failure and the running pattern must survive
a failed LOAD. Host tests do not substitute for this physical SD/UART test.

The obsolete USB display bridges have been removed. Identify the ESP32 port with `pio device
list`, then run `pio run -d esp32 -t upload --upload-port <esp32_port>`.
Flash the Daisy in DFU mode, then press its RESET button. The display works
over UART; MIDI uses native USB. Verify OSC/Form, OSC/FM, LFO2 submenus,
VCF Key/Drive/Env, envelope editing, HOME and the sequencer on the actual display.

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
