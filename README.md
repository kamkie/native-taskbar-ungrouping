# Native Compact Vertical Taskbar

A private Windhawk mod for the native Windows 11 left/right taskbar.

## Behavior

- Keeps running windows as separate native taskbar buttons.
- Collapses only the native label column so buttons remain icon-only and compact.
- Collapses the Start button's native label and labeled width.
- Preserves native indicators, progress, badges, overlays, highlights,
  animations, tray, clock, flyouts, and hit testing.
- Does nothing when the taskbar is horizontal.
- Restores the Windows grouping preference when disabled.

## Design

On the target machine, Windows already supplies the requested native 48-DIP
vertical width. Runtime measurement confirms a 48-logical-pixel taskbar window
and a matching 72-physical-pixel appbar reservation at 150% DPI.

Earlier versions attempted to override internal frame and `HasLabel` paths.
Those values have different semantics in Microsoft's new native vertical
implementation and caused the taskbar XAML content to collapse. The current
version never changes frame sizes or `HasLabel`. It forces native `Never
combine`, then hooks `TaskListButton::UpdateVisualStates` only to collapse the
existing `LabelControl` and its grid column after Windows finishes updating the
button. Running/progress indicators and every other visual-state child are
untouched. `ExperienceToggleButton::UpdateVisualStates` performs the equivalent
label-only change for the Start button. Explorer remains solely responsible for
the outer HWND, appbar reservation, and all internal frame dimensions.

## Current target

- Windows 11 Pro 25H2, build 26200.9278, x64
- Windhawk 2.0.0-alpha.2
- Native taskbar position: Left or Right

## Installation

1. Disable **Taskbar Labels for Windows 11**.
2. In Windhawk, create a local mod or edit the existing local mod.
3. Replace the editor contents with
   [`native-vertical-taskbar-width.wh.cpp`](native-vertical-taskbar-width.wh.cpp).
4. Compile the mod, exit editing mode, and enable it.
5. Restart Explorer once if existing buttons don't rebuild immediately.

Disable or remove the mod to restore the grouping preference stored in Windows.

## Origin and license

The grouping-settings hook is adapted from m417z's
[`taskbar-labels`](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-labels.wh.cpp)
mod. This derivative is licensed under GPL-3.0.
