# Pomodoro Timer

A productivity timer using the Pomodoro Technique on your wrist.

## How It Works
- **Work session**: 25 minutes (default)
- **Short break**: 5 minutes
- **Long break**: 15 minutes (after 4 work sessions)
- Select: Start/Pause
- Up: Reset current session
- Down: Skip to next phase
- Vibration alerts on phase transitions
- Progress bar shows time remaining

## Platforms
- diorite, basalt, emery

## Build
```bash
pebble build
pebble install --emulator diorite
```
