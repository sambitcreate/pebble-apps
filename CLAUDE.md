# Pebble Apps Monorepo

Collection of fun Pebble smartwatch apps targeting **Pebble 2 (diorite) and up**.

## Target Platforms
- **diorite** — Pebble 2 (144×168, B&W, 64KB)
- **basalt** — Pebble Time (144×168, 64-color, 64KB)
- **emery** — Pebble Time 2 (200×228, 64-color, 128KB)

## Building
Each subdirectory is a standalone Pebble project. Build with:
```bash
cd <project-name>
pebble build
```
Or import into CloudPebble at cloudpebble.repebble.com.

## Testing
```bash
pebble install --emulator diorite
pebble install --emulator basalt
pebble install --emulator emery
```

## Project Structure
Each app contains:
- `package.json` — Pebble project manifest
- `wscript` — Build configuration
- `src/main.c` — Application source
- `CLAUDE.md` — Project-specific docs

## Language
All apps are written in C targeting the Pebble SDK 4.x API.
Use `PBL_IF_COLOR_ELSE()` and `PBL_IF_ROUND_ELSE()` for cross-platform support.
