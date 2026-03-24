# Tally Counter

Simple persistent tally counter. Count anything.

## How It Works
- Big number display, easy to read at a glance
- Select: +1 (with haptic click)
- Up: +5
- Down: -1 (won't go below 0)
- Long press Select: Reset to 0
- Count persists across app restarts (persistent storage)

## Platforms
- diorite, basalt, emery

## Build
```bash
pebble build
pebble install --emulator diorite
```
