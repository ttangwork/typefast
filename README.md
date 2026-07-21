# typefast

typefast is an offline typing game for your fingers' pleasure. It randomly
generates a series of common English words for you to type within a limited time
window of your choosing, then scores your speed and accuracy.

It is small, instant to start, and completely offline — a single ~226 KB `.exe`
with no installer and no runtime dependencies.

## Features

- **Three timed modes:** 60 seconds, 2 minutes, or 5 minutes.
- **Live feedback:** each letter is colored as you type — white for correct, red
  for wrong, dim for not-yet-typed — with a blinking caret at your position.
- **Fair clock:** the countdown doesn't start until your first keystroke, so
  opening a round never costs you time.
- **Real scoring:** net WPM (only fully-correct words count), raw WPM, and
  per-keystroke accuracy.
- **Personal bests** are saved per mode and shown on the menu.
- **Anti-aliased text** rendered with Direct2D / DirectWrite; a calligraphy title
  over a highly legible monospace typing face.

## Controls

| Key | Action |
| --- | --- |
| `1` / `2` / `3` | Select 60s / 2m / 5m on the menu |
| `Enter` | Start a round (or play again on the results screen) |
| `Space` | Commit the current word and move on |
| `Backspace` | Delete the last character |
| `Ctrl+Backspace` | Clear the whole current word |
| `Esc` | Back to menu (from a round) / quit (from the menu) |

Input is read as text (`WM_CHAR`), so your keyboard layout, Shift, and dead keys
all work normally.

## Scoring

WPM uses the standard convention of 5 characters per word.

- **Net WPM** — credits only words you typed exactly right (including the space):
  `(correct characters / 5) / minutes`.
- **Raw WPM** — counts everything you typed, mistakes included.
- **Accuracy** — the percentage of keystrokes that were correct, tallied live.

Personal bests (best net WPM per mode) are stored as plain text at
`%APPDATA%\typefast\scores.txt`.

## Building from source

Requires **MSVC + the Windows SDK** (for `windows.h`, `d2d1.h`, `dwrite.h`) and
**CMake**. The easiest way to get both is the "Visual Studio Build Tools" with the
*Desktop development with C++* workload.

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The resulting binary is `build\Release\typefast.exe`. It is optimized for size and
statically links the CRT, so it runs on any Windows 10/11 machine with nothing else
installed.

## Technical stack

Written in C++17 against raw Win32, with text rendered in anti-aliased calligraphy
via Direct2D and DirectWrite. The word list (~470 common English words) is compiled
directly into the executable, keeping it fully self-contained and offline. No
frameworks, no bundled runtime, no external data files.
