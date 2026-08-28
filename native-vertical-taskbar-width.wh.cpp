// ==WindhawkMod==
// @id              native-vertical-taskbar-width
// @name            Native Compact Vertical Taskbar
// @description     Keep native Windows 11 vertical taskbar buttons separate without replacing or restyling the taskbar.
// @version         0.5
// @author          kamkie
// @github          https://github.com/kamkie
// @include         explorer.exe
// @architecture    x86-64
// @license         GPL-3.0
// ==/WindhawkMod==

// The grouping-settings hook is adapted from m417z's taskbar-labels mod:
// https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-labels.wh.cpp

// ==WindhawkModReadme==
/*
# Native Compact Vertical Taskbar

Keeps running windows as separate buttons on Microsoft's native Windows 11
left/right taskbar.

On the target machine, Windows already supplies the desired native 48-DIP
vertical width and hides labels in the vertical layout. This mod deliberately
does not override frame dimensions or label/XAML state. It only makes Explorer
use its native **Never combine** mode while the taskbar is vertical.

This preserves Microsoft's native taskbar frame, icons, running indicators,
progress, badges, highlights, animations, tray, clock, flyouts, and hit
testing. It does nothing while the taskbar is at the top or bottom.

Disable the broader **Taskbar Labels for Windows 11** mod while using this one.
Disabling or removing this mod restores the grouping preference stored in
Windows settings.

Target: Windows 11 25H2 build 26200.9278 (x64), Windhawk 1.7.3.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>

#include <atomic>

std::atomic<bool> g_unloading;
std::atomic<int> g_lastLoggedEdge{-1};
std::atomic<bool> g_loggedGroupingOverride;

const wchar_t* EdgeName(UINT edge) {
    switch (edge) {
        case ABE_LEFT:
            return L"left";
        case ABE_TOP:
            return L"top";
        case ABE_RIGHT:
            return L"right";
        case ABE_BOTTOM:
            return L"bottom";
        default:
            return L"unknown";
    }
}

bool GetNativeTaskbarEdge(UINT* edge) {
    APPBARDATA appBarData{
        .cbSize = sizeof(APPBARDATA),
    };

    if (!SHAppBarMessage(ABM_GETTASKBARPOS, &appBarData)) {
        Wh_Log(L"SHAppBarMessage(ABM_GETTASKBARPOS) failed");
        return false;
    }

    *edge = appBarData.uEdge;

    int previous = g_lastLoggedEdge.exchange(static_cast<int>(*edge));
    if (previous != static_cast<int>(*edge)) {
        Wh_Log(L"Detected native taskbar orientation: %s (ABE_%u)",
               EdgeName(*edge), *edge);
        g_loggedGroupingOverride = false;
    }

    return true;
}

bool IsNativeVerticalTaskbar() {
    UINT edge;
    return GetNativeTaskbarEdge(&edge) &&
           (edge == ABE_LEFT || edge == ABE_RIGHT);
}

bool IsCurrentProcessTaskbarWindow(HWND window) {
    if (!window) {
        return false;
    }

    DWORD processId = 0;
    if (!GetWindowThreadProcessId(window, &processId) ||
        processId != GetCurrentProcessId()) {
        return false;
    }

    wchar_t className[32];
    return GetClassName(window, className, ARRAYSIZE(className)) &&
           _wcsicmp(className, L"Shell_TrayWnd") == 0;
}

using RegGetValueW_t = decltype(&RegGetValueW);
RegGetValueW_t RegGetValueW_Original;

LONG WINAPI RegGetValueW_Hook(HKEY key,
                              LPCWSTR subKey,
                              LPCWSTR valueName,
                              DWORD flags,
                              LPDWORD type,
                              PVOID data,
                              LPDWORD dataSize) {
    LONG result = RegGetValueW_Original(key, subKey, valueName, flags, type,
                                       data, dataSize);

    if (g_unloading || key != HKEY_CURRENT_USER || !subKey || !valueName ||
        _wcsicmp(
            subKey,
            LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Advanced)") !=
            0 ||
        (_wcsicmp(valueName, L"TaskbarGlomLevel") != 0 &&
         _wcsicmp(valueName, L"MMTaskbarGlomLevel") != 0) ||
        flags != RRF_RT_REG_DWORD || !data || !dataSize ||
        *dataSize != sizeof(DWORD) || !IsNativeVerticalTaskbar()) {
        return result;
    }

    DWORD original = result == ERROR_SUCCESS ? *static_cast<DWORD*>(data) : 0;

    // 0 = Always combine, 2 = Never combine.
    constexpr DWORD finalValue = 2;
    *static_cast<DWORD*>(data) = finalValue;

    if (type) {
        *type = REG_DWORD;
    }

    if (!g_loggedGroupingOverride.exchange(true)) {
        Wh_Log(L"Forcing %s to native Never combine: %u->%u", valueName,
               original, finalValue);
    }

    return ERROR_SUCCESS;
}

void RequestTaskbarBehaviorRefresh() {
    HWND taskbarWindow = FindWindow(L"Shell_TrayWnd", nullptr);
    if (!IsCurrentProcessTaskbarWindow(taskbarWindow)) {
        Wh_Log(L"Primary native taskbar window wasn't found");
        return;
    }

    SendMessage(taskbarWindow, WM_SETTINGCHANGE, 0, 0);
}

BOOL Wh_ModInit() {
    Wh_Log(L"Initializing native vertical Never combine behavior");

    HMODULE kernelBase = GetModuleHandle(L"kernelbase.dll");
    auto regGetValueW = reinterpret_cast<RegGetValueW_t>(
        GetProcAddress(kernelBase, "RegGetValueW"));
    if (!regGetValueW ||
        !WindhawkUtils::SetFunctionHook(regGetValueW, RegGetValueW_Hook,
                                        &RegGetValueW_Original)) {
        Wh_Log(L"Failed to hook native taskbar grouping settings");
        return FALSE;
    }

    return TRUE;
}

void Wh_ModAfterInit() {
    RequestTaskbarBehaviorRefresh();
}

void Wh_ModBeforeUninit() {
    Wh_Log(L"Restoring the Windows grouping preference");
    g_unloading = true;
    RequestTaskbarBehaviorRefresh();
}
