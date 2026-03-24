# Binary Clock

A watchface that displays the current time in binary (BCD) format.

## How It Works
- Hours, minutes, and seconds are shown as columns of binary dots
- Each column represents one decimal digit in Binary-Coded Decimal
- Filled circles = 1, empty circles = 0
- Read top-to-bottom: 8, 4, 2, 1

## Platforms
- diorite (Pebble 2) — B&W
- basalt (Pebble Time) — uses color accents
- emery (Pebble Time 2) — larger layout

## Build
```bash
pebble build
pebble install --emulator diorite
```
