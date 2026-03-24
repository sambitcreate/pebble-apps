# Stopwatch

Precision stopwatch with lap time tracking, circular arc, and lap comparison.

## How It Works
- Large MM:SS.t display (tenths of seconds)
- Circular elapsed-time arc completes one rotation per minute
- Select: Start/Stop
- Up: Lap (records individual lap duration while running)
- Down: Reset (when stopped)
- Shows last 3 lap times on screen
- Lap delta comparison: shows "+X.X" (red/slower) or "-X.X" (green/faster) vs previous lap
- Vibration on lap, double vibration on BT disconnect
- State persisted across app restarts (elapsed, running, laps)
- First-launch help overlay (shown once)
- StatusBarLayer with dotted separator

## Architecture
- `s_laps[]` stores individual lap durations (not cumulative)
- `s_last_lap_start` tracks where the current lap began
- `s_lap_delta` = current lap - previous lap (positive = slower)
- Persist keys 0-24 store state across sessions

## Platforms
- diorite, basalt, emery

## Build
```bash
pebble build
pebble install --emulator diorite
```
