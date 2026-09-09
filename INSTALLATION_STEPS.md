# ✅ Daisy Installation - Step by Step

Based on the diagnostic output, here's what you need to do:

## Current Status

✅ **Already have:**
- ARM compiler: version 10.3.1
- PlatformIO: version 6.1.19

❌ **Still need:**
- DAISY_ROOT environment variable configured
- libDaisy repository cloned
- DaisySP repository cloned

---

## Installation Instructions (Copy & Paste)

### Step 1: Clone the Daisy Repositories

```bash
# Create directory for Daisy development
mkdir -p ~/daisy-dev
cd ~/daisy-dev

# Clone the two required repositories
git clone https://github.com/electro-smith/libDaisy.git
git clone https://github.com/electro-smith/DaisySP.git

# Verify both cloned successfully
ls -la ~/daisy-dev/
# Should show both "libDaisy" and "DaisySP" directories
```

**Time**: ~2-3 minutes (depends on internet speed)

---

### Step 2: Set Environment Variable

```bash
# Add environment variable to shell configuration
echo 'export DAISY_ROOT=~/daisy-dev/libDaisy' >> ~/.zshrc

# Apply immediately
source ~/.zshrc

# Verify it's set
echo $DAISY_ROOT
# Should print: /Users/maxpatissier/daisy-dev/libDaisy
```

**Note**: If you use bash instead of zsh, replace `~/.zshrc` with `~/.bash_profile`

---

### Step 3: Verify Everything is Ready

```bash
# Run the diagnostic again
cd /Users/maxpatissier/Downloads/DCO-ONE
bash check_hardware.sh
```

**Expected output:**
- ✅ ARM compiler installed
- ✅ PlatformIO installed
- ✅ Daisy environment ready

---

## Now You Can Build!

Once the diagnostic shows everything ✅:

```bash
cd /Users/maxpatissier/Downloads/DCO-ONE

# Build both Daisy and ESP32-S3
make build-all

# Or build individually
make build-daisy    # Just Daisy
make build-esp32    # Just ESP32-S3
```

---

## If Something Goes Wrong

### Issue: "git: command not found"
```bash
# Install Xcode Command Line Tools
xcode-select --install
```

### Issue: "Permission denied" when cloning
```bash
# Make sure you have write permissions in home directory
ls -ld ~
# Should show "drwx------" or similar with your username
```

### Issue: DAISY_ROOT still shows as empty after `source ~/.zshrc`
```bash
# Double check the file was updated
grep DAISY_ROOT ~/.zshrc

# If it's not there, run the echo command again
echo 'export DAISY_ROOT=~/daisy-dev/libDaisy' >> ~/.zshrc
source ~/.zshrc
```

### Issue: "libDaisy not found at path"
```bash
# Verify the directory exists
ls ~/daisy-dev/libDaisy

# If it doesn't exist, the clone failed
# Re-run the clone commands
cd ~/daisy-dev
git clone https://github.com/electro-smith/libDaisy.git
```

---

## Next Steps After Build

Once build succeeds:

1. **Connect hardware**
   - Connect Daisy Seed 3 to Mac Mini via USB
   - Connect ESP32-S3 to Mac Mini via different USB port

2. **Upload firmware**
   ```bash
   make upload-daisy    # (Put Daisy in DFU mode first)
   make upload-esp32
   ```

3. **Monitor output**
   ```bash
   make monitor-daisy   # Terminal 1
   make monitor-esp32   # Terminal 2
   ```

---

## Reference Documents

- [DAISY_INSTALLATION.md](DAISY_INSTALLATION.md) - Detailed Daisy setup
- [PHASE_1_SETUP.md](PHASE_1_SETUP.md) - Phase 1 instructions
- [README.md](README.md) - Project overview
- [START_HERE.md](START_HERE.md) - Quick actions

---

## External Resources

- **libDaisy**: https://github.com/electro-smith/libDaisy
- **DaisySP**: https://github.com/electro-smith/DaisySP
- **Daisy Seed 3 Docs**: https://docs.daisy.audio/hardware/Seed3/
- **Getting Started**: https://github.com/electro-smith/DaisyWiki/wiki/1.-Getting-Started

---

**Ready?** → Go to [PHASE_1_SETUP.md](PHASE_1_SETUP.md) for the next steps
