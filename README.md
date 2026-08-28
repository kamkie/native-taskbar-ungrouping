# Native Taskbar Ungrouping

A Windhawk mod that gives every window its own native Windows 11 taskbar
button without changing the taskbar's native look — including the compact
48-DIP icon-only layout of the native left/right taskbar.

## Behavior

- Forces native **combine** presentation in Explorer while the taskbar is on
  the left or right, preserving the 48-DIP icon-only layout without changing
  the saved Windows preference.
- Creates a separate native task group and button for each newly opened window,
  adjacent to windows from the same application.
- Preserves native running indicators, progress, badges, overlays, highlights,
  animations, Start, tray, clock, flyouts, appbar geometry, and hit testing.
- Preserves application identity for pinned items, icons, launching, and jump
  lists.
- Has no user settings or cosmetic options.

Existing windows must be closed and reopened after enabling the mod so Explorer
resolves them into separate task groups.

## Architecture

The mod adapts the proven task-model implementation from m417z's
[`taskbar-grouping`](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-grouping.wh.cpp)
mod. Each resolved window receives a temporary unique AppID suffix. Supporting
hooks strip or translate the suffix where Explorer needs the application's real
identity, including pins, icons, launches, and jump lists.

The mod intercepts Explorer's `TaskbarGlomLevel` reads only while the native
taskbar is vertical, returning combine mode in memory. It never changes the
registry, taskbar XAML, labels, button dimensions, frame measurements,
system-tray measurements, HWND size, or appbar reservation. The implementation
doesn't contain Windows 10 or ExplorerPatcher compatibility paths.

## Current target

- Windows 11 Pro 25H2, build 26200.9278, x64
- Windhawk 2.0.0-alpha.2
- Native taskbar position: Left or Right

## Installation

Compile and install `native-taskbar-ungrouping.wh.cpp` as a local Windhawk
mod. Disable **Taskbar Labels for Windows 11**, **Vertical Taskbar for Windows
11**, and the upstream **Disable grouping on the taskbar** mod to avoid duplicate
hooks.

## License

Licensed under [GPL-3.0](LICENSE), matching the upstream derivative. See
[NOTICE.md](NOTICE.md) for attribution and a summary of modifications.
