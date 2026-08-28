# Attribution and modifications

This project is derived from `taskbar-grouping` version 1.3.10 by Michael
Maltsev (`m417z`), published in the
[Windhawk mods repository](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-grouping.wh.cpp)
under the GNU General Public License version 3.

Modified by `kamkie` on 2026-08-28 to provide a fixed, settings-free behavior
for Microsoft's native Windows 11 vertical taskbar:

- renamed and repackaged as **Native Taskbar Ungrouping**;
- fixed pinned-item, placement, icon, grouping, and legacy-taskbar choices;
- removed the public settings schema and settings loading;
- retained native Windows combine-mode presentation while splitting task-model
  groups per window;
- updated deprecated Windhawk hook-wrapper calls for Windhawk 2.0.

The complete work remains licensed under GPL-3.0.
