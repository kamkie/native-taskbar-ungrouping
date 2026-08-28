# Native Vertical Taskbar Width

A private/personal Windhawk mod that changes only the thickness of Microsoft's
native Windows 11 left/right taskbar.

The mod leaves Windows in charge of orientation, taskbar buttons, labels,
grouping, running/open-window indicators, progress, badges, overlays, hover and
pressed states, tray, clock, flyouts, and hit testing. It doesn't emulate a
vertical taskbar and doesn't depend on the older Windhawk vertical-taskbar mod.

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

No taskbar-button or indicator visual state is hooked.

## Install as a local mod

1. Open Windhawk and enable **Developer mode** in Windhawk Settings if needed.
2. Choose **Create a new mod**.
3. Replace the editor contents with
   `native-vertical-taskbar-width.wh.cpp`.
4. Choose **Compile Mod**, exit editing mode, and enable the mod.
5. Open the mod's settings to change **Vertical taskbar width**.

Disable or remove the mod in Windhawk to restore Microsoft's native dimensions.

## Origin and license

The frame-sizing approach is adapted from m417z's
[`taskbar-icon-size`](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-icon-size.wh.cpp)
mod. This derivative is licensed under GPL-3.0.
