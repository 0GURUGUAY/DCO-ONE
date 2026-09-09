# DCO-ONE Project Index

## 📌 Quick Navigation

### 🚀 First-Time Setup
1. [START_HERE.md](START_HERE.md) - **Read this first!**
2. [README.md](README.md) - Project overview
3. [PHASE_1_SETUP.md](PHASE_1_SETUP.md) - Detailed instructions

### 🔧 Build & Verification
4. [VERIFICATION_TASKS.md](VERIFICATION_TASKS.md) - Testing checklist
5. [USB_PORT_GUIDE.md](USB_PORT_GUIDE.md) - Hardware identification
6. [PHASE_1_IMPLEMENTATION.md](PHASE_1_IMPLEMENTATION.md) - What was built

### 📐 Architecture & Design
7. [PROJECT_ARCHITECTURE_V2.md](PROJECT_ARCHITECTURE_V2.md) - Full specs

---

## 📁 Directory Structure

```
DCO-ONE/
│
├── 📘 START_HERE.md                 ← Read first!
├── 📘 README.md                     ← Project overview
├── 📘 PHASE_1_SETUP.md              ← Detailed setup
├── 📘 PHASE_1_IMPLEMENTATION.md    ← What was created
├── 📘 VERIFICATION_TASKS.md        ← Testing guide
├── 📘 USB_PORT_GUIDE.md            ← Port identification
├── 📘 PROJECT_ARCHITECTURE_V2.md   ← Full architecture
├── 📘 PROJECT_INDEX.md             ← This file
│
├── 🔨 Makefile                      ← Build automation
├── 🔧 check_hardware.sh             ← Diagnostic tool
│
├── 📁 daisy/                         ← Daisy Seed 3 firmware
│   ├── CMakeLists.txt               ← CMake config
│   ├── src/
│   │   ├── main.cpp                 ← Audio callback
│   │   └── oscillator.cpp           ← 440 Hz generator
│   └── include/
│       └── oscillator.h             ← Class definition
│
├── 📁 esp32/                         ← ESP32-S3 firmware
│   ├── platformio.ini               ← PlatformIO config
│   └── src/
│       └── main.cpp                 ← Display + audio
│
├── 📁 shared/                        ← Shared code (Phase 2+)
│
└── 📁 backup/                        ← Backup directory
```

---

## 📖 Document Guide

### START_HERE.md (You are here!)
**Length**: ~300 lines | **Time to read**: 10 min  
**Content**: Quick actions, next steps, command reference  
**When to read**: When you first start

### README.md
**Length**: ~200 lines | **Time to read**: 10 min  
**Content**: Project overview, quick start, hardware requirements  
**When to read**: For project summary

### PHASE_1_SETUP.md
**Length**: ~350 lines | **Time to read**: 20 min  
**Content**: Detailed setup, build instructions, troubleshooting  
**When to read**: When building firmware

### PHASE_1_IMPLEMENTATION.md
**Length**: ~400 lines | **Time to read**: 15 min  
**Content**: What was implemented, technical specs, architecture  
**When to read**: To understand Phase 1 implementation

### VERIFICATION_TASKS.md
**Length**: ~500 lines | **Time to read**: 25 min  
**Content**: Step-by-step testing guide, verification checklist  
**When to read**: When testing hardware

### USB_PORT_GUIDE.md
**Length**: ~250 lines | **Time to read**: 10 min  
**Content**: USB identification, port detection, troubleshooting  
**When to read**: If you can't find USB ports

### PROJECT_ARCHITECTURE_V2.md
**Length**: ~1000 lines | **Time to read**: 45 min  
**Content**: Full architecture, all phases, detailed specs  
**When to read**: To understand overall project design

---

## 🎯 Suggested Reading Order

### For First-Time Users (30 min)
1. [START_HERE.md](START_HERE.md) (5 min)
2. [README.md](README.md) (10 min)
3. Quick skim of [PHASE_1_SETUP.md](PHASE_1_SETUP.md) (15 min)

### For Building & Testing (45 min)
1. [USB_PORT_GUIDE.md](USB_PORT_GUIDE.md) (10 min) - Identify ports
2. [VERIFICATION_TASKS.md](VERIFICATION_TASKS.md) (25 min) - Follow checklist
3. [PHASE_1_SETUP.md](PHASE_1_SETUP.md) as reference

### For Understanding the Design (60 min)
1. [PHASE_1_IMPLEMENTATION.md](PHASE_1_IMPLEMENTATION.md) (15 min)
2. [PROJECT_ARCHITECTURE_V2.md](PROJECT_ARCHITECTURE_V2.md) (45 min)

---

## 🔑 Key Information at a Glance

### Hardware
- **Daisy Seed 3**: 600 MHz STM32H7, 48 kHz audio, real-time safe
- **ESP32-S3**: Dual-core 240 MHz, Waveshare AMOLED 1.75" (466×466)
- **Mac Mini A4 Pro**: Development host with USB

### Phase 1 Output
- **Daisy**: Clean 440 Hz sine wave
- **ESP32-S3**: Oscilloscope-style waveform display (placeholder)

### Build Tools Required
- ✅ PlatformIO 6.1.19+ (for ESP32-S3)
- ✅ ARM GCC compiler 10.3.1+ (for Daisy)
- ❌ Daisy toolchain (needs installation)

### Phase 1 Status
🟢 **Framework Complete - Ready for Hardware Testing**

---

## 🚀 Next Steps

```bash
# 1. Install Daisy toolchain
brew install arm-none-eabi-gcc

# 2. Connect both boards via USB

# 3. Check hardware
make check-hardware

# 4. Build firmware
make build-all

# 5. Upload to boards
make upload-daisy
make upload-esp32

# 6. Monitor output
make monitor-daisy     # Terminal 1
make monitor-esp32     # Terminal 2 (in another window)
```

---

## 📞 Finding Information

| Question | Document |
|----------|----------|
| "What is this project?" | [README.md](README.md) |
| "How do I get started?" | [START_HERE.md](START_HERE.md) |
| "How do I build the firmware?" | [PHASE_1_SETUP.md](PHASE_1_SETUP.md) |
| "How do I test everything?" | [VERIFICATION_TASKS.md](VERIFICATION_TASKS.md) |
| "Which USB port is which?" | [USB_PORT_GUIDE.md](USB_PORT_GUIDE.md) |
| "What was implemented?" | [PHASE_1_IMPLEMENTATION.md](PHASE_1_IMPLEMENTATION.md) |
| "What's the full architecture?" | [PROJECT_ARCHITECTURE_V2.md](PROJECT_ARCHITECTURE_V2.md) |
| "How do I compile the code?" | [Makefile](Makefile) |

---

## 🎯 Success Criteria for Phase 1

✅ Both boards recognized by OS  
✅ Both firmwares compile without errors  
✅ Both firmwares upload successfully  
✅ Daisy generates stable 440 Hz sine wave  
✅ ESP32-S3 display initializes  
✅ Both boards run stable for >1 minute  

---

## 📝 Key Files to Remember

| File | Purpose |
|------|---------|
| `Makefile` | Build automation - run all commands here |
| `check_hardware.sh` | Verify hardware connections |
| `daisy/src/main.cpp` | Daisy audio callback |
| `esp32/src/main.cpp` | ESP32-S3 display code |

---

## 🔗 External Resources

- [Daisy Seed 3 Docs](https://docs.daisy.audio/hardware/Seed3/)
- [Waveshare AMOLED Docs](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75)
- [libDaisy GitHub](https://github.com/electro-smith/libDaisy)
- [PlatformIO Docs](https://docs.platformio.org/)

---

## 📅 Development Timeline

| Phase | Status | Goal |
|-------|--------|------|
| **Phase 1** | ✅ Complete | Hardware verification (current) |
| **Phase 2** | ⏳ Ready | Audio I/O + UART communication |
| **Phase 3** | 📅 Planned | Physical controls |
| **Phase 4** | 📅 Planned | CV inputs |
| **Phase 5-13** | 📅 Planned | See PROJECT_ARCHITECTURE_V2.md |

---

## 💾 Backup

All code is backed up in `backup/` directory.

---

## 🔄 Version Information

| Component | Version |
|-----------|---------|
| Phase 1 Framework | 1.0 |
| Created | 2026-09-05 |
| Daisy Target | Seed 3 |
| ESP32 Target | S3 (240 MHz) |
| Display | Waveshare AMOLED 1.75" |

---

**Welcome to DCO-ONE! 🎵**

Start with [START_HERE.md](START_HERE.md) →
