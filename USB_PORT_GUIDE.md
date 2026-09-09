# USB Port Identification Guide

## Quick Port Check

### macOS
To see all USB serial ports:
```bash
# List all USB serial ports
ls /dev/cu.usbserial-*
ls /dev/cu.usbmodem*

# Or use this to see more details
ls -lha /dev/tty.* | grep -E "usbserial|usbmodem"

# System profiler (detailed info)
system_profiler SPUSBDataType
```

### Linux
```bash
# List all serial ports
ls /dev/ttyUSB*
ls /dev/ttyACM*

# Detailed information
dmesg | tail
```

---

## Identifying Your Boards

### Daisy Seed 3 USB Characteristics
- **Vendor ID**: 0x0483 (STMicroelectronics)
- **Product ID**: Depends on bootloader state
- **Port Pattern**: `/dev/cu.usbserial-*` or `/dev/cu.usbmodem*`
- **Speed**: 115200 baud (default)
- **Signature**: "Daisy" in `system_profiler` output

**Detection Command:**
```bash
system_profiler SPUSBDataType | grep -A5 "Daisy"
```

### ESP32-S3 Waveshare AMOLED USB Characteristics
- **Vendor ID**: 0x10C4 (Silicon Labs USB-to-UART)
- **Chip**: CP2102N USB bridge
- **Port Pattern**: `/dev/cu.usbserial-*`
- **Speed**: 115200 or 921600 baud
- **Signature**: "CP2102" or "Silicon Labs" in system_profiler

**Detection Command:**
```bash
system_profiler SPUSBDataType | grep -A5 "Silicon Labs"
```

---

## Port Assignment

When both boards are connected, you'll see two `/dev/cu.usbserial-*` ports.

### Method to Distinguish

**Option 1: Check System Profiler**
```bash
system_profiler SPUSBDataType | grep -E "Daisy|Silicon Labs|CP2102" -A2
```

**Option 2: Test with Serial Monitor**

Daisy (typically shows Daisy firmware output):
```bash
screen /dev/cu.usbserial-<port> 115200
# Should show Daisy-related messages or Audio initialization
```

ESP32-S3 (typically shows PlatformIO/Arduino output):
```bash
screen /dev/cu.usbserial-<port> 115200
# Should show "Setup complete - ready to receive audio data"
```

**Option 3: Disconnect one board at a time**
1. Unplug ESP32-S3
2. Run: `ls /dev/cu.usbserial-*`
3. Note the remaining port - this is Daisy
4. Plug in ESP32-S3
5. Run: `ls /dev/cu.usbserial-*`
6. The new port is ESP32-S3

---

## Update Configuration Files

### For Daisy (if needed in future)
Edit `Makefile`:
```makefile
DAISY_PORT = /dev/cu.usbserial-XXXXX
```

### For ESP32-S3 (PlatformIO)
Edit `esp32/platformio.ini`:
```ini
monitor_port = /dev/cu.usbserial-XXXXX
upload_port = /dev/cu.usbserial-XXXXX
```

---

## Testing Port Connection

### Simple Echo Test

**For Daisy:**
```bash
screen /dev/cu.usbserial-XXXXX 115200
# Type some characters - if Daisy is running, you might see debug output
# Press Ctrl+A, then Ctrl+\ to exit screen
```

**For ESP32-S3:**
```bash
platformio device monitor --port /dev/cu.usbserial-XXXXX
# You should see the Arduino setup() messages
# Ctrl+C to exit
```

---

## Troubleshooting Port Issues

### "No such file or directory"
```bash
# Ports not visible? Try:
ls /dev/cu.*

# If no ports show up, check:
1. USB cable is plugged in properly
2. Board is powered on
3. Check System Report → USB for unknown devices
```

### "Permission denied" when using screen
```bash
# Grant permissions
chmod 666 /dev/cu.usbserial-*
```

### "Device busy" error
```bash
# Another program is using the port
# Close serial monitor/screen/other terminal programs

# Or use lsof to find what's using it:
lsof /dev/cu.usbserial-*
```

### USB device "Unknown" in System Report
- Try different USB cable (some cables are charge-only)
- Try different USB port on Mac
- For Daisy: Hold BOOT, press RESET to force DFU mode
- For ESP32-S3: Press RESET to restart

---

## Permanent Port Identification

You can create udev rules (Linux) or use permanent identifiers:

### macOS Workaround
Use the full device path including serial number:
```bash
# Get the detailed path
system_profiler SPUSBDataType | grep -E "Location|Daisy" -A1 -B1

# The port includes location info:
/dev/cu.usbserial-<VENDOR>_<SERIAL>_<PORT>
```

### Update platformio.ini with serial number
```ini
; More specific port identification
upload_port = /dev/cu.usbserial-*
monitor_port = /dev/cu.usbserial-*
```

---

## Quick Reference

| Device | Vendor | Identifier | Baud |
|--------|--------|------------|------|
| Daisy Seed 3 | STMicroelectronics | `usbmodem*` or `usbserial-*` | 115200 |
| ESP32-S3 | Silicon Labs (CP2102N) | `usbserial-*` | 115200 |

---

## Next Steps

1. Connect both boards
2. Run: `ls /dev/cu.usbserial-*`
3. Note both port numbers
4. Update configuration files if needed
5. Run: `make check-hardware`
6. Proceed with Phase 1 testing

---

**For more information:**
- Daisy: https://docs.daisy.audio/
- ESP32-S3: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75
- PlatformIO: https://docs.platformio.org/en/latest/core/quickstart.html
