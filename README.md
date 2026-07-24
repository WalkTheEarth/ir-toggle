# IR Toggle

A Flipper Zero app that turns the IR LED on and off like a flashlight.

## Usage

- **OK** — Toggle IR LED on/off
- **BACK** — Exit app

The IR LED emits at 38 kHz (940nm). To verify it's working, point the Flipper Zero at your phone's camera — the IR LED will appear as a bright purple/white glow on screen.

> **Note:** Continuous IR operation draws significant power. Avoid leaving it on for extended periods to preserve battery life.

## Build

```bash
# Install ufbt if you don't have it
pip install ufbt

# Build the .fap
ufbt build
```

The compiled app will be at `build/f7-firmware-D/.extapps/ir_toggle.fap`.

## Install

Copy `ir_toggle.fap` to your Flipper Zero SD card under `apps/GPIO/` (or `apps/Tools/`), then launch it from the GPIO (or Tools) menu.

## License

MIT
