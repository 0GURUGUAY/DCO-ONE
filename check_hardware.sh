#!/bin/bash

echo "=== DCO-ONE Phase 1 - Hardware & Environment Diagnostic ==="
echo ""

# Check Daisy Seed 3
echo "1. Checking Daisy Seed 3 Connection..."
DAISY_PORTS=$(ls /dev/cu.usbserial-* 2>/dev/null | wc -l)

if [ "$DAISY_PORTS" -gt 0 ]; then
    echo "   ✓ Found USB serial device(s):"
    ls /dev/cu.usbserial-* 2>/dev/null
else
    echo "   ⚠️  No USB serial devices found"
    echo "   → Connect Daisy Seed 3 or ESP32-S3 via USB"
fi

echo ""

# Check Daisy environment
echo "2. Checking Daisy Development Environment..."

if [ -z "$DAISY_ROOT" ]; then
    echo "   ✗ DAISY_ROOT environment variable not set"
    echo "   → Add to ~/.zshrc: export DAISY_ROOT=~/daisy-dev/libDaisy"
    echo "   → Then run: source ~/.zshrc"
else
    echo "   ✓ DAISY_ROOT is set to: $DAISY_ROOT"
    
    if [ -d "$DAISY_ROOT" ]; then
        echo "   ✓ libDaisy directory found"
    else
        echo "   ✗ libDaisy directory not found at: $DAISY_ROOT"
        echo "   → Clone from: https://github.com/electro-smith/libDaisy.git"
    fi
fi

echo ""

# Check DaisySP
echo "3. Checking DaisySP Library..."
if [ -d "$HOME/daisy-dev/DaisySP" ]; then
    echo "   ✓ DaisySP directory found"
else
    echo "   ⚠️  DaisySP not found at ~/daisy-dev/DaisySP"
    echo "   → Clone from: https://github.com/electro-smith/DaisySP.git"
fi

echo ""

# Check ARM compiler
echo "4. Checking ARM Toolchain..."
if command -v arm-none-eabi-gcc &> /dev/null; then
    echo "   ✓ ARM compiler found"
    arm-none-eabi-gcc --version | head -1
else
    echo "   ✗ ARM compiler not installed"
    echo "   → Install: brew install arm-none-eabi-gcc arm-none-eabi-binutils"
fi

echo ""

# Check PlatformIO
echo "5. Checking PlatformIO Installation..."
if command -v platformio &> /dev/null; then
    echo "   ✓ PlatformIO found"
    platformio --version
else
    echo "   ✗ PlatformIO not installed"
    echo "   → Install: pip install -U platformio"
fi

echo ""

# Summary
echo "=== Diagnostic Summary ==="
echo ""

READY=true

if [ -z "$DAISY_ROOT" ] || [ ! -d "$DAISY_ROOT" ]; then
    echo "❌ Daisy environment not configured"
    READY=false
else
    echo "✅ Daisy environment ready"
fi

if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo "❌ ARM compiler not installed"
    READY=false
else
    echo "✅ ARM compiler installed"
fi

if ! command -v platformio &> /dev/null; then
    echo "❌ PlatformIO not installed"
    READY=false
else
    echo "✅ PlatformIO installed"
fi

echo ""

if [ "$READY" = true ]; then
    echo "✅ Ready to build!"
    echo ""
    echo "Next steps:"
    echo "  1. Connect Daisy Seed 3 and ESP32-S3 via USB"
    echo "  2. Run: make build-all"
    echo "  3. Follow PHASE_1_SETUP.md for upload instructions"
else
    echo "⚠️  Some dependencies are missing"
    echo ""
    echo "Setup instructions:"
    echo "  1. See DAISY_INSTALLATION.md for Daisy setup"
    echo "  2. Install PlatformIO: pip install -U platformio"
    echo "  3. Re-run this diagnostic: bash check_hardware.sh"
fi

echo ""
