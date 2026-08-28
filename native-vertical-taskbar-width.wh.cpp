// ==WindhawkMod==
// @id              native-vertical-taskbar-width
// @name            Native Vertical Taskbar Width
// @description     Adjust the width of the native Windows 11 left/right taskbar.
// @version         0.1
// @author          kamkie
// @github          https://github.com/kamkie
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -DWINVER=0x0A00 -lshcore
// @license         GPL-3.0
// ==/WindhawkMod==

// Adapted from the taskbar frame-sizing code in m417z's
// taskbar-icon-size Windhawk mod:
// https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-icon-size.wh.cpp

// ==WindhawkModReadme==
/*
# Native Vertical Taskbar Width

Adjusts only the thickness of Microsoft's native Windows 11 vertical taskbar.
Windows continues to own taskbar orientation, layout, buttons, labels,
grouping, running indicators, progress, badges, animations, tray, and flyouts.

This personal mod is tested against Windows 11 25H2 build 26200.9278 (x64),
Taskbar.View.dll 2607.28001.200.0, SystemTray.dll 2607.28000.0.0, and Windhawk
1.7.3. It deliberately does nothing while the native taskbar is at the top or
bottom.

## Installation

1. In Windhawk, enable developer mode in Settings if it isn't already enabled.
2. Choose **Create a new mod**.
3. Replace the editor contents with this file and choose **Compile Mod**.
4. Exit editing mode and enable the mod.

Change **Vertical taskbar width** in the mod settings. Disable or remove the mod
normally in Windhawk to restore the native taskbar dimensions.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- taskbarWidth: 48
  $name: Vertical taskbar width
  $description: Width of the native Windows 11 left/right taskbar in logical pixels.
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <atomic>
#include <algorithm>
#include <cstdint>

struct {
    int taskbarWidth;
} g_settings;

std::atomic<bool> g_unloading;
std::atomic<bool> g_taskbarViewHooked;
std::atomic<bool> g_systemTrayHooked;
std::atomic<unsigned> g_logEpoch{1};
std::atomic<int> g_lastLoggedEdge{-1};

struct LogGate {
    std::atomic<unsigned> epoch{};
};

LogGate g_trayMinSizeLogGate;
LogGate g_taskbarFrameLogGate;
LogGate g_systemTrayFrameLogGate;
LogGate g_secondaryTrayFrameLogGate;
LogGate g_appBarLogGate;

WINUSERAPI UINT WINAPI GetDpiForWindow(HWND hwnd);

typedef enum MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

STDAPI GetDpiForMonitor(HMONITOR monitor,
                        MONITOR_DPI_TYPE dpiType,
                        UINT* dpiX,
                        UINT* dpiY);

using SHAppBarMessage_t = decltype(&SHAppBarMessage);
SHAppBarMessage_t SHAppBarMessage_Original;

int RequestedWidth() {
    return std::max(g_settings.taskbarWidth, 2);
}

bool ShouldLog(LogGate& gate) {
    unsigned epoch = g_logEpoch.load();
    return gate.epoch.exchange(epoch) != epoch;
}

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

    auto appBarMessage =
        SHAppBarMessage_Original ? SHAppBarMessage_Original : SHAppBarMessage;
    if (!appBarMessage(ABM_GETTASKBARPOS, &appBarData)) {
        Wh_Log(L"SHAppBarMessage(ABM_GETTASKBARPOS) failed");
        return false;
    }

    *edge = appBarData.uEdge;

    int previous = g_lastLoggedEdge.exchange(static_cast<int>(*edge));
    if (previous != static_cast<int>(*edge)) {
        Wh_Log(L"Detected native taskbar orientation: %s (ABE_%u)",
               EdgeName(*edge), *edge);
        g_logEpoch.fetch_add(1);
    }

    return true;
}

bool IsNativeVerticalTaskbar(UINT* edge = nullptr) {
    UINT taskbarEdge;
    if (!GetNativeTaskbarEdge(&taskbarEdge)) {
        return false;
    }

    if (edge) {
        *edge = taskbarEdge;
    }

    return taskbarEdge == ABE_LEFT || taskbarEdge == ABE_RIGHT;
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
    if (!GetClassName(window, className, ARRAYSIZE(className))) {
        return false;
    }

    return _wcsicmp(className, L"Shell_TrayWnd") == 0 ||
           _wcsicmp(className, L"Shell_SecondaryTrayWnd") == 0;
}

void LoadSettings() {
    g_settings.taskbarWidth = Wh_GetIntSetting(L"taskbarWidth");
    Wh_Log(L"Requested vertical taskbar width: %d DIPs",
           RequestedWidth());
}

using TrayUI_GetMinSize_t = void(WINAPI*)(void* pThis,
                                          HMONITOR monitor,
                                          SIZE* size);
TrayUI_GetMinSize_t TrayUI_GetMinSize_Original;

void WINAPI TrayUI_GetMinSize_Hook(void* pThis,
                                   HMONITOR monitor,
                                   SIZE* size) {
    TrayUI_GetMinSize_Original(pThis, monitor, size);

    if (g_unloading || !IsNativeVerticalTaskbar()) {
        return;
    }

    UINT dpiX = 96;
    UINT dpiY = 96;
    if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        Wh_Log(L"GetDpiForMonitor failed; using 96 DPI");
        dpiX = 96;
    }

    LONG originalWidth = size->cx;
    size->cx = MulDiv(RequestedWidth(), dpiX, 96);

    if (ShouldLog(g_trayMinSizeLogGate)) {
        MONITORINFO monitorInfo{
            .cbSize = sizeof(MONITORINFO),
        };
        GetMonitorInfo(monitor, &monitorInfo);
        Wh_Log(L"TrayUI::GetMinSize monitor=%p rect=(%d,%d)-(%d,%d) "
               L"dpi=%u originalWidth=%dpx requested=%dDIP finalWidth=%dpx",
               monitor, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
               monitorInfo.rcMonitor.right, monitorInfo.rcMonitor.bottom, dpiX,
               originalWidth, RequestedWidth(), size->cx);
    }
}

bool IsTaskbarFrameSize(int taskbarSize) {
    return taskbarSize == 1 || taskbarSize == 2;
}

using TaskbarConfiguration_GetFrameSize_t =
    double(WINAPI*)(int taskbarSize);
TaskbarConfiguration_GetFrameSize_t
    TaskbarConfiguration_GetFrameSize_Original;

double WINAPI TaskbarConfiguration_GetFrameSize_Hook(int taskbarSize) {
    double original =
        TaskbarConfiguration_GetFrameSize_Original(taskbarSize);

    if (g_unloading || !IsTaskbarFrameSize(taskbarSize) ||
        !IsNativeVerticalTaskbar()) {
        return original;
    }

    if (ShouldLog(g_taskbarFrameLogGate)) {
        Wh_Log(L"TaskbarConfiguration::GetFrameSize size=%d "
               L"original=%.1fDIP final=%dDIP",
               taskbarSize, original, RequestedWidth());
    }

    return RequestedWidth();
}

using SystemTrayController_GetFrameSize_t =
    double(WINAPI*)(void* pThis, int taskbarSize);
SystemTrayController_GetFrameSize_t
    SystemTrayController_GetFrameSize_Original;

double WINAPI SystemTrayController_GetFrameSize_Hook(void* pThis,
                                                     int taskbarSize) {
    double original =
        SystemTrayController_GetFrameSize_Original(pThis, taskbarSize);

    if (g_unloading || !IsTaskbarFrameSize(taskbarSize) ||
        !IsNativeVerticalTaskbar()) {
        return original;
    }

    if (ShouldLog(g_systemTrayFrameLogGate)) {
        Wh_Log(L"SystemTrayController::GetFrameSize size=%d "
               L"original=%.1fDIP final=%dDIP",
               taskbarSize, original, RequestedWidth());
    }

    return RequestedWidth();
}

using SystemTraySecondaryController_GetFrameSize_t =
    double(WINAPI*)(void* pThis, int taskbarSize);
SystemTraySecondaryController_GetFrameSize_t
    SystemTraySecondaryController_GetFrameSize_Original;

double WINAPI SystemTraySecondaryController_GetFrameSize_Hook(
    void* pThis,
    int taskbarSize) {
    double original =
        SystemTraySecondaryController_GetFrameSize_Original(pThis,
                                                            taskbarSize);

    if (g_unloading || !IsTaskbarFrameSize(taskbarSize) ||
        !IsNativeVerticalTaskbar()) {
        return original;
    }

    if (ShouldLog(g_secondaryTrayFrameLogGate)) {
        Wh_Log(L"SystemTraySecondaryController::GetFrameSize size=%d "
               L"original=%.1fDIP final=%dDIP",
               taskbarSize, original, RequestedWidth());
    }

    return RequestedWidth();
}

auto WINAPI SHAppBarMessage_Hook(DWORD message, PAPPBARDATA data) {
    auto result = SHAppBarMessage_Original(message, data);

    if (g_unloading || message != ABM_QUERYPOS || !result || !data ||
        !IsCurrentProcessTaskbarWindow(data->hWnd) ||
        (data->uEdge != ABE_LEFT && data->uEdge != ABE_RIGHT)) {
        return result;
    }

    UINT dpi = GetDpiForWindow(data->hWnd);
    if (!dpi) {
        dpi = 96;
    }

    int width = MulDiv(RequestedWidth(), dpi, 96);
    RECT originalRect = data->rc;

    if (data->uEdge == ABE_LEFT) {
        data->rc.right = data->rc.left + width;
    } else {
        data->rc.left = data->rc.right - width;
    }

    if (ShouldLog(g_appBarLogGate)) {
        Wh_Log(L"SHAppBarMessage(ABM_QUERYPOS) hwnd=%p edge=%s dpi=%u "
               L"original=(%d,%d)-(%d,%d) final=(%d,%d)-(%d,%d)",
               data->hWnd, EdgeName(data->uEdge), dpi, originalRect.left,
               originalRect.top, originalRect.right, originalRect.bottom,
               data->rc.left, data->rc.top, data->rc.right, data->rc.bottom);
    }

    return result;
}

HMODULE GetTaskbarViewModule() {
    HMODULE module = GetModuleHandle(L"Taskbar.View.dll");
    if (!module) {
        module = GetModuleHandle(L"ExplorerExtensions.dll");
    }
    return module;
}

HMODULE GetSystemTrayModule() {
    return GetModuleHandle(L"SystemTray.dll");
}

bool HookTaskbarDllSymbols() {
    HMODULE module =
        LoadLibraryEx(L"taskbar.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) {
        Wh_Log(L"Failed to load taskbar.dll");
        return false;
    }

    WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {
            {LR"(public: virtual void __cdecl TrayUI::GetMinSize(struct HMONITOR__ *,struct tagSIZE *))"},
            &TrayUI_GetMinSize_Original,
            TrayUI_GetMinSize_Hook,
        },
    };

    if (!WindhawkUtils::HookSymbols(module, hooks, ARRAYSIZE(hooks))) {
        Wh_Log(L"Failed to hook taskbar.dll frame sizing symbols");
        return false;
    }

    Wh_Log(L"Hooked taskbar.dll TrayUI frame sizing");
    return true;
}

bool HookTaskbarViewSymbols(HMODULE module) {
    WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {
            {LR"(public: static double __cdecl winrt::Taskbar::implementation::TaskbarConfiguration::GetFrameSize(enum winrt::WindowsUdk::UI::Shell::TaskbarSize))"},
            &TaskbarConfiguration_GetFrameSize_Original,
            TaskbarConfiguration_GetFrameSize_Hook,
        },
    };

    if (!WindhawkUtils::HookSymbols(module, hooks, ARRAYSIZE(hooks))) {
        Wh_Log(L"Failed to hook Taskbar.View.dll frame sizing symbols");
        return false;
    }

    Wh_Log(L"Hooked Taskbar.View.dll frame sizing");
    return true;
}

bool HookSystemTraySymbols(HMODULE module) {
    WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {
            {LR"(private: double __cdecl winrt::SystemTray::implementation::SystemTrayController::GetFrameSize(enum winrt::WindowsUdk::UI::Shell::TaskbarSize))"},
            &SystemTrayController_GetFrameSize_Original,
            SystemTrayController_GetFrameSize_Hook,
        },
        {
            {LR"(private: double __cdecl winrt::SystemTray::implementation::SystemTraySecondaryController::GetFrameSize(enum winrt::WindowsUdk::UI::Shell::TaskbarSize))"},
            &SystemTraySecondaryController_GetFrameSize_Original,
            SystemTraySecondaryController_GetFrameSize_Hook,
            true,
        },
    };

    if (!WindhawkUtils::HookSymbols(module, hooks, ARRAYSIZE(hooks))) {
        Wh_Log(L"Failed to hook SystemTray.dll frame sizing symbols");
        return false;
    }

    Wh_Log(L"Hooked SystemTray.dll frame sizing");
    return true;
}

using LoadLibraryExW_t = decltype(&LoadLibraryExW);
LoadLibraryExW_t LoadLibraryExW_Original;

HMODULE WINAPI LoadLibraryExW_Hook(LPCWSTR fileName,
                                   HANDLE file,
                                   DWORD flags) {
    HMODULE module = LoadLibraryExW_Original(fileName, file, flags);
    if (!module) {
        return module;
    }

    if (!g_taskbarViewHooked && GetTaskbarViewModule() == module &&
        !g_taskbarViewHooked.exchange(true)) {
        Wh_Log(L"Taskbar view module loaded: %s", fileName);
        if (HookTaskbarViewSymbols(module)) {
            Wh_ApplyHookOperations();
        }
    }

    if (!g_systemTrayHooked && GetSystemTrayModule() == module &&
        !g_systemTrayHooked.exchange(true)) {
        Wh_Log(L"System tray module loaded: %s", fileName);
        if (HookSystemTraySymbols(module)) {
            Wh_ApplyHookOperations();
        }
    }

    return module;
}

void RequestTaskbarLayout() {
    Wh_Log(L"Requesting native taskbar layout refresh");

    EnumWindows(
        [](HWND window, LPARAM) -> BOOL {
            if (IsCurrentProcessTaskbarWindow(window)) {
                DWORD_PTR ignored;
                SendMessageTimeout(window, WM_SETTINGCHANGE,
                                   SPI_SETLOGICALDPIOVERRIDE, 0,
                                   SMTO_ABORTIFHUNG, 2000, &ignored);
            }
            return TRUE;
        },
        0);
}

BOOL Wh_ModInit() {
    Wh_Log(L"Initializing for Windows 11 native vertical taskbar");
    LoadSettings();

    if (!HookTaskbarDllSymbols()) {
        return FALSE;
    }

    bool delayedModuleHookNeeded = false;

    if (HMODULE module = GetTaskbarViewModule()) {
        g_taskbarViewHooked = true;
        if (!HookTaskbarViewSymbols(module)) {
            return FALSE;
        }
    } else {
        Wh_Log(L"Taskbar.View.dll isn't loaded yet");
        delayedModuleHookNeeded = true;
    }

    if (HMODULE module = GetSystemTrayModule()) {
        g_systemTrayHooked = true;
        if (!HookSystemTraySymbols(module)) {
            return FALSE;
        }
    } else {
        Wh_Log(L"SystemTray.dll isn't loaded yet");
        delayedModuleHookNeeded = true;
    }

    if (!WindhawkUtils::SetFunctionHook(SHAppBarMessage,
                                        SHAppBarMessage_Hook,
                                        &SHAppBarMessage_Original)) {
        Wh_Log(L"Failed to hook SHAppBarMessage");
        return FALSE;
    }

    if (delayedModuleHookNeeded) {
        HMODULE kernelBase = GetModuleHandle(L"kernelbase.dll");
        auto loadLibraryExW = reinterpret_cast<LoadLibraryExW_t>(
            GetProcAddress(kernelBase, "LoadLibraryExW"));
        if (!loadLibraryExW ||
            !WindhawkUtils::SetFunctionHook(loadLibraryExW,
                                            LoadLibraryExW_Hook,
                                            &LoadLibraryExW_Original)) {
            Wh_Log(L"Failed to hook delayed taskbar module loading");
            return FALSE;
        }
    }

    return TRUE;
}

void Wh_ModAfterInit() {
    RequestTaskbarLayout();
}

void Wh_ModBeforeUninit() {
    Wh_Log(L"Restoring native taskbar dimensions");
    g_unloading = true;
    g_logEpoch.fetch_add(1);
    RequestTaskbarLayout();
}

void Wh_ModSettingsChanged() {
    LoadSettings();
    g_logEpoch.fetch_add(1);
    RequestTaskbarLayout();
}
