#!/bin/bash
# Flash Daisy Seed 3 with new waveform display firmware

set -e

cd "$(dirname "$0")"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${YELLOW}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${YELLOW}║  Daisy Seed 3 - Waveform Display Firmware Flashing        ║${NC}"
echo -e "${YELLOW}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if binary exists
BINARY="/tmp/dco_one_phase1.bin"
if [ ! -f "$BINARY" ]; then
    echo -e "${RED}❌ Error: Firmware binary not found at $BINARY${NC}"
    echo "Run this first:"
    echo "  arm-none-eabi-objcopy -O binary daisy/build/dco_one_phase1 /tmp/dco_one_phase1.bin"
    exit 1
fi

# Check if dfu-util is available
if ! command -v dfu-util &> /dev/null; then
    echo -e "${RED}❌ Error: dfu-util not found${NC}"
    echo "Install it with: brew install dfu-util"
    exit 1
fi

echo -e "${YELLOW}📋 Instructions:${NC}"
echo ""
echo "1. Connect Daisy Seed 3 via USB cable to your Mac"
echo "2. Hold BOOT button on Daisy"
echo "3. While holding BOOT, press RESET button"
echo "4. Release both buttons (Daisy is now in DFU mode)"
echo "5. When ready, press ENTER to continue"
echo ""
read -p "Press ENTER when Daisy is in DFU mode..."

echo ""
echo -e "${YELLOW}Checking for DFU device...${NC}"
if ! dfu-util -l | grep -q "STM32"; then
    echo -e "${RED}❌ No Daisy in DFU mode detected${NC}"
    echo ""
    echo "Troubleshooting:"
    echo "  - Make sure you're holding BOOT when pressing RESET"
    echo "  - Try a different USB cable"
    echo "  - Check: dfu-util -l"
    exit 1
fi

echo -e "${GREEN}✓ Daisy found in DFU mode!${NC}"
echo ""

echo -e "${YELLOW}📦 Flashing firmware...${NC}"
dfu-util -a 0 -D "$BINARY" -s 0x08000000

echo ""
echo -e "${GREEN}✓ Flashing complete!${NC}"
echo ""
echo -e "${YELLOW}📌 Next steps:${NC}"
echo "1. Daisy will reset automatically"
echo "2. Both devices should now have the updated firmware"
echo "3. Run: python3 usb_bridge.py"
echo "4. Check ESP32 display for waveform in the center"
echo ""
echo -e "${GREEN}🎵 Waveform display is now active!${NC}"
