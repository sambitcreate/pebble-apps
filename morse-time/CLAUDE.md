# Morse Time

Displays the current time and vibrates it in Morse code.

## How It Works
- Shows current time in both digital and Morse code notation
- Press Select to vibrate the time as Morse code
- Dot = short buzz (100ms), Dash = long buzz (300ms)
- Visual dots and dashes animate as they play
- Up: toggle 12h/24h format
- Down: vibrate current minutes only

## Platforms
- diorite, basalt, emery

## Build
```bash
pebble build
pebble install --emulator diorite
```
