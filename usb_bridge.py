#!/usr/bin/env python3
"""
DCO-ONE Phase 1 — USB bridge (Mac)

During development both the Daisy Seed 3 and the ESP32-S3 are connected to the
Mac over USB.  This script relays the DCO1 audio frames produced by the Daisy
to the ESP32 so the AMOLED can draw the live waveform.

Usage:
    python3 usb_bridge.py
    python3 usb_bridge.py /dev/cu.usbmodemDAISY /dev/cu.usbmodemESP32

The target (ESP32) will later be replaced by a direct UART link between the
Daisy and the ESP32.
"""

import re
import sys
import time
import serial
import serial.tools.list_ports

BAUDRATE = 115200

MIDI_RE = re.compile(r"^MIDI,([0-9A-Fa-f]{2})(?:,([0-9A-Fa-f]{2}))?(?:,([0-9A-Fa-f]{2}))?$")

# Throttling for MIDI activity pulses forwarded to the ESP32 display.
# MIDI Clock (0xF8) and Active Sensing (0xFE) are ignored as activity.
_MIDI_ACTIVITY_INTERVAL = 0.03  # 30 ms
_midi_activity_last_out = 0.0


def send_midi_activity(esp, out_active=False):
    """Send a short MACT,OUT=1 pulse to the ESP32 so it flashes the green arrow."""
    global _midi_activity_last_out
    if out_active:
        now = time.time()
        if now - _midi_activity_last_out < _MIDI_ACTIVITY_INTERVAL:
            return
        _midi_activity_last_out = now
    frame = f"MACT,IN=0,OUT={1 if out_active else 0}\n"
    try:
        if hasattr(esp, "is_open") and esp.is_open:
            esp.write(frame.encode("ascii"))
    except (serial.SerialException, AttributeError, OSError):
        pass


def list_ports():
    print("Detected USB serial ports:")
    for p in serial.tools.list_ports.comports():
        print(f"  {p.device}: {p.description} [{p.hwid}]")
    print()


def auto_detect_daisy_port():
    """Try to find a Daisy Seed USB CDC port."""
    for p in serial.tools.list_ports.comports():
        # Daisy Seed 3 appears as an STM32 USB CDC device
        if "STM32" in p.hwid.upper() or "STMicroelectronics" in p.description:
            return p.device
        if "Daisy" in p.description:
            return p.device
    return None


def auto_detect_esp_port():
    """Try to find the ESP32-S3 USB CDC port."""
    for p in serial.tools.list_ports.comports():
        # ESP32-S3 native USB often shows as a USB JTAG/serial debug unit
        if "USB JTAG" in p.description or "CP210" in p.description:
            return p.device
        if "ESP32" in p.description:
            return p.device
    # Fallback: any cu.usbmodem port that is not the Daisy port
    return None


def open_port(path):
    try:
        return serial.Serial(path, BAUDRATE, timeout=0.05)
    except serial.SerialException as exc:
        print(f"ERROR: cannot open {path}: {exc}")
        sys.exit(1)


def main():
    daisy_path = sys.argv[1] if len(sys.argv) > 1 else auto_detect_daisy_port()
    esp_path = sys.argv[2] if len(sys.argv) > 2 else auto_detect_esp_port()

    if not daisy_path or not esp_path:
        print("Could not auto-detect both USB serial ports.\n")
        list_ports()
        print("Usage:")
        print("  python3 usb_bridge.py <daisy_port> <esp_port>")
        sys.exit(1)

    print(f"Opening Daisy port: {daisy_path}")
    print(f"Opening ESP32 port: {esp_path}")
    print("Bridge is running. Press Ctrl+C to stop.\n")

    # Byte buffers accumulated across read() calls so that a '\n' terminator
    # is never lost/misaligned by a short pyserial readline() timeout.
    daisy_buffer = bytearray()
    esp_buffer = bytearray()

    with open_port(daisy_path) as daisy, open_port(esp_path) as esp:
        while True:
            try:
                # Non-blocking: read(max(1, in_waiting)) used to block up to
                # the port's 50ms timeout whenever the buffer was momentarily
                # empty, stalling the whole relay loop (same bug fixed in
                # combined_bridge.py -- see its comment for the full story).
                chunk = daisy.read(daisy.in_waiting) if daisy.in_waiting else b""
                if chunk:
                    daisy_buffer.extend(chunk)

                while b"\n" in daisy_buffer:
                    line, _, rest = daisy_buffer.partition(b"\n")
                    daisy_buffer = bytearray(rest)
                    raw_line = line + b"\n"
                    text = raw_line.decode("utf-8", errors="replace").strip()
                    # Relay ALL control messages to ESP32
                    m = MIDI_RE.match(text)
                    if m:
                        status = int(m.group(1), 16)
                        if status not in (0xF8, 0xFE):
                            send_midi_activity(esp, out_active=True)
                        print(f"[DAISY LOG] {text}")
                        continue

                    is_control_msg = (
                        text.startswith("DCO1,") or text.startswith("NAV,")
                        or text.startswith("EDIT,") or text.startswith("STAT,")
                        or text.startswith("EN2,") or text.startswith("VCF,")
                        or text.startswith("LFO,") or text.startswith("LF2,")
                        or text.startswith("WAVE,") or text.startswith("POLY,")
                    )
                    if is_control_msg:
                        esp.write(raw_line)
                        print(f"[DAISY -> ESP] {text[:80]}...")
                    elif text:
                        print(f"[DAISY LOG] {text}")

                # Optional: print ESP debug output
                esp_chunk = esp.read(esp.in_waiting) if esp.in_waiting else b""
                if esp_chunk:
                    esp_buffer.extend(esp_chunk)
                while b"\n" in esp_buffer:
                    esp_line, _, esp_rest = esp_buffer.partition(b"\n")
                    esp_buffer = bytearray(esp_rest)
                    esp_text = esp_line.decode("utf-8", errors="replace").strip()
                    if esp_text:
                        print(f"[ESP LOG] {esp_text}")

                if not chunk and not esp_chunk:
                    # Nothing to do this pass: brief yield instead of busy-spinning.
                    time.sleep(0.0005)

            except KeyboardInterrupt:
                print("\nBridge stopped.")
                break
            except serial.SerialException as exc:
                # A board was unplugged / lost power: stop instead of
                # spinning forever re-printing the same OS error.
                print(f"\nDevice disconnected, stopping bridge: {exc}")
                break
            except Exception as exc:
                print(f"Bridge error: {exc}")
                time.sleep(0.1)


if __name__ == "__main__":
    main()
