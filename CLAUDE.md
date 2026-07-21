# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

typefast is a timed English-word typing game for Windows: a tiny, self-contained,
fully offline `.exe` with no installer and no runtime dependencies. The player picks
a duration (60s / 2m / 5m), then types randomly chosen common words while a countdown
runs; results show WPM and accuracy. Design priorities, in order: small binary,
instant startup, zero dependencies.

## Build

Requires MSVC + the Windows SDK (for `windows.h`, `d2d1.h`, `dwrite.h`) and CMake.
There is no compiler on a bare machine — install "Visual Studio Build Tools" with the
"Desktop development with C++" workload, or `winget install Kitware.CMake` plus the
Build Tools.

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release   # optimized-for-size exe in build/Release/
cmake --build build --config Debug     # debuggable build
```

Run: `build/Release/typefast.exe` (or Debug). There is no separate run/lint step and
no test suite yet.

## Architecture

Three translation units, deliberately layered so the game rules stay testable in
isolation from Windows:

- **`src/game.{h,cpp}` — pure logic, no Win32/Direct2D.** Owns the rolling word
  stream, per-word typed state, and all scoring counters. `Game` is the source of
  truth for *counts*; it knows nothing about time or pixels. Key scoring rule: **net
  WPM credits only fully-correct words** (`correctChars`, incremented in `commitWord`
  when `typed == target`), while raw WPM counts everything typed. Accuracy is
  per-keystroke, tallied live in `typeChar`. WPM uses the standard 5-chars-per-word
  convention.

- **`src/app.{h,cpp}` — window + all Direct2D/DirectWrite rendering.** `App` owns the
  `HWND`, device resources, and *time* (QPC clock, countdown). The `Screen` enum
  (Menu / Typing / Results) is the state machine; `onChar` is the single input entry
  point and switches on `screen_`. The clock deliberately does **not** start until the
  first character is typed (`clockRunning_`), so opening the screen costs nothing.

- **`src/words.h` — the embedded word list**, a `constexpr` array compiled into the
  binary (keeps the exe self-contained; no external data file to ship or find). To
  change the vocabulary, edit this array only.

- **`src/main.cpp`** — `wWinMain`, sets DPI awareness, hands off to `App`.

### Things that will bite you if you don't know them

- **Input is via `WM_CHAR`, not raw key events** — this is intentional so keyboard
  layouts, shift, and dead keys work for free. Consequence: control keys arrive as
  control codes — Backspace = `0x08`, **Ctrl+Backspace = `0x7F` (DEL)**, Esc = `0x1B`,
  Enter = `\r`. Don't add a `WM_KEYDOWN` handler for these; you'll double-handle.

- **Rendering is on-demand, not a game loop.** Everything repaints via
  `InvalidateRect` on input, plus a 50 ms `WM_TIMER` only while on the Typing screen
  (for the countdown and caret blink). Don't introduce a continuous render loop — it
  fights the low-power design.

- **The typing view re-lays-out a rolling window of words each frame** in
  `renderTyping`. `firstVisible_` advances (dropping the top line) once the caret
  reaches the third line, via an iterative re-layout using `GetLineMetrics`. Per-glyph
  coloring (correct / wrong / extra / dim-untyped) is done with `SetDrawingEffect`
  ranges on the `IDWriteTextLayout`, coalesced into runs.

- **Device resources are recreated on demand.** `createDeviceResources` is a no-op if
  `rt_` exists; on `D2DERR_RECREATE_TARGET` from `EndDraw` all brushes and the render
  target are reset and rebuilt next paint. Any new device-dependent resource must be
  reset there too.

- **High scores** are best net-WPM per duration, persisted as plain text at
  `%APPDATA%\typefast\scores.txt` (`loadScores`/`saveScores`).

### Rendering stack note

Text uses **Direct2D + DirectWrite** (not raw Direct3D) for hardware-accelerated,
anti-aliased glyphs with no bundled font dependency. Calligraphy (`Segoe Script`) is
used for the title only; the words you actually type render in a legible monospace
(`Consolas`) — script faces are too ambiguous to type against.
