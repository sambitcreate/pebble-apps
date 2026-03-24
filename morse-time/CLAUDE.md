# Morse Time

Displays the current time and vibrates it in Morse code with visual bar display.

## How It Works
- Shows current time in digital format with graphical Morse bars below
- Short thick bar = dot, long thick bar = dash, with colon separator between HH and MM
- Press Select to vibrate the time; bars highlight in sync during playback
- Up: toggle 12h/24h format
- Down: vibrate current minutes only
- Hold Up: cycle playback speed (Slow 150/450ms, Normal 100/300ms, Fast 60/180ms)
- Speed indicator displayed below the bars
- StatusBarLayer with dotted separator
- BT disconnect vibration alert
- First-launch help overlay (shown once, persisted)
- Settings (24h mode, speed) persisted across launches

## Architecture
- `s_bars_layer` canvas draws dot/dash bars from `MORSE_DIGITS` lookup
- `s_active_element` tracks which flat element index is highlighted during playback
- Playback uses `AppTimer` chain: `start_playback` -> `clear_highlight` -> `advance_playback` loop
- Vibration pattern built with speed-dependent durations
- Persist keys: KEY_24H (0), KEY_SPEED (1), KEY_FIRST_LAUNCH (2)

## Platforms
- diorite, basalt, emery

## Build
```bash
pebble build
pebble install --emulator diorite
```
