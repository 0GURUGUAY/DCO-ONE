#!/usr/bin/env python3
"""
DCO-ONE — Combined USB bridge + MIDI bridge

Reads the single Daisy USB CDC port and:
  1. Relays control frames (NAV, STAT, EDIT, WAVE, POLY) to the ESP32 port.
  2. Parses MIDI frames and sends them to a virtual MIDI output port.

Usage:
    python3 combined_bridge.py /dev/cu.usbmodemDAISY /dev/cu.usbmodemESP32
    python3 combined_bridge.py /dev/cu.usbmodemDAISY /dev/cu.usbmodemESP32 "IAC Driver Bus 1"
"""

import re
import sys
import time
import serial
import serial.tools.list_ports
import rtmidi

BAUDRATE = 115200
TIMEOUT = 0.05

MIDI_RE = re.compile(r"^MIDI,([0-9A-Fa-f]{2})(?:,([0-9A-Fa-f]{2}))?(?:,([0-9A-Fa-f]{2}))?$")


def auto_detect_daisy_port():
    for p in serial.tools.list_ports.comports():
        if "STM32" in p.hwid.upper() or "STMicroelectronics" in p.description:
            return p.device
        if "Daisy" in p.description:
            return p.device
    return None


def auto_detect_esp_port():
    for p in serial.tools.list_ports.comports():
        if "USB JTAG" in p.description or "CP210" in p.description:
            return p.device
        if "ESP32" in p.description:
            return p.device
    return None


def open_serial(path):
    try:
        return serial.Serial(path, BAUDRATE, timeout=TIMEOUT)
    except serial.SerialException as exc:
        print(f"ERROR: cannot open {path}: {exc}")
        sys.exit(1)


def open_midi_output(requested_name=None):
    midi_out = rtmidi.MidiOut()
    available = midi_out.get_ports()

    if not available:
        print("No MIDI output ports found.")
        sys.exit(1)

    if requested_name:
        try:
            idx = int(requested_name)
            port_index = idx
        except ValueError:
            matches = [i for i, n in enumerate(available) if requested_name in n]
            if not matches:
                print(f"MIDI port '{requested_name}' not found.")
                port_index = None
            else:
                port_index = matches[0]
    else:
        matches = [i for i, n in enumerate(available) if "IAC" in n]
        port_index = matches[0] if matches else None

    if port_index is None:
        print("Available MIDI output ports:")
        for i, name in enumerate(available):
            print(f"  [{i}] {name}")
        sys.exit(1)

    port_name = available[port_index]
    midi_out.open_port(port_index)
    print(f"MIDI output opened: [{port_index}] {port_name}")
    return midi_out


def main():
    daisy_path = sys.argv[1] if len(sys.argv) > 1 else auto_detect_daisy_port()
    esp_path = sys.argv[2] if len(sys.argv) > 2 else auto_detect_esp_port()
    midi_name = sys.argv[3] if len(sys.argv) > 3 else None

    if not daisy_path or not esp_path:
        print("Usage: python3 combined_bridge.py <daisy_port> <esp_port> [midi_port]")
        sys.exit(1)

    midi_out = open_midi_output(midi_name)

    print(f"Opening Daisy port: {daisy_path}")
    print(f"Opening ESP32 port: {esp_path}")
    print("Combined bridge running. Press Ctrl+C to stop.\n")

    daisy_buffer = bytearray()
    esp_buffer = bytearray()

    with open_serial(daisy_path) as daisy, open_serial(esp_path) as esp:
        while True:
            try:
                # Non-blocking: read(max(1, in_waiting)) used to block up to
                # TIMEOUT (50ms) waiting for a byte whenever the buffer was
                # momentarily empty, stalling the whole loop -- Daisy MIDI
                # clock bytes queued up during that stall and got flushed in
                # bursts once it unblocked, which is what made Ableton's BPM
                # follower see the clock as wildly irregular.
                chunk = daisy.read(daisy.in_waiting) if daisy.in_waiting else b""
                if chunk:
                    daisy_buffer.extend(chunk)

                while b"\n" in daisy_buffer:
                    line, _, rest = daisy_buffer.partition(b"\n")
                    daisy_buffer = bytearray(rest)
                    text = line.decode("utf-8", errors="replace").strip()

                    m = MIDI_RE.match(text)
                    if m:
                        status = int(m.group(1), 16)
                        data0 = int(m.group(2), 16) if m.group(2) is not None else None
                        data1 = int(m.group(3), 16) if m.group(3) is not None else None

                        msg = [status]
                        if data0 is not None:
                            msg.append(data0)
                        if data1 is not None:
                            msg.append(data1)

                        midi_out.send_message(msg)
                        # Skip printing for MIDI Clock (0xF8): it fires up to
                        # ~50x/sec while playing, and synchronous console I/O
                        # on every tick injects jitter directly into the
                        # timing-critical clock path, which is what Ableton's
                        # tempo follower sees as a constantly-changing BPM.
                        if status != 0xF8:
                            print(f"-> MIDI {msg}")
                        continue

                    is_control_msg = (
                        text.startswith("DCO1,")
                        or text.startswith("NAV,")
                        or text.startswith("EDIT,")
                        or text.startswith("STAT,")
                        or text.startswith("VCF,")
                        or text.startswith("WAVE,")
                        or text.startswith("POLY,")
                    )
                    if is_control_msg:
                        esp.write(line + b"\n")
                        # High-frequency frames (STAT heartbeat, WAVE) are no
                        # longer printed synchronously: console I/O was adding
                        # variable latency to the relay loop and could stall
                        # the Daisy->ESP link on every redraw. Navigation and
                        # user-action frames are still logged for diagnostics.
                        if text.startswith("NAV,") or text.startswith("EDIT,") or text.startswith("POLY,"):
                            print(f"[DAISY -> ESP] {text[:80]}...")
                    elif text:
                        print(f"[DAISY LOG] {text}")

                # Same non-blocking pattern: an idle ESP32 port must never
                # stall the loop and delay the next Daisy/MIDI read.
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
                    # Nothing to do this pass: brief yield so the loop doesn't
                    # spin at 100% CPU, short enough to stay well under the
                    # ~20ms MIDI clock period so it can't add audible jitter.
                    time.sleep(0.0005)

            except KeyboardInterrupt:
                print("\nBridge stopped.")
                break
            except serial.SerialException as exc:
                print(f"\nSerial error: {exc}")
                break


if __name__ == "__main__":
    main()
