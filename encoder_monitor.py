#!/usr/bin/env python3
"""
GPIO Monitoring Dashboard for DCO-ONE Daisy Seed 3
Displays real-time GPIO states in a clean table format
"""

import serial
import serial.tools.list_ports
import time
import sys
import os

# GPIO configuration
GPIO_CONFIG = {
    # Main encoder
    "MAIN_ENC_A":      {"pin": "D21", "type": "enc_a"},
    "MAIN_ENC_B":      {"pin": "D22", "type": "enc_b"},
    "MAIN_ENC_SW":     {"pin": "D23", "type": "button"},
    "MAIN_ENC_POS":    {"pin": "-",   "type": "counter"},
    # Programming encoder
    "PROG_ENC_A":      {"pin": "D15", "type": "enc_a"},
    "PROG_ENC_B":      {"pin": "D16", "type": "enc_b"},
    "PROG_ENC_SW":     {"pin": "D24", "type": "button"},
    "PROG_ENC_POS":    {"pin": "-",   "type": "counter"},
    # Menu encoder
    "MENU_ENC_A":      {"pin": "D17", "type": "enc_a"},
    "MENU_ENC_B":      {"pin": "D18", "type": "enc_b"},
    "MENU_ENC_SW":     {"pin": "D25", "type": "button"},
    "MENU_ENC_POS":    {"pin": "-",   "type": "counter"},
    # Sub-menu encoder
    "SUBMENU_ENC_A":   {"pin": "D19", "type": "enc_a"},
    "SUBMENU_ENC_B":   {"pin": "D20", "type": "enc_b"},
    "SUBMENU_ENC_SW":  {"pin": "D28", "type": "button"},
    "SUBMENU_ENC_POS": {"pin": "-",   "type": "counter"},
    # Buttons
    "BTN_HOME":        {"pin": "D12", "type": "button"},
    "BTN_MUTE":        {"pin": "D11", "type": "button"},
    "BTN_SOLO":        {"pin": "D10", "type": "button"},
    "BTN_TRIGGER":     {"pin": "D9",  "type": "button"},
}

# Current state
state = {
    "MAIN_ENC_A": 0, "MAIN_ENC_B": 0, "MAIN_ENC_SW": "R", "MAIN_ENC_POS": 0,
    "PROG_ENC_A": 0, "PROG_ENC_B": 0, "PROG_ENC_SW": "R", "PROG_ENC_POS": 0,
    "MENU_ENC_A": 0, "MENU_ENC_B": 0, "MENU_ENC_SW": "R", "MENU_ENC_POS": 0,
    "SUBMENU_ENC_A": 0, "SUBMENU_ENC_B": 0, "SUBMENU_ENC_SW": "R", "SUBMENU_ENC_POS": 0,
    "BTN_HOME": "R", "BTN_MUTE": "R", "BTN_SOLO": "R", "BTN_TRIGGER": "R",
}

def find_daisy_port():
    """Daisy Seed 3 enumerates as an STM32 USB CDC device."""
    for p in serial.tools.list_ports.comports():
        if "STM32" in p.hwid.upper() or "Daisy" in p.description or "0483:5740" in p.hwid.upper():
            return p.device
    return None

def parse_status_line(line):
    """Extract GPIO state from status line.
    Old format: Main:0[R] Menu:0[R] Prog:0[R] Submenu:0[R] Home:R Mute:R Solo:R Trigger:R
    New format: Main:0 Menu:0 Prog:0 MainSw:R MenuSw:R ProgSw:R Home:R Mute:R Solo:R Trigger:R
    Also handles: ENCODER_MAIN: CW [pos=1], etc.
    """
    try:
        parts = line.split()
        for part in parts:
            if part.startswith("Main:"):
                # Main:0[R] (old) or Main:0 (new)
                content = part.split(":")[1]
                if "[" in content:
                    pos, sw = content.split("[")
                    state["MAIN_ENC_POS"] = int(pos)
                    state["MAIN_ENC_SW"] = sw.rstrip("]")
                else:
                    state["MAIN_ENC_POS"] = int(content)
            elif part.startswith("Prog:"):
                # Prog:0[R] (old) or Prog:0 (new)
                content = part.split(":")[1]
                if "[" in content:
                    pos, sw = content.split("[")
                    state["PROG_ENC_POS"] = int(pos)
                    state["PROG_ENC_SW"] = sw.rstrip("]")
                else:
                    state["PROG_ENC_POS"] = int(content)
            elif part.startswith("Menu:"):
                # Menu:0[R] (old) or Menu:0 (new)
                content = part.split(":")[1]
                if "[" in content:
                    pos, sw = content.split("[")
                    state["MENU_ENC_POS"] = int(pos)
                    state["MENU_ENC_SW"] = sw.rstrip("]")
                else:
                    state["MENU_ENC_POS"] = int(content)
            elif part.startswith("Submenu:"):
                # Submenu:0[R] -> pos=0, switch=R
                content = part.split(":")[1]
                if "[" in content:
                    pos, sw = content.split("[")
                    state["SUBMENU_ENC_POS"] = int(pos)
                    state["SUBMENU_ENC_SW"] = sw.rstrip("]")
            elif part.startswith("MainSw:"):
                state["MAIN_ENC_SW"] = part.split(":")[1]
            elif part.startswith("MenuSw:"):
                state["MENU_ENC_SW"] = part.split(":")[1]
            elif part.startswith("ProgSw:"):
                state["PROG_ENC_SW"] = part.split(":")[1]
            elif part.startswith("Home:"):
                # Home:R or Home:P
                state["BTN_HOME"] = part.split(":")[1]
            elif part.startswith("Mute:"):
                # Mute:R or Mute:P
                state["BTN_MUTE"] = part.split(":")[1]
            elif part.startswith("Solo:"):
                # Solo:R or Solo:P
                state["BTN_SOLO"] = part.split(":")[1]
            elif part.startswith("Trigger:"):
                # Trigger:R or Trigger:P
                state["BTN_TRIGGER"] = part.split(":")[1]
    except:
        pass

def parse_event_line(line):
    """Extract GPIO state from event lines"""
    if "ENCODER_MAIN:" in line:
        if "CW" in line:
            state["MAIN_ENC_A"] = 1
        elif "CCW" in line:
            state["MAIN_ENC_A"] = -1
        if "pos=" in line:
            try:
                pos = int(line.split("pos=")[1].rstrip("]"))
                state["MAIN_ENC_POS"] = pos
            except:
                pass
    elif "ENCODER_MAIN_SWITCH:" in line:
        state["MAIN_ENC_SW"] = "P" if "PRESSED" in line else "R"
    elif "ENCODER_PROG:" in line:
        if "CW" in line:
            state["PROG_ENC_A"] = 1
        elif "CCW" in line:
            state["PROG_ENC_A"] = -1
        if "pos=" in line:
            try:
                pos = int(line.split("pos=")[1].rstrip("]"))
                state["PROG_ENC_POS"] = pos
            except:
                pass
    elif "BUTTON_PROG:" in line:
        state["PROG_ENC_SW"] = "P" if "PRESSED" in line else "R"
    elif "BUTTON_MAIN:" in line:
        state["MAIN_ENC_SW"] = "P" if "PRESSED" in line else "R"
    elif "BUTTON_MENU:" in line:
        state["MENU_ENC_SW"] = "P" if "PRESSED" in line else "R"
    elif "ENCODER_MENU:" in line:
        if "CW" in line:
            state["MENU_ENC_A"] = 1
        elif "CCW" in line:
            state["MENU_ENC_A"] = -1
        if "pos=" in line:
            try:
                pos = int(line.split("pos=")[1].rstrip("]"))
                state["MENU_ENC_POS"] = pos
            except:
                pass
    elif "ENCODER_MENU_SWITCH:" in line:
        state["MENU_ENC_SW"] = "P" if "PRESSED" in line else "R"
    elif "ENCODER_SUBMENU:" in line:
        if "CW" in line:
            state["SUBMENU_ENC_A"] = 1
        elif "CCW" in line:
            state["SUBMENU_ENC_A"] = -1
        if "pos=" in line:
            try:
                pos = int(line.split("pos=")[1].rstrip("]"))
                state["SUBMENU_ENC_POS"] = pos
            except:
                pass
    elif "ENCODER_SUBMENU_SWITCH:" in line:
        state["SUBMENU_ENC_SW"] = "P" if "PRESSED" in line else "R"
    elif "BUTTON_HOME:" in line:
        state["BTN_HOME"] = "P" if "PRESSED" in line else "R"
    elif "BUTTON_MUTE:" in line:
        state["BTN_MUTE"] = "P" if "PRESSED" in line else "R"
    elif "BUTTON_SOLO:" in line:
        state["BTN_SOLO"] = "P" if "PRESSED" in line else "R"
    elif "BUTTON_TRIGGER:" in line:
        state["BTN_TRIGGER"] = "P" if "PRESSED" in line else "R"

def clear_screen():
    """Clear terminal screen"""
    os.system("clear" if os.name != "nt" else "cls")

def print_table():
    """Print GPIO state table"""
    clear_screen()
    print("╔════════════════════════════════════════════════════════════╗")
    print("║         DCO-ONE GPIO MONITOR — DAISY SEED 3               ║")
    print("╠════════════════════════════════════════════════════════════╣")
    print("║ GPIO Name            │ Pin  │ State                       ║")
    print("╠════════════════════════════════════════════════════════════╣")
    
    # MAIN ENCODER
    print(f"║ 🔄 MAIN ENC-A       │ {GPIO_CONFIG['MAIN_ENC_A']['pin']:4s} │ {str(state['MAIN_ENC_A']):31s} ║")
    print(f"║    MAIN ENC-B       │ {GPIO_CONFIG['MAIN_ENC_B']['pin']:4s} │ {str(state['MAIN_ENC_B']):31s} ║")
    print(f"║    MAIN ENC-POS     │ {GPIO_CONFIG['MAIN_ENC_POS']['pin']:4s} │ Position: {state['MAIN_ENC_POS']:23d} ║")
    print(f"║    MAIN ENC-SW      │ {GPIO_CONFIG['MAIN_ENC_SW']['pin']:4s} │ {('PRESSED' if state['MAIN_ENC_SW']=='P' else 'RELEASED'):31s} ║")
    
    print("╠════════════════════════════════════════════════════════════╣")
    
    # PROGRAMMING ENCODER
    print(f"║ ⚡ PROG ENC-A       │ {GPIO_CONFIG['PROG_ENC_A']['pin']:4s} │ {str(state['PROG_ENC_A']):31s} ║")
    print(f"║    PROG ENC-B       │ {GPIO_CONFIG['PROG_ENC_B']['pin']:4s} │ {str(state['PROG_ENC_B']):31s} ║")
    print(f"║    PROG ENC-POS     │ {GPIO_CONFIG['PROG_ENC_POS']['pin']:4s} │ Position: {state['PROG_ENC_POS']:23d} ║")
    print(f"║    PROG ENC-SW      │ {GPIO_CONFIG['PROG_ENC_SW']['pin']:4s} │ {('PRESSED' if state['PROG_ENC_SW']=='P' else 'RELEASED'):31s} ║")
    
    print("╠════════════════════════════════════════════════════════════╣")
    
    # MENU ENCODER
    print(f"║ ⚙️  MENU ENC-A       │ {GPIO_CONFIG['MENU_ENC_A']['pin']:4s} │ {str(state['MENU_ENC_A']):31s} ║")
    print(f"║    MENU ENC-B       │ {GPIO_CONFIG['MENU_ENC_B']['pin']:4s} │ {str(state['MENU_ENC_B']):31s} ║")
    print(f"║    MENU ENC-POS     │ {GPIO_CONFIG['MENU_ENC_POS']['pin']:4s} │ Position: {state['MENU_ENC_POS']:23d} ║")
    print(f"║    MENU ENC-SW      │ {GPIO_CONFIG['MENU_ENC_SW']['pin']:4s} │ {('PRESSED' if state['MENU_ENC_SW']=='P' else 'RELEASED'):31s} ║")
    
    print("╠════════════════════════════════════════════════════════════╣")
    
    # SUB-MENU ENCODER
    print(f"║ 📋 SUBMENU ENC-A    │ {GPIO_CONFIG['SUBMENU_ENC_A']['pin']:4s} │ {str(state['SUBMENU_ENC_A']):31s} ║")
    print(f"║    SUBMENU ENC-B    │ {GPIO_CONFIG['SUBMENU_ENC_B']['pin']:4s} │ {str(state['SUBMENU_ENC_B']):31s} ║")
    print(f"║    SUBMENU ENC-POS  │ {GPIO_CONFIG['SUBMENU_ENC_POS']['pin']:4s} │ Position: {state['SUBMENU_ENC_POS']:23d} ║")
    print(f"║    SUBMENU ENC-SW   │ {GPIO_CONFIG['SUBMENU_ENC_SW']['pin']:4s} │ {('PRESSED' if state['SUBMENU_ENC_SW']=='P' else 'RELEASED'):31s} ║")
    
    print("╠════════════════════════════════════════════════════════════╣")
    
    # BUTTONS
    print(f"║ 🏠 HOME BUTTON      │ {GPIO_CONFIG['BTN_HOME']['pin']:4s} │ {('PRESSED' if state['BTN_HOME']=='P' else 'RELEASED'):31s} ║")
    print(f"║ 🔇 MUTE BUTTON      │ {GPIO_CONFIG['BTN_MUTE']['pin']:4s} │ {('PRESSED' if state['BTN_MUTE']=='P' else 'RELEASED'):31s} ║")
    print(f"║ 🔊 SOLO BUTTON      │ {GPIO_CONFIG['BTN_SOLO']['pin']:4s} │ {('PRESSED' if state['BTN_SOLO']=='P' else 'RELEASED'):31s} ║")
    print(f"║ ⏯️  TRIGGER BUTTON   │ {GPIO_CONFIG['BTN_TRIGGER']['pin']:4s} │ {('PRESSED' if state['BTN_TRIGGER']=='P' else 'RELEASED'):31s} ║")
    
    print("╚════════════════════════════════════════════════════════════╝")
    print("\nPress Ctrl+C to exit\n")

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_daisy_port()
    baud = 115200

    if not port:
        print("Could not auto-detect the Daisy Seed serial port.")
        print("Detected ports:")
        for p in serial.tools.list_ports.comports():
            print(f"  {p.device}: {p.description} [{p.hwid}]")
        sys.exit(1)

    print(f"Connecting to {port} at {baud} baud...")

    ser = None
    try:
        ser = serial.Serial(port, baud, timeout=1)
    except serial.SerialException as e:
        print(f"✗ Failed to connect: {e}")
        sys.exit(1)

    try:
        time.sleep(0.5)

        last_update = time.time()
        update_interval = 0.1  # Update display every 100ms

        while True:
            try:
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line and not line.startswith("DCO1"):  # Skip audio frames
                        parse_event_line(line)
                        parse_status_line(line)

                # Update display at regular intervals
                now = time.time()
                if now - last_update >= update_interval:
                    print_table()
                    last_update = now

            except KeyboardInterrupt:
                clear_screen()
                print("\n✓ Monitor stopped by user\n")
                break
            except Exception as e:
                pass
    finally:
        if ser is not None and ser.is_open:
            ser.close()

if __name__ == "__main__":
    main()
