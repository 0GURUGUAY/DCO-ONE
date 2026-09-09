# Phase 1 - Hardware Testing Guide

## 🎯 Objectives
1. Verify Daisy Seed 3 connection and 440 Hz sine wave generation
2. Verify ESP32-S3 connection and display initialization
3. Establish UART communication between boards

## ✅ Status: Firmware Compiled
- ✓ Daisy Seed 3 firmware: `daisy/build/dco_one_phase1` (427K)
- ✓ ESP32-S3 firmware: `esp32/.pio/build/esp32-s3-amoled/firmware.bin` (327K)

---

## 📝 Step 1: Hardware Connection

### Daisy Seed 3
1. Connect USB-C cable to Daisy Seed 3
2. Connect other end to Mac Mini A4 Pro USB port (port A)
3. Verify device appears in `/dev/cu.usbserial-*`

### ESP32-S3
1. Connect USB-C cable to ESP32-S3 devkit
2. Connect other end to Mac Mini A4 Pro USB port (port B)
3. Verify device appears in `/dev/cu.usbserial-*` or `/dev/cu.usbmodem*`

### Verification
```bash
# Check connected USB devices
ls /dev/cu.usbserial-* /dev/cu.usbmodem* 2>/dev/null

# Or run the diagnostic
bash check_hardware.sh
```

Expected output:
```
✅ Daisy Seed 3: /dev/cu.usbserial-... [Bootloader Ready]
✅ ESP32-S3: /dev/cu.usbmodem... [Ready to Upload]
```

---

## 📥 Step 2: Upload Firmware

### 2a. Daisy Seed 3 (Manual DFU Mode)
1. **Hold BOOT button** on Daisy Seed 3
2. **Press RESET button** while holding BOOT
3. **Release BOOT button** (keep RESET pressed for 1 second)
4. Device enters DFU mode (no LED activity)
5. Run upload command:
```bash
cd /Users/maxpatissier/Downloads/DCO-ONE
make upload-daisy
```
6. Wait for completion (~5 seconds)
7. Firmware uploads via DFU, then Daisy resets automatically

### 2b. ESP32-S3 (Automatic)
```bash
cd /Users/maxpatissier/Downloads/DCO-ONE
make upload-esp32
```
- Auto-detects device in bootloader mode
- If stuck, hold BOOT button, then press RESET
- Upload should complete in ~10 seconds

### Combined Upload (Both boards)
```bash
# Upload both after connecting and setting Daisy to DFU mode
make upload-daisy
make upload-esp32
```

---

## 🔬 Step 3: Verification & Monitoring

### Terminal 1: Monitor Daisy Seed 3
```bash
cd /Users/maxpatissier/Downloads/DCO-ONE
make monitor-daisy
```
Expected output:
```
Daisy Seed 3 - Phase 1 Audio Test
Oscillator initialized: 440 Hz, 48 kHz
Audio output: ACTIVE
...
```

### Terminal 2: Monitor ESP32-S3
```bash
cd /Users/maxpatissier/Downloads/DCO-ONE
make monitor-esp32
```
Expected output:
```
ESP32-S3 AMOLED Display - Phase 1
Display initialized: 466x466 pixels
Audio buffer: Ready
UART reception: Waiting...
Oscilloscope: Drawing...
```

### Hardware Verification Checklist
- [ ] Daisy Seed 3: Blue LED on after upload (idle state)
- [ ] Daisy Seed 3: Audio jack shows 440 Hz sine wave (oscilloscope or headphones test)
- [ ] Daisy Seed 3: Serial output shows initialization messages
- [ ] ESP32-S3: AMOLED display lights up (should show oscilloscope preview)
- [ ] ESP32-S3: Serial output shows successful initialization
- [ ] Both boards: No error messages in serial monitors

---

## 🐛 Troubleshooting

### Problem: DFU device not recognized
```
dfu-util: No DFU capable USB device available
```
**Solution:**
1. Verify USB cable connection
2. Re-enter DFU mode: Hold BOOT → Press RESET → Release BOOT
3. Run: `dfu-util --list` to verify device detection
4. Try different USB port

### Problem: ESP32 upload stuck
```
Connecting........_____
```
**Solution:**
1. Press BOOT button on ESP32-S3
2. While holding, press RESET
3. Release BOOT
4. Run upload command again
5. If timeout, try: `pio device monitor --port /dev/cu.usbmodem... -b 115200`

### Problem: No audio output from Daisy
**Solution:**
1. Verify firmware uploaded correctly (check serial for init messages)
2. Check oscillator initialization: `osc.Init(hw.AudioSampleRate())`
3. Test with headphones or oscilloscope probe on audio jack
4. Verify amplitude is not too low (should be ~0.3V peak for 30% amplitude)

### Problem: ESP32 display not showing waveform
**Solution:**
1. Verify AMOLED display powers on (LED light visible)
2. Check serial output for display initialization errors
3. Ensure Daisy is transmitting audio data via UART
4. Verify UART connections (RX/TX pins configured correctly)

---

## 📊 Expected Behavior

### Daisy Seed 3
- Generates continuous 440 Hz sine wave at 48 kHz sample rate
- Amplitude: 30% to prevent clipping
- Output on both L/R channels (stereo)
- Waveform: Clean sine wave visible on oscilloscope

### ESP32-S3
- AMOLED display shows real-time oscilloscope waveform
- Updates at ~60 FPS (16.6 ms refresh)
- Circular display with 466×466 pixel resolution
- Smooth, anti-aliased waveform visualization

### UART Communication (Phase 2 preparation)
- Daisy transmits audio frames: 64 samples @ 48 kHz = every 1.33 ms
- ESP32 receives and buffers for real-time display
- Zero-copy audio pipeline for minimal latency

---

## 🚀 Next Phase: Phase 2
Once Phase 1 verification is complete:
1. Implement real-time UART audio transmission (Daisy → ESP32)
2. Replace oscilloscope preview with live audio data
3. Add frequency/amplitude controls via buttons
4. Implement audio triggering (sweep, hold, trigger modes)

---

## 📞 Support References

| Component | Port | Settings | Notes |
|-----------|------|----------|-------|
| Daisy Seed 3 | `/dev/cu.usbserial-*` | DFU bootloader mode | Hold BOOT + RESET |
| ESP32-S3 | `/dev/cu.usbmodem*` | 115200 baud | Auto-detect |
| Oscilloscope | Audio jack | 48 kHz, 20 Hz - 20 kHz | Test sine wave |
| Display | I2C/SPI | 1.75" AMOLED 466×466 | Circular screen |

---

Generated: 2024
Framework: Daisy Seed 3 + ESP32-S3 Audio Instrument (DCO-ONE)
