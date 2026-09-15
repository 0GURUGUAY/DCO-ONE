#!/usr/bin/env python3
"""
Build and flash both Daisy Seed 3 and ESP32-S3 for DCO-ONE.

Usage:
    python3 flash_all.py           # build + flash both boards
    python3 flash_all.py --build   # build only
    python3 flash_all.py --daisy   # build + flash Daisy only
    python3 flash_all.py --esp32   # build + flash ESP32 only
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DAISY_BUILD = ROOT / "daisy" / "build"
DAISY_ELF = DAISY_BUILD / "dco_one_phase1"
DAISY_BIN = Path("/tmp/dco_one_phase1.bin")
ESP32_DIR = ROOT / "esp32"

GREEN = "\033[0;32m"
YELLOW = "\033[1;33m"
RED = "\033[0;31m"
CYAN = "\033[0;36m"
NC = "\033[0m"


def run(cmd, cwd=None, check=True, capture=False):
    """Run a shell command and return (returncode, stdout)."""
    print(f"{CYAN}$ {' '.join(str(c) for c in cmd)}{NC}")
    if capture:
        result = subprocess.run(
            cmd, cwd=cwd, text=True, capture_output=True
        )
    else:
        result = subprocess.run(cmd, cwd=cwd)
    if check and result.returncode != 0:
        raise RuntimeError(
            f"Command failed with code {result.returncode}: {' '.join(str(c) for c in cmd)}"
        )
    return result.returncode, result.stdout if capture else ""


def which(tool):
    return shutil.which(tool)


def section(title):
    print("")
    print(f"{YELLOW}{'=' * 60}{NC}")
    print(f"{YELLOW}  {title}{NC}")
    print(f"{YELLOW}{'=' * 60}{NC}")
    print("")


def build_daisy():
    section("Building Daisy firmware")
    if not which("cmake"):
        raise RuntimeError("cmake is not installed or not in PATH")
    if not which("make"):
        raise RuntimeError("make is not installed or not in PATH")

    DAISY_BUILD.mkdir(parents=True, exist_ok=True)
    run(["cmake", ".."], cwd=DAISY_BUILD)
    run(["make", "-j"], cwd=DAISY_BUILD)

    if not DAISY_ELF.exists():
        raise RuntimeError(f"Daisy ELF not found: {DAISY_ELF}")

    print(f"{GREEN}✓ Daisy build complete{NC}")


def build_esp32():
    section("Building ESP32-S3 firmware")
    if not which("platformio") and not which("pio"):
        raise RuntimeError("PlatformIO is not installed or not in PATH")

    pio = which("pio") or "platformio"
    run([pio, "run"], cwd=ESP32_DIR)
    print(f"{GREEN}✓ ESP32-S3 build complete{NC}")


def prepare_daisy_binary():
    section("Preparing Daisy binary")
    objcopy = which("arm-none-eabi-objcopy")
    if not objcopy:
        raise RuntimeError("arm-none-eabi-objcopy not found. Install arm-none-eabi-gcc.")
    run([objcopy, "-O", "binary", str(DAISY_ELF), str(DAISY_BIN)])
    print(f"{GREEN}✓ Binary ready: {DAISY_BIN}{NC}")


def upload_daisy():
    section("Flashing Daisy Seed 3")
    dfu = which("dfu-util")
    if not dfu:
        raise RuntimeError("dfu-util not found. Install it with: brew install dfu-util")

    print("Put Daisy in DFU mode:")
    print("  1. Connect Daisy via USB")
    print("  2. Hold BOOT")
    print("  3. Press RESET while holding BOOT")
    print("  4. Release both buttons")
    input(f"\n{YELLOW}Press ENTER when Daisy is in DFU mode...{NC}")

    print("\nChecking for DFU device...")
    result = subprocess.run(
        [dfu, "-l"], text=True, capture_output=True
    )
    dfu_output = result.stdout + result.stderr
    if "Found DFU" not in dfu_output:
        print(f"{RED}❌ No Daisy in DFU mode detected{NC}")
        print("dfu-util output:")
        print(dfu_output or "(empty)")
        print("Run 'dfu-util -l' to check.")
        sys.exit(1)

    print(f"{GREEN}✓ Daisy found in DFU mode{NC}\n")
    run([dfu, "-a", "0", "-D", str(DAISY_BIN), "-s", "0x08000000"])
    print(f"\n{GREEN}✓ Daisy flashing complete{NC}")
    print("Device will reset automatically...")
    time.sleep(2)


def find_esp32_port():
    """Find a likely ESP32-S3 USB serial port on macOS."""
    candidates = sorted(glob.glob("/dev/cu.usbmodem*")) + sorted(glob.glob("/dev/cu.usbserial*"))
    # Filter out obvious Daisy debug probe
    return [p for p in candidates if "debug" not in p.lower()]


def upload_esp32():
    section("Flashing ESP32-S3")
    pio = which("pio") or "platformio"

    ports = find_esp32_port()
    if not ports:
        print(f"{RED}❌ No ESP32-S3 serial port found{NC}")
        print("Available /dev/cu.* ports:")
        for p in sorted(glob.glob("/dev/cu.*")):
            print(f"  {p}")
        print("\nCheck that the ESP32-S3 is connected via USB.")
        sys.exit(1)

    port = ports[0]
    print(f"Using upload port: {port}")
    run([pio, "run", "--target", "upload", "--upload-port", port], cwd=ESP32_DIR)
    print(f"\n{GREEN}✓ ESP32-S3 flashing complete{NC}")


def main():
    parser = argparse.ArgumentParser(
        description="Build and/or flash DCO-ONE firmware for Daisy and ESP32-S3"
    )
    parser.add_argument(
        "--build", action="store_true", help="Build only, do not flash"
    )
    parser.add_argument(
        "--daisy", action="store_true", help="Daisy only"
    )
    parser.add_argument(
        "--esp32", action="store_true", help="ESP32-S3 only"
    )
    parser.add_argument(
        "--no-bridge", action="store_true", help="Do not start the serial bridge after flashing"
    )
    args = parser.parse_args()

    do_both = not args.daisy and not args.esp32
    build_only = args.build
    start_bridge = not build_only and not args.no_bridge

    try:
        if do_both or args.daisy:
            build_daisy()
            if not build_only:
                prepare_daisy_binary()
                upload_daisy()

        if do_both or args.esp32:
            build_esp32()
            if not build_only:
                upload_esp32()

        section("Done")
        print(f"{GREEN}🎵 All operations completed successfully!{NC}")

        if start_bridge:
            section("Starting serial bridge")
            bridge_script = ROOT / "combined_bridge.py"
            if bridge_script.exists():
                print(f"{CYAN}$ python3 {bridge_script}{NC}\n")
                subprocess.run([sys.executable, str(bridge_script)], cwd=ROOT)
            else:
                print(f"{YELLOW}⚠️  Bridge script not found: {bridge_script}{NC}")
    except RuntimeError as e:
        print(f"\n{RED}❌ Error: {e}{NC}")
        sys.exit(1)
    except KeyboardInterrupt:
        print(f"\n{YELLOW}⚠️  Aborted by user{NC}")
        sys.exit(130)


if __name__ == "__main__":
    main()
