# Daisy Installation Quick Guide

## Problem
You tried to install `libopencm3` with Homebrew, but it doesn't exist as a formula.

## Solution
`libopencm3` comes with Daisy itself. You only need to:

1. **Install the ARM compiler**
2. **Clone the Daisy libraries**
3. **Set an environment variable**

---

## Step-by-Step Installation

### 1️⃣ Install ARM Compiler

```bash
brew install arm-none-eabi-gcc arm-none-eabi-binutils
```

Verify it worked:
```bash
arm-none-eabi-gcc --version
```

Output should show version 10.3.1 or similar.

---

### 2️⃣ Clone Daisy Libraries

```bash
# Create daisy development directory
mkdir -p ~/daisy-dev
cd ~/daisy-dev

# Clone the two required repositories
git clone https://github.com/electro-smith/libDaisy.git
git clone https://github.com/electro-smith/DaisySP.git

# Verify directories were created
ls ~/daisy-dev/
# Should show: libDaisy  DaisySP
```

---

### 3️⃣ Set Environment Variable

Add this line to your shell configuration file (`~/.zshrc` on Mac with zsh):

```bash
# Add to ~/.zshrc
echo 'export DAISY_ROOT=~/daisy-dev/libDaisy' >> ~/.zshrc

# Apply the change immediately
source ~/.zshrc

# Verify it's set
echo $DAISY_ROOT
# Should output: /Users/maxpatissier/daisy-dev/libDaisy
```

---

### 4️⃣ Verify Everything is Ready

```bash
# Check ARM compiler
arm-none-eabi-gcc --version

# Check environment variable is set
echo $DAISY_ROOT

# Check Daisy directories exist
ls $DAISY_ROOT
ls ~/daisy-dev/DaisySP
```

---

## Now You Can Build

From the DCO-ONE project directory:

```bash
cd /Users/maxpatissier/Downloads/DCO-ONE
make build-daisy
```

If the build succeeds, you're all set! ✅

---

## If Build Still Fails

### Error: "DAISY_ROOT not set"
- Make sure you ran: `echo 'export DAISY_ROOT=~/daisy-dev/libDaisy' >> ~/.zshrc`
- Make sure you ran: `source ~/.zshrc`
- Check: `echo $DAISY_ROOT` (should print a path, not empty)

### Error: "libDaisy not found"
- Verify directory exists: `ls ~/daisy-dev/libDaisy`
- Verify CMake files are there: `ls ~/daisy-dev/libDaisy/cmake/`
- If missing, re-run: `cd ~/daisy-dev && git clone https://github.com/electro-smith/libDaisy.git`

### Error: "DaisySP not found"
- Verify directory exists: `ls ~/daisy-dev/DaisySP`
- If missing, run: `cd ~/daisy-dev && git clone https://github.com/electro-smith/DaisySP.git`

---

## Reference

- **libDaisy**: https://github.com/electro-smith/libDaisy
- **DaisySP**: https://github.com/electro-smith/DaisySP
- **Daisy Documentation**: https://docs.daisy.audio/

---

**Next**: Run `make build-daisy` in the DCO-ONE directory
