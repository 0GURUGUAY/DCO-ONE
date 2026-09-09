# Flashing Instructions - Waveform Display Update

## Status
✅ **ESP32-S3** : Déjà flashé avec succès  
⏳ **Daisy Seed 3** : Prêt à flasher

## Next Step: Flash Daisy Seed 3

### Prerequisites
- Daisy Seed 3 connected via USB
- `dfu-util` installed (check with: `which dfu-util`)

### Flashing Steps

1. **Put Daisy in DFU (Device Firmware Update) Mode:**
   - Hold the **BOOT** button
   - Press the **RESET** button
   - Release both buttons
   - The Daisy should now be in DFU mode (no LED indicator, but DFU should be detected)

2. **Verify DFU Mode:**
   ```bash
   dfu-util -l
   ```
   You should see something like:
   ```
   Found DFU: [xxxx:xxxx] ver=yyyy, devnum=X, cfg=1, intf=0, path="X-X", alt=0, name="STM32H750XX"...
   ```

3. **Flash the Firmware:**
   ```bash
   dfu-util -a 0 -D /tmp/dco_one_phase1.bin -s 0x08000000
   ```
   Or use the Makefile command:
   ```bash
   cd /Users/maxpatissier/Downloads/DCO-ONE
   make upload-daisy
   ```

4. **Verify Upload:**
   - The terminal should show: "File downloaded successfully"
   - The Daisy will reset automatically

## What Changed

### Daisy Firmware
- Added periodic audio sample stream over USB (format: `WAVE,S=<samples>`)
- Samples sent every 100ms to ESP32 for waveform display

### ESP32 Firmware
- Added real-time waveform display in center of screen
- Waveform now takes up most of the central hub area
- Removed blue circle border
- Displays BPM at top, waveform in middle, Root/Scale/Play at bottom

### USB Bridge
- Added relay of `WAVE,` messages from Daisy to ESP32

## Testing

After flashing both boards:

1. **Start the USB Bridge:**
   ```bash
   cd /Users/maxpatissier/Downloads/DCO-ONE
   source .venv/bin/activate
   python3 usb_bridge.py /dev/cu.usbmodem3764336034331 /dev/cu.usbmodem101
   ```
   (Update port numbers as needed)

2. **Verify:**
   - ESP32 display should show waveform in center
   - Waveform should update in real-time as you turn the encoder
   - No blue circle should be visible

## Rollback

If needed, the previous firmware is available in:
- `backup/backup_20260906_202817_persistent_settings/daisy/build/dco_one_phase1`
- `backup/backup_20260906_202817_persistent_settings/esp32/.pio/build/...`

Use the same DFU procedure to revert.
