# DCO-ONE Phase 1 - Verification Tasks

## 🎯 Immediate Priorities

### Task 1: Verify Hardware Connections
**Status**: READY TO TEST
**Location**: Mac Mini A4 Pro USB ports

#### Daisy Seed 3 Connection
- [ ] Connect Daisy Seed 3 via USB to Mac Mini A4 Pro
- [ ] Verify device appears: `ls /dev/cu.usbserial-*`
- [ ] Note the port name (e.g., `/dev/cu.usbserial-14230`)
- [ ] Update Makefile with correct port if needed

#### ESP32-S3 Waveshare AMOLED Connection
- [ ] Connect ESP32-S3 via USB to Mac Mini A4 Pro (different USB port than Daisy)
- [ ] Verify device appears: `ls /dev/cu.usbserial-*`
- [ ] Note the port name
- [ ] Update platformio.ini with correct port if needed

#### Run Diagnostic
```bash
bash check_hardware.sh
```

Expected output:
```
✓ Found Daisy Seed 3 connected on: /dev/cu.usbserial-XXXXX
✓ Found potential ESP32-S3 on: /dev/cu.usbserial-YYYYY
✓ Daisy toolchain detected
✓ PlatformIO found
✓ ARM compiler found
```

---

### Task 2: Build Daisy Firmware
**Status**: READY TO BUILD
**Requirements**: 
- Daisy toolchain installed
- ARM GCC compiler in PATH

```bash
make build-daisy
```

**Expected output:**
- No compilation errors
- Binary generated in `daisy/build/`
- Message: "✓ Daisy build complete"

**If build fails:**
1. Check CMakeLists.txt paths
2. Verify libDaisy location
3. Ensure DaisySP is available

---

### Task 3: Build ESP32-S3 Firmware
**Status**: READY TO BUILD
**Requirements**: 
- PlatformIO installed
- ESP32-S3 support available

```bash
make build-esp32
```

**Expected output:**
- No compilation errors
- Binary generated in `esp32/.pio/`
- Message: "✓ ESP32-S3 build complete"

**If build fails:**
1. Verify PlatformIO: `platformio --version`
2. Update board definitions: `platformio platform update espressif32`

---

### Task 4: Upload Daisy Firmware
**Status**: READY FOR UPLOAD
**Prerequisites**: 
- Build successful (Task 2)
- Daisy connected via USB

```bash
# Put Daisy in DFU mode:
# 1. Hold BOOT button
# 2. Press RESET button
# 3. Release both

make upload-daisy
```

**Expected output:**
- Firmware uploads to device
- Device reboots
- Message: "✓ Daisy upload complete"

**Verification:**
- Daisy LED should blink or indicate operation
- Audio outputs should be live (440 Hz sine wave)

---

### Task 5: Upload ESP32-S3 Firmware
**Status**: READY FOR UPLOAD
**Prerequisites**: 
- Build successful (Task 3)
- ESP32-S3 connected via USB

```bash
make upload-esp32
```

**Expected output:**
- Firmware uploads to device
- Device reboots
- Message: "✓ ESP32-S3 upload complete"

**Verification:**
- Display backlight powers on
- No error messages on serial monitor

---

### Task 6: Verify Audio Output from Daisy
**Status**: READY TO TEST
**Prerequisites**: 
- Daisy firmware uploaded (Task 4)
- Audio cable connected to Daisy outputs

**Test Method A: Listen to Audio**
1. Connect stereo audio cable to Daisy audio outputs (3.5mm jack)
2. Connect to headphones or amplifier
3. Listen for clean 440 Hz tone (A note)

**Test Method B: Oscilloscope Verification**
1. Connect oscilloscope CH1 to Daisy Audio Out 1 (mono test)
2. Set vertical scale to 1V/div
3. Set horizontal scale to 5ms/div (or auto)
4. Measure:
   - Frequency: Should be 440 Hz ± 1%
   - Amplitude: Should be ~1.5V peak (0.3 amplitude at line level)
   - Shape: Clean sine wave, no distortion

**Test Method C: Serial Monitor**
```bash
make monitor-daisy
```

Expected output:
```
=== DCO-ONE Phase 1: Audio Generation ===
Audio initialized - 48 kHz, 64-sample blocks
Oscillator: 440 Hz sine wave
[Periodic status messages]
```

---

### Task 7: Verify ESP32-S3 Display Output
**Status**: READY TO TEST
**Prerequisites**: 
- ESP32-S3 firmware uploaded (Task 5)
- Display visible

**Visual Checks:**
- [ ] Display backlight is on (not black)
- [ ] No visible errors or corruption on screen
- [ ] Display is responsive (no freezing)

**Serial Monitor:**
```bash
make monitor-esp32
```

Expected output:
```
Initializing ESP32-S3 Oscilloscope Display
Display initialized
Setup complete - ready to receive audio data
```

---

## 📋 Verification Checklist

Use this checklist to track Phase 1 completion:

```
HARDWARE:
 [ ] Daisy Seed 3 connected and recognized
 [ ] ESP32-S3 connected and recognized
 [ ] Both boards have stable USB connections
 [ ] No loose cables or connectors

SOFTWARE:
 [ ] Daisy firmware compiles without errors
 [ ] ESP32-S3 firmware compiles without errors
 [ ] Daisy firmware uploads successfully
 [ ] ESP32-S3 firmware uploads successfully

AUDIO VERIFICATION:
 [ ] Daisy outputs 440 Hz sine wave
 [ ] Audio frequency is stable
 [ ] Audio amplitude is correct (~1.5V peak)
 [ ] No clipping or distortion

DISPLAY VERIFICATION:
 [ ] ESP32-S3 display powers on
 [ ] Display backlight is visible
 [ ] No initialization errors on serial monitor
 [ ] Serial communication is working

PHASE 1 COMPLETE:
 [ ] All checks pass
 [ ] Audio generation stable for >1 minute
 [ ] Display remains stable
 [ ] No error messages or warnings
```

---

## 🚀 Next Steps After Phase 1

Once Phase 1 is verified, proceed to **Phase 2**:

1. **Audio Input/Output**: Implement input detection and processing
2. **UART Communication**: Connect Daisy and ESP32-S3 via UART
3. **Real-time Waveform Display**: Display actual audio waveform on ESP32-S3
4. **Display Integration**: Full oscilloscope visualization

See [PROJECT_ARCHITECTURE_V2.md](PROJECT_ARCHITECTURE_V2.md) section "17. DEVELOPMENT STRATEGY" for the complete phase roadmap.

---

## 📞 Support

### Common Issues

**"Daisy board not found"**
- Ensure USB cable is connected to correct port
- Try different USB port on Mac
- Check USB cable quality

**"ESP32-S3 not responding"**
- Verify USB cable connection
- Check if device driver is installed
- Hold boot button and press reset to force recovery mode

**"Build fails - libDaisy not found"**
- Verify DAISY_ROOT path in CMakeLists.txt
- Ensure libDaisy is in the correct location
- Run `arm-none-eabi-gcc --version` to verify toolchain

**"No audio from Daisy"**
- Check audio cable connections
- Verify volume level is not muted
- Check amplitude setting (currently 30%)

**"Display is black"**
- Verify display power supply (3.3V)
- Check SPI connection pins
- Verify display reset pin

---

## 📝 Notes

- **Backup**: All work is backed up in `backup/` directory
- **Ports**: USB ports may change between sessions; update .ini files if needed
- **DFU Mode**: Daisy requires DFU mode for uploads; keep USB cable connected
- **Power**: Both boards are USB-powered; ensure sufficient power supply

---

**Phase 1 Status**: READY FOR IMPLEMENTATION  
**Last Updated**: 2026-09-05  
**Version**: 1.0
