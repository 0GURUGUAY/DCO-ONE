#!/usr/bin/env python3
"""
DCO-ONE — Daisy MIDI bridge to Ableton Live 12 (via macOS IAC bus)

Reads text-form MIDI frames emitted by the Daisy on its USB CDC port:
    MIDI,<status>[,<data0>[,<data1>]]
and forwards them as real MIDI messages to a selectable output port.

On macOS, create an IAC bus in:
    Applications -> Utilitaires -> Configuration Audio et MIDI -> Fenêtre -> Afficher la fenêtre MIDI
    Double-clique "Périphériques MIDI IAC" -> coche "Périphérique en ligne"
Ableton Live verra ce bus comme un INPUT MIDI standard.

Usage:
    python3 midi_bridge.py /dev/cu.usbmodemDAISY
    python3 midi_bridge.py /dev/cu.usbmodemDAISY "IAC Driver Bus 1"
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


def list_available_ports():
    print("Available MIDI output ports:")
    midi_out = rtmidi.MidiOut()
    ports = midi_out.get_ports()
    if not ports:
        print("  (none)")
    for i, name in enumerate(ports):
        print(f"  [{i}] {name}")
    return ports


def main():
    daisy_path = sys.argv[1] if len(sys.argv) > 1 else auto_detect_daisy_port()
    if not daisy_path:
        print("Could not auto-detect Daisy port.")
        print("Usage: python3 midi_bridge.py <daisy_port> [midi_out_port_name]")
        sys.exit(1)

    requested_name = sys.argv[2] if len(sys.argv) > 2 else None
    midi_out = rtmidi.MidiOut()
    available = midi_out.get_ports()

    if requested_name:
        try:
            idx = int(requested_name)
            port_index = idx
        except ValueError:
            matches = [i for i, n in enumerate(available) if requested_name in n]
            if not matches:
                print(f"MIDI port '{requested_name}' not found.")
                list_available_ports()
                sys.exit(1)
            port_index = matches[0]
    else:
        # Prefer an IAC bus by default
        matches = [i for i, n in enumerate(available) if "IAC" in n]
        if not matches:
            print("No IAC bus found. Available ports:")
            list_available_ports()
            sys.exit(1)
        port_index = matches[0]

    port_name = available[port_index]
    midi_out.open_port(port_index)
    print(f"MIDI output opened: [{port_index}] {port_name}")

    print(f"Opening Daisy port: {daisy_path}")
    print("MIDI bridge running. Press Ctrl+C to stop.\n")

    buffer = bytearray()
    with serial.Serial(daisy_path, BAUDRATE, timeout=TIMEOUT) as daisy:
        while True:
            try:
                chunk = daisy.read(max(1, daisy.in_waiting))
                if chunk:
                    buffer.extend(chunk)

                while b"\n" in buffer:
                    line, _, rest = buffer.partition(b"\n")
                    buffer = bytearray(rest)
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
                        print(f"-> MIDI {msg}")
                    elif text:
                        # Optionally echo non-MIDI Daisy log lines
                        print(f"[DAISY] {text}")

            except KeyboardInterrupt:
                print("\nMIDI bridge stopped.")
                break
            except serial.SerialException as exc:
                print(f"\nDaisy disconnected: {exc}")
                break


if __name__ == "__main__":
    main()
