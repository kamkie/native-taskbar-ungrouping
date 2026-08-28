# Native Compact Vertical Taskbar

A private/personal Windhawk mod that makes Microsoft's native Windows 11
left/right taskbar compact:

- 48-DIP taskbar width by default.
- Labels always hidden.
- Running windows always kept as separate buttons.

The mod leaves Windows in charge of orientation, taskbar buttons,
running/open-window indicators, progress, badges, overlays, hover and pressed
states, tray, clock, flyouts, and hit testing. It doesn't emulate a vertical
taskbar and doesn't depend on the older Windhawk vertical-taskbar mod or the
broad **Taskbar Labels for Windows 11** mod.

## Current target

- Windows 11 Pro 25H2, build 26200.9278, x64
- `explorer.exe` 10.0.26100.8875
- `Taskbar.View.dll` 2607.28001.200.0
- `SystemTray.dll` 2607.28000.0.0
- Windhawk 1.7.3
- Primary monitor at 150% effective DPI

The source of the mod is
[`native-vertical-taskbar-width.wh.cpp`](native-vertical-taskbar-width.wh.cpp).

## Design

Three matching frame-size layers are adjusted only for `ABE_LEFT` and
`ABE_RIGHT`:

1. `TaskbarConfiguration::GetFrameSize` supplies the native taskbar XAML frame
   thickness.
2. `SystemTrayController::GetFrameSize` and its secondary-monitor counterpart
   supply the native tray frame thickness.
3. `TrayUI::GetMinSize`, plus Explorer's `ABM_QUERYPOS` appbar negotiation,
   supplies the DPI-scaled outer taskbar window and reserved work-area width.

For the two fixed behavioral changes, the mod makes Explorer read `Never`
combine and returns `HasLabel=false` from the native taskbar view model. No
taskbar-button or indicator visual state is hooked.

`SystemTray.dll` frame symbols are optional because these functions can be
inlined and Microsoft's PDB isn't available for every serviced build. Failure
to retrieve that PDB doesn't disable the Taskbar.View, TrayUI, label, or
grouping behavior.

## Install as a local mod

1. Open Windhawk and enable **Developer mode** in Windhawk Settings if needed.
2. Choose **Create a new mod**.
3. Replace the editor contents with
   `native-vertical-taskbar-width.wh.cpp`.
4. Choose **Compile Mod**, exit editing mode, and enable the mod.
5. Open the mod's settings to change **Vertical taskbar width**.

Disable **Taskbar Labels for Windows 11** while this mod is enabled; the compact
mod replaces the only two behaviors needed from it without its cosmetic
settings and visual modifications.

Disable or remove the mod in Windhawk to restore Microsoft's native dimensions,
labels, and grouping behavior.

## Origin and license

The frame-sizing approach is adapted from m417z's
[`taskbar-icon-size`](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-icon-size.wh.cpp)
mod. This derivative is licensed under GPL-3.0.
