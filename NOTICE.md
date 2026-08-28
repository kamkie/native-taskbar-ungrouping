# Attribution and modifications

This project is derived from `taskbar-grouping` version 1.3.10 by Michael
Maltsev (`m417z`), published in the
[Windhawk mods repository](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-grouping.wh.cpp)
under the GNU General Public License version 3.

Modified by `kamkie` on 2026-08-28 and 2026-08-29 to provide a fixed,
settings-free behavior for Microsoft's native Windows 11 vertical taskbar:

- renamed and repackaged as **Native Taskbar Ungrouping**;
- retained the pinned-item identity, placement, icon, launch, and jump-list
  handling needed for separate native buttons;
- removed settings, exclusions, custom groups, dead branches, Windows 10, and
  ExplorerPatcher support;
- limited task-model splitting to the native left/right taskbar;
- added an in-memory combine-mode override that preserves the compact frame
  without changing the saved Windows preference;
- updated deprecated Windhawk hook-wrapper calls for Windhawk 2.0.

The complete work remains licensed under GPL-3.0.
