# Pebble Apps Collection

13 fun Pebble smartwatch apps — watchfaces, games, and utilities — all targeting **Pebble 2 and up**.

## Apps

| App | Type | Description |
|-----|------|-------------|
| **binary-clock** | Watchface | Time displayed as BCD binary dots |
| **magic-8ball** | App | Shake for fortune, accelerometer-powered |
| **pomodoro-timer** | App | 25/5 work-break cycles with progress bar |
| **reaction-tester** | Game | Measure your reflex time in milliseconds |
| **game-of-life** | Sim | Conway's cellular automaton, 36x42 grid |
| **morse-time** | App | Vibrate the current time in Morse code |
| **dice-roller** | App | D4/D6/D8/D10/D12/D20, shake to roll |
| **breathing** | Wellness | Guided breathing (4-7-8, Box, Simple patterns) |
| **pixel-invaders** | Game | Space invaders with wave progression |
| **snake** | Game | Classic snake with persistent high score |
| **tally-counter** | Utility | Persistent counter with haptic feedback |
| **stopwatch** | Utility | Precision timer with lap tracking |
| **compass** | Utility | Digital compass with rotating dial |

## Target Platforms

All apps support:
- **Diorite** — Pebble 2 / Pebble 2 Duo (144x168, B&W, 64KB)
- **Basalt** — Pebble Time / Time Steel (144x168, 64-color, 64KB)
- **Emery** — Pebble Time 2 (200x228, 64-color, 128KB)

Apps use `PBL_IF_COLOR_ELSE()` for color-aware rendering on basalt/emery while staying clean B&W on diorite.

---

## Local Development Setup (macOS)

### Prerequisites

You need [Homebrew](https://brew.sh), Node.js, and the `uv` package manager.

```bash
brew install node
curl -LsSf https://astral.sh/uv/install.sh | sh
```

### Install the Pebble SDK

```bash
# Install pebble-tool (uses Python 3.13 in an isolated env)
uv tool install pebble-tool --python 3.13

# Download the latest SDK (4.9.127+)
pebble sdk install latest

# Verify
pebble --version
pebble sdk list
```

> **Apple Silicon note:** The SDK's bundled ARM cross-compiler and QEMU emulator are x86_64 binaries that run via Rosetta 2. Install Rosetta if you haven't: `softwareupdate --install-rosetta --agree-to-license`

### Build an App

```bash
cd snake
pebble build
```

This compiles your C code for all target platforms and produces a `build/<appname>.pbw` file.

### Test in the Emulator

```bash
# Launch the QEMU emulator for Pebble 2 (B&W)
pebble install --emulator diorite

# Or test on color platforms
pebble install --emulator basalt
pebble install --emulator emery

# View logs
pebble logs --emulator diorite

# Take a screenshot
pebble screenshot --emulator diorite
```

The emulator maps keyboard keys to the four physical buttons (Up, Down, Select, Back).

---

## Deploy to a Real Pebble Watch

### Method 1: WiFi via Phone IP

1. On your phone, open the Pebble (or Core Devices) companion app
2. Go to **Settings > Developer Mode > Enable**
3. Tap **Developer Connection** and note the IP address
4. On your Mac:

```bash
cd snake
pebble build
pebble install --phone 192.168.1.XXX   # your phone's IP

# Watch live logs from the real device
pebble logs --phone 192.168.1.XXX
```

### Method 2: CloudPebble Connection

```bash
pebble install --cloudpebble
```

Authenticates via GitHub — no IP address needed. Still requires Developer Mode in the companion app.

### Method 3: Sideload the .pbw File

After `pebble build`, share the `build/<appname>.pbw` directly:
- AirDrop / email it to your phone
- Open it with the Pebble companion app
- It installs to the watch automatically

### Method 4: Publish to the App Store

Upload your `.pbw` to [dev-portal.rebble.io](https://dev-portal.rebble.io) with screenshots and metadata. Apps appear in both the Rebble and Core Devices stores.

---

## Updating an App After Code Changes

After editing `src/main.c` or any source file, follow these steps to get the updated app onto your watch:

```bash
# 1. Rebuild
cd binary-clock
pebble build

# 2. Pick ONE of these to deploy:

# Option A: Direct install via WiFi
pebble install --phone 192.168.1.XXX

# Option B: Copy to iCloud Drive, then open .pbw on phone
cp build/binary-clock.pbw ~/Library/Mobile\ Documents/com~apple~CloudDocs/Pebble\ Apps/

# Option C: AirDrop the .pbw to your phone
open build/   # opens Finder, then AirDrop the .pbw file
```

**Bulk rebuild all apps:**
```bash
cd /path/to/pebble
for app in */; do
  [ -f "$app/src/main.c" ] || continue
  echo "Building $app..."
  (cd "$app" && pebble build)
done
```

**Bulk copy all .pbw files to iCloud:**
```bash
for app in */build/*.pbw; do
  cp "$app" ~/Library/Mobile\ Documents/com~apple~CloudDocs/Pebble\ Apps/
done
```

The `.pbw` file in `build/` is the compiled app. Every time you change code, you need to `pebble build` again to regenerate it. The old version on your watch gets replaced when you install the new `.pbw`.

---

## Browser-Based Alternatives (No Local Setup)

If you don't want to install the SDK locally:

| Option | URL | What It Does |
|--------|-----|-------------|
| **CloudPebble** | cloudpebble.repebble.com | Full IDE with compiler + emulator in the browser |
| **GitHub Codespaces** | cloud.repebble.com | VS Code in browser with SDK pre-installed |
| **WebAssembly Emulator** | ericmigi.github.io/pebble-qemu-wasm | Boots real PebbleOS firmware in your browser |

CloudPebble is the fastest path — create a project, paste in `src/main.c` and `package.json`, click Build, and run on the emulator.

---

## Running Tests

### Static Analysis (no SDK required)

```bash
# Install cppcheck
brew install cppcheck

# Run the full test suite
./test.sh
```

The test suite checks:
1. **cppcheck static analysis** — catches bugs, memory issues, portability problems
2. **Project structure** — verifies all required files exist
3. **Platform targeting** — ensures diorite/basalt/emery are in package.json
4. **Code quality** — checks create/destroy balance, main() entry point, event loop
5. **SDK build** — compiles all apps (only if `pebble` CLI is installed)

### Full SDK Build Test

Once the SDK is installed, `./test.sh` automatically detects it and builds all 13 apps.

---

## Project Structure

```
pebble/
├── README.md
├── CLAUDE.md           # AI assistant context
├── test.sh             # Test suite
├── .gitignore
├── info.md             # Pebble dev reference guide
├── binary-clock/
│   ├── CLAUDE.md       # Per-project docs
│   ├── package.json    # Pebble manifest
│   ├── wscript         # Build config
│   └── src/
│       └── main.c      # App source
├── snake/
│   └── ...
└── (11 more apps)
```

## Troubleshooting

**`pebble` command not found after install:**
```bash
# Ensure ~/.local/bin is in your PATH
export PATH="$HOME/.local/bin:$PATH"
```

**Emulator fails to launch on Apple Silicon:**
The QEMU binary needs Rosetta 2. If the emulator times out, try running the command again — first launch can be slow.

**Python version too new:**
The `uv tool install --python 3.13` flag handles this automatically. Don't use your system Python.

**Build fails with missing SDK:**
```bash
pebble sdk install latest
pebble sdk set-channel release
```

---

## Resources

- [Developer Docs (Rebble)](https://developer.rebble.io) — archived original Pebble docs
- [Developer Docs (Core Devices)](https://developer.repebble.com) — actively updated, includes Alloy
- [CloudPebble IDE](https://cloudpebble.repebble.com) — browser-based development
- [Rebble Discord](https://rebble.io/discord) — #app-dev channel for help
- [r/pebble](https://reddit.com/r/pebble) — community forum
- [pebble-examples](https://github.com/pebble-examples) — official example projects
- [Learning C with Pebble](https://pebble.gitbooks.io) — free textbook
