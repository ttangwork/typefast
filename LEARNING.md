# Learning roadmap — how to build typefast yourself

This project sits at the intersection of about five separate skills. You don't need
all of it at once. Learn it roughly in this order — **C++ first**, then the Windows
layer, then the build/ship tooling falls into place as you go.

> **Don't read all of this before writing code.** Get through the C++ basics, then
> start building tiny windowed programs immediately and reach for the graphics/COM
> material as you hit the need for it. You learn this stack far faster by making tiny
> broken things than by finishing every book first.

---

## Stage 1 — Modern C++ (the language)

The foundation and the biggest time investment. Everything else assumes it.

- [ ] **[learncpp.com](https://www.learncpp.com)** — free, comprehensive, up to date. If
      you do one thing, do this.
- [ ] *A Tour of C++* by Bjarne Stroustrup — short and dense if you already program.
      (Or *Programming: Principles and Practice Using C++* if you're new to coding.)
- [ ] *Effective Modern C++* by Scott Meyers — smart pointers, move semantics, RAII.
      This is what `ComPtr` and the resource handling in `app.cpp` rely on.

**Unlocks in this repo:** all of `game.cpp`, the classes, `std::vector`/`std::wstring`,
RAII cleanup.

## Stage 2 — The Win32 API (windows, messages, the event loop)

- [ ] **[Learn to program Windows in C++](https://learn.microsoft.com/en-us/windows/win32/learnwin32/learn-to-program-for-windows)**
      (Microsoft Learn, free) — maps almost 1:1 to this repo. Goes from a bare window
      to a Direct2D app and introduces COM along the way.
- [ ] *Programming Windows*, 5th ed., by Charles Petzold — old (uses GDI) but the
      canonical explanation of the window/message model.

**Unlocks:** `main.cpp` and the top of `app.cpp` — `WndProc`, `WM_CHAR`, `WM_PAINT`,
the message loop.

## Stage 3 — COM + Direct2D + DirectWrite (the rendering)

COM is the object model behind `ID2D1Factory`, `IDWriteFactory`, and `ComPtr`.
Direct2D draws; DirectWrite lays out and rasterizes text.

- [ ] **[Direct2D docs](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-portal)**
      and **[DirectWrite docs](https://learn.microsoft.com/en-us/windows/win32/directwrite/direct-write-portal)**
      (Microsoft Learn) — both have "Getting Started" tutorials with working code.
- [ ] **Kenny Kerr's "Windows with C++" articles** (archived MSDN Magazine) — the best
      writing on *modern* COM with `ComPtr`/WRL, the exact style used in `app.h`.
      Search "Kenny Kerr Direct2D" / "Kenny Kerr COM".

**Unlocks:** `createDeviceResources`, brushes, text formats, `DrawTextLayout` with
per-glyph `SetDrawingEffect`, and the device-lost/recreate handling.

## Stage 4 — CMake + the MSVC/SDK toolchain (the build)

How the source becomes the ~226 KB exe.

- [ ] **[An Introduction to Modern CMake](https://cliutils.gitlab.io/modern-cmake/)** by
      Henry Schreiner (free) — the target-based style used in this repo's `CMakeLists.txt`.
- [ ] *Professional CMake: A Practical Guide* by Craig Scott — definitive, paid, kept current.

## Stage 5 — Git, GitHub, releases (shipping)

- [ ] **[Pro Git](https://git-scm.com/book)** by Scott Chacon — free, the standard.
- [ ] **GitHub docs** on Releases + the **[gh CLI manual](https://cli.github.com/manual/)** —
      the `gh release create` workflow used to publish `v0.1.0`.

## Bonus — the design patterns

- [ ] **[Game Programming Patterns](https://gameprogrammingpatterns.com)** by Robert
      Nystrom (free) — the "State" and "Game Loop" chapters *are* the
      Menu/Typing/Results state machine and the on-demand rendering in this app.

---

## The realistic minimum path

To rebuild *this specific project*, you can skip a lot:

1. **learncpp.com** (Stage 1)
2. **Microsoft's "Learn to program Windows in C++"** tutorial (Stages 2–3 in one)
3. Skim **Modern CMake** and **Pro Git**

Two free resources and two skims gets you ~90% of the way.

## Honest time estimate

- **C++ to real competence:** months, not weeks — the long pole.
- **Win32 + Direct2D layer:** ~2–4 weeks of evenings once your C++ is solid.
- **CMake, Git, release flow:** a few days each, picked up as you go.

Don't learn them in parallel. C++ first, then the Windows tutorial, and the tooling
will fall into place naturally.

## Suggested first exercise

Before touching this codebase, write the smallest possible version from scratch:
**open a window and draw one line of anti-aliased text in the center.** That single
program forces you through window creation, the message loop, COM initialization, a
Direct2D render target, and a DirectWrite text format — the entire skeleton of
`app.cpp` in ~80 lines. Everything else here is detail layered on top of that.
