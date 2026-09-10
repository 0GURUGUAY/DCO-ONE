#!/usr/bin/env python3
"""
DCO-ONE — Combined USB bridge + MIDI bridge

Reads the single Daisy USB CDC port and:
  1. Relays control frames (NAV, STAT, EDIT, WAVE, POLY) to the ESP32 port.
  2. Parses MIDI OUT frames and sends them to a virtual MIDI output port.
  3. Receives MIDI IN from a real MIDI input port and forwards it to the Daisy
     as text frames on the same CDC link.

Usage:
    python3 combined_bridge.py
    python3 combined_bridge.py [daisy_port] [esp_port] [midi_out_port] [midi_in_port]
    python3 combined_bridge.py /dev/cu.usbmodemDAISY /dev/cu.usbmodemESP32 \
        "IAC Driver Bus 1" "IAC Driver Bus 2"

Without arguments the script scans connected serial ports and auto-selects the
Daisy and ESP32 ports. Positional arguments override auto-detection.
"""

import re
import sys
import time
import threading
import serial
import serial.tools.list_ports
import rtmidi

BAUDRATE = 115200
TIMEOUT = 0.05

MIDI_RE = re.compile(r"^MIDI,([0-9A-Fa-f]{2})(?:,([0-9A-Fa-f]{2}))?(?:,([0-9A-Fa-f]{2}))?$")

# Throttling for MIDI activity pulses forwarded to the ESP32 display.
# MIDI Clock (0xF8) and Active Sensing (0xFE) are ignored as activity.
_MIDI_ACTIVITY_INTERVAL = 0.03  # 30 ms
_midi_activity_last = {"in": 0.0, "out": 0.0}


def send_midi_activity(esp, esp_lock, out_active=False, in_active=False):
    """Send a short MACT pulse to the ESP32 so it can flash the MIDI arrows."""
    global _midi_activity_last
    direction = "in" if in_active else "out" if out_active else None
    if direction:
        now = time.time()
        if now - _midi_activity_last[direction] < _MIDI_ACTIVITY_INTERVAL:
            return
        _midi_activity_last[direction] = now
    if esp is None:
        return
    frame = f"MACT,IN={1 if in_active else 0},OUT={1 if out_active else 0}\n"
    print(f"[MIDI ACT -> ESP] {frame.strip()}")
    with esp_lock:
        try:
            if hasattr(esp, "is_open") and esp.is_open:
                esp.write(frame.encode("ascii"))
        except (serial.SerialException, AttributeError, OSError):
            pass


def _list_serial_ports():
    """Return the current list of serial ports, sorted by device path."""
    ports = list(serial.tools.list_ports.comports())
    ports.sort(key=lambda p: p.device)
    return ports


def auto_detect_daisy_port():
    """Find the Daisy Seed USB-CDC port among currently connected serial ports."""
    for p in _list_serial_ports():
        desc = (p.description or "").upper()
        hwid = (p.hwid or "").upper()
        manufacturer = (getattr(p, "manufacturer", None) or "").upper()
        if any(k in desc or k in hwid or k in manufacturer for k in
               ("DAISY", "STM32", "STMICROELECTRONICS", "ST_DFU", "STM32 BOOTLOADER")):
            return p.device
    return None


def auto_detect_esp_port():
    """Find the ESP32 USB-to-serial port among currently connected serial ports."""
    for p in _list_serial_ports():
        desc = (p.description or "").upper()
        hwid = (p.hwid or "").upper()
        manufacturer = (getattr(p, "manufacturer", None) or "").upper()
        if any(k in desc or k in hwid or k in manufacturer for k in
               ("ESP32", "CP210", "CP2102", "CH340", "CH9102", "USB JTAG",
                "SERIAL DEBUG UNIT", "SILICON LABS", "WCH.CN", "QINHENG")):
            return p.device
    return None


def scan_ports():
    """Scan and print every serial port currently visible to the system."""
    print("Scanning serial ports...")
    ports = _list_serial_ports()
    if not ports:
        print("  (no serial ports found)")
        return None, None

    daisy = auto_detect_daisy_port()
    esp = auto_detect_esp_port()

    for p in ports:
        marker = ""
        if p.device == daisy:
            marker += " [DAISY]"
        if p.device == esp:
            marker += " [ESP32]"
        print(f"  {p.device}: {p.description!r} | {p.hwid!r}{marker}")

    return daisy, esp


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


def open_midi_input(requested_name=None):
    midi_in = rtmidi.MidiIn()
    available = midi_in.get_ports()

    if not available:
        print("No MIDI input ports found.")
        return None

    if requested_name:
        try:
            idx = int(requested_name)
            port_index = idx
        except ValueError:
            matches = [i for i, n in enumerate(available) if requested_name in n]
            if not matches:
                print(f"MIDI input port '{requested_name}' not found.")
                port_index = None
            else:
                port_index = matches[0]
    else:
        port_index = 0

    if port_index is None or port_index >= len(available):
        print("Available MIDI input ports:")
        for i, name in enumerate(available):
            print(f"  [{i}] {name}")
        return None

    port_name = available[port_index]
    midi_in.open_port(port_index)
    print(f"MIDI input opened: [{port_index}] {port_name}")
    return midi_in


def main():
    # Optional positional overrides: [daisy_port] [esp_port] [midi_out_port] [midi_in_port]
    daisy_path = sys.argv[1] if len(sys.argv) > 1 else None
    esp_path = sys.argv[2] if len(sys.argv) > 2 else None
    midi_out_name = sys.argv[3] if len(sys.argv) > 3 else None
    midi_in_name = sys.argv[4] if len(sys.argv) > 4 else None

    # At every invocation, scan connected serial ports. CLI args override
    # auto-detection so the bridge can still be forced to a specific port.
    detected_daisy, detected_esp = scan_ports()

    daisy_path = daisy_path or detected_daisy
    esp_path = esp_path or detected_esp

    if not daisy_path:
        print("\nUsage: python3 combined_bridge.py [daisy_port] [esp_port] [midi_out_port] [midi_in_port]")
        print("  Port(s) not auto-detected can be supplied manually as positional arguments.")
        print("  Pass '-' as the ESP32 port to disable ESP32 forwarding (Daisy + MIDI only).")
        sys.exit(1)

    midi_out = open_midi_output(midi_out_name)
    midi_in = open_midi_input(midi_in_name)

    print(f"\nOpening Daisy port: {daisy_path}")
    if esp_path and esp_path != "-":
        print(f"Opening ESP32 port: {esp_path}")
    else:
        print("ESP32 forwarding disabled.")
    print("Combined bridge running. Press Ctrl+C to stop.\n")

    daisy_buffer = bytearray()
    esp_buffer = bytearray()

    with open_serial(daisy_path) as daisy, \
         (open_serial(esp_path) if esp_path and esp_path != "-" else open("/dev/null", "rb")) as esp:
        daisy_lock = threading.Lock()
        esp_lock = threading.Lock()

        def forward_midi_in(msg, _time_stamp):
            if not msg or not msg[0]:
                return
            data = msg[0]
            text = f"MIDI,{data[0]:02X}"
            for b in data[1:]:
                text += f",{b:02X}"
            text += "\n"
            print(f"[MIDI IN -> Daisy] {text.strip()}")
            with daisy_lock:
                try:
                    if daisy.is_open:
                        daisy.write(text.encode("ascii"))
                except serial.SerialException as exc:
                    print(f"[MIDI IN -> Daisy] serial error: {exc}")
            # Flash the red MIDI IN arrow on the ESP32 play screen.
            if data and data[0] not in (0xF8, 0xFE):
                send_midi_activity(esp, esp_lock, in_active=True)

        if midi_in:
            midi_in.set_callback(forward_midi_in)

        while True:
            try:
                # Non-blocking: read(max(1, in_waiting)) used to block up to
                # TIMEOUT (50ms) waiting for a byte whenever the buffer was
                # momentarily empty, stalling the whole loop -- Daisy MIDI
                # clock bytes queued up during that stall and got flushed in
                # bursts once it unblocked, which is what made Ableton's BPM
                # follower see the clock as wildly irregular.
                with daisy_lock:
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
                        # Flash the green MIDI OUT arrow on the ESP32 play
                        # screen for any non-clock / non-active-sensing msg.
                        if status not in (0xF8, 0xFE):
                            send_midi_activity(esp, esp_lock, out_active=True)
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
                        or text.startswith("EN2,")
                        or text.startswith("VCF,")
                        or text.startswith("LFO,")
                        or text.startswith("LF2,")
                        or text.startswith("MAT,")
                        or text.startswith("WAVE,")
                        or text.startswith("POLY,")
                    )
                    if is_control_msg and esp_path and esp_path != "-":
                        try:
                            with esp_lock:
                                esp.write(line + b"\n")
                        except (serial.SerialException, AttributeError, OSError):
                            pass
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
                if esp_path and esp_path != "-":
                    esp_chunk = esp.read(esp.in_waiting) if esp.in_waiting else b""
                    if esp_chunk:
                        esp_buffer.extend(esp_chunk)
                    while b"\n" in esp_buffer:
                        esp_line, _, esp_rest = esp_buffer.partition(b"\n")
                        esp_buffer = bytearray(esp_rest)
                        esp_text = esp_line.decode("utf-8", errors="replace").strip()
                        if esp_text:
                            print(f"[ESP LOG] {esp_text}")
                else:
                    esp_chunk = b""

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
