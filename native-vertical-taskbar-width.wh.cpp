// ==WindhawkMod==
// @id              native-vertical-taskbar-width
// @name            Native Compact Vertical Taskbar
// @description     Keep native Windows 11 vertical taskbar buttons compact and separate without restyling native indicators.
// @version         1.0
// @author          kamkie
// @github          https://github.com/kamkie
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -lshcore
// @license         GPL-3.0
// ==/WindhawkMod==

// The grouping-settings hook is adapted from m417z's taskbar-labels mod:
// https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-labels.wh.cpp

// ==WindhawkModReadme==
/*
# Native Compact Vertical Taskbar

Keeps running windows as compact, separate buttons on Microsoft's native
Windows 11 left/right taskbar.

On the target machine, Windows already supplies the desired native 48-DIP
vertical width. Enabling native **Never combine** exposes labels and expands the
taskbar, so this mod collapses only the native `LabelControl` column after
Windows updates a button's visual state. The native Start button's label and
explicit labeled width are handled through its own visual-state callback.

This preserves Microsoft's native taskbar frame, icons, running indicators,
progress, badges, highlights, animations, tray, clock, flyouts, and hit
testing. It never changes `HasLabel`, TaskbarConfiguration/SystemTray frame
sizes, outer HWND/appbar geometry, indicator elements, padding, badges, or
button backgrounds. It does nothing while the taskbar is at the top or bottom.

Disable the broader **Taskbar Labels for Windows 11** mod while using this one.
Disabling or removing this mod restores the grouping preference stored in
Windows settings.

Target: Windows 11 25H2 build 26200.9278 (x64), Windhawk 2.0.0-alpha.2.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>

#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <atomic>

using namespace winrt::Windows::UI::Xaml;

std::atomic<bool> g_unloading;
std::atomic<bool> g_taskbarViewHooked;
std::atomic<int> g_lastLoggedEdge{-1};
std::atomic<bool> g_loggedGroupingOverride;
std::atomic<bool> g_loggedLabelCollapse;
std::atomic<bool> g_loggedStartCollapse;
std::atomic<bool> g_loggedFrameClamp;
std::atomic<bool> g_loggedWidthOverride;

constexpr int kVerticalTaskbarWidth = 48;

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
    if (!GetClassName(window, className, ARRAYSIZE(className))) {
        return false;
    }

    return _wcsicmp(className, L"Shell_TrayWnd") == 0 ||
           _wcsicmp(className, L"Shell_SecondaryTrayWnd") == 0;
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
        dpiX = 96;
    }

    LONG originalWidth = size->cx;
    size->cx = MulDiv(kVerticalTaskbarWidth, dpiX, 96);

    if (!g_loggedWidthOverride.exchange(true)) {
        Wh_Log(L"Setting compact outer width: %dpx->%dpx", originalWidth,
               size->cx);
    }
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
        Wh_Log(L"Failed to hook TrayUI::GetMinSize");
        return false;
    }

    return true;
}

FrameworkElement FindChildByName(FrameworkElement element, PCWSTR name) {
    int childCount = Media::VisualTreeHelper::GetChildrenCount(element);
    for (int i = 0; i < childCount; i++) {
        auto child = Media::VisualTreeHelper::GetChild(element, i)
                         .try_as<FrameworkElement>();
        if (child && child.Name() == name) {
            return child;
        }
    }

    return nullptr;
}

FrameworkElement FindDescendantByName(FrameworkElement element, PCWSTR name) {
    if (auto child = FindChildByName(element, name)) {
        return child;
    }

    int childCount = Media::VisualTreeHelper::GetChildrenCount(element);
    for (int i = 0; i < childCount; i++) {
        auto child = Media::VisualTreeHelper::GetChild(element, i)
                         .try_as<FrameworkElement>();
        if (child) {
            if (auto result = FindDescendantByName(child, name)) {
                return result;
            }
        }
    }

    return nullptr;
}

void ApplyCompactRepeaterState(FrameworkElement child) {
    FrameworkElement current = child;
    for (int i = 0; i < 6 && current; i++) {
        current = Media::VisualTreeHelper::GetParent(current)
                      .try_as<FrameworkElement>();
        if (!current || current.Name() != L"TaskbarFrameRepeater") {
            continue;
        }

        if (g_unloading) {
            current.ClearValue(FrameworkElement::WidthProperty());
            current.ClearValue(FrameworkElement::MinWidthProperty());
            current.ClearValue(FrameworkElement::MaxWidthProperty());
            current.ClearValue(FrameworkElement::HorizontalAlignmentProperty());
        } else {
            current.MinWidth(0);
            current.MaxWidth(48);
            current.Width(48);
            current.HorizontalAlignment(HorizontalAlignment::Left);
        }
        return;
    }
}

void ApplyCompactLabelState(FrameworkElement taskListButton) {
    if (!IsNativeVerticalTaskbar()) {
        return;
    }

    ApplyCompactRepeaterState(taskListButton);

    auto iconPanel = FindChildByName(taskListButton, L"IconPanel");
    auto iconPanelGrid = iconPanel.try_as<Controls::Grid>();
    if (!iconPanelGrid) {
        return;
    }

    auto columns = iconPanelGrid.ColumnDefinitions();
    if (columns.Size() != 2) {
        return;
    }

    auto labelControl =
        FindChildByName(iconPanel, L"LabelControl").try_as<Controls::TextBlock>();
    if (!labelControl) {
        return;
    }

    if (g_unloading) {
        taskListButton.ClearValue(FrameworkElement::WidthProperty());
        taskListButton.ClearValue(FrameworkElement::MinWidthProperty());
        taskListButton.ClearValue(FrameworkElement::MaxWidthProperty());
        columns.GetAt(1).Width(GridLength{
            .Value = 1,
            .GridUnitType = GridUnitType::Auto,
        });
        labelControl.Visibility(Visibility::Visible);
        return;
    }

    // Change only the native label column. Running/progress indicators and all
    // other visual-state children remain owned by Windows.
    taskListButton.MinWidth(0);
    taskListButton.MaxWidth(48);
    taskListButton.Width(48);
    labelControl.Visibility(Visibility::Collapsed);
    columns.GetAt(1).Width(GridLength{
        .Value = 0,
        .GridUnitType = GridUnitType::Pixel,
    });

    if (!g_loggedLabelCollapse.exchange(true)) {
        Wh_Log(L"Collapsed the native LabelControl column and button width");
    }
}

bool IsTaskbarFrameSize(int taskbarSize) {
    return taskbarSize == 1 || taskbarSize == 2;
}

using TaskbarConfiguration_GetFrameSize_t = double(WINAPI*)(int taskbarSize);
TaskbarConfiguration_GetFrameSize_t TaskbarConfiguration_GetFrameSize_Original;

double WINAPI TaskbarConfiguration_GetFrameSize_Hook(int taskbarSize) {
    double original = TaskbarConfiguration_GetFrameSize_Original(taskbarSize);

    // Only clamp the expanded labeled frame back to the compact native size.
    // The native compact value passes through untouched, so this never
    // undersizes a frame the way forcing a fixed value did in v0.3.
    if (g_unloading || !IsTaskbarFrameSize(taskbarSize) ||
        original <= kVerticalTaskbarWidth || !IsNativeVerticalTaskbar()) {
        return original;
    }

    if (!g_loggedFrameClamp.exchange(true)) {
        Wh_Log(L"Clamping labeled frame size: %.1fDIP->%dDIP", original,
               kVerticalTaskbarWidth);
    }

    return kVerticalTaskbarWidth;
}

using TaskListButton_UpdateVisualStates_t = void(WINAPI*)(void* pThis);
TaskListButton_UpdateVisualStates_t TaskListButton_UpdateVisualStates_Original;

void WINAPI TaskListButton_UpdateVisualStates_Hook(void* pThis) {
    TaskListButton_UpdateVisualStates_Original(pThis);

    void* taskListButtonIUnknownPtr = static_cast<void**>(pThis) + 3;
    winrt::Windows::Foundation::IUnknown taskListButtonIUnknown;
    winrt::copy_from_abi(taskListButtonIUnknown, taskListButtonIUnknownPtr);

    ApplyCompactLabelState(taskListButtonIUnknown.as<FrameworkElement>());
}

void ApplyCompactStartButtonState(FrameworkElement toggleButton) {
    if (!IsNativeVerticalTaskbar() ||
        winrt::get_class_name(toggleButton) !=
            L"Taskbar.ExperienceToggleButton" ||
        Automation::AutomationProperties::GetAutomationId(toggleButton) !=
            L"StartButton") {
        return;
    }

    ApplyCompactRepeaterState(toggleButton);

    auto rootPanel =
        FindChildByName(toggleButton, L"ExperienceToggleButtonRootPanel")
            .try_as<Controls::Grid>();
    if (!rootPanel) {
        return;
    }

    auto labelControl = FindDescendantByName(rootPanel, L"LabelControl");

    if (g_unloading) {
        rootPanel.ClearValue(FrameworkElement::WidthProperty());
        if (labelControl) {
            labelControl.ClearValue(UIElement::VisibilityProperty());
        }
        return;
    }

    rootPanel.Width(48);
    if (labelControl) {
        labelControl.Visibility(Visibility::Collapsed);
    }

    if (!g_loggedStartCollapse.exchange(true)) {
        Wh_Log(L"Collapsed the native Start label and width");
    }
}

using ExperienceToggleButton_UpdateVisualStates_t =
    void(WINAPI*)(void* pThis);
ExperienceToggleButton_UpdateVisualStates_t
    ExperienceToggleButton_UpdateVisualStates_Original;

void WINAPI ExperienceToggleButton_UpdateVisualStates_Hook(void* pThis) {
    ExperienceToggleButton_UpdateVisualStates_Original(pThis);

    void* toggleButtonIUnknownPtr = static_cast<void**>(pThis) + 2;
    winrt::Windows::Foundation::IUnknown toggleButtonIUnknown;
    winrt::copy_from_abi(toggleButtonIUnknown, toggleButtonIUnknownPtr);

    ApplyCompactStartButtonState(toggleButtonIUnknown.as<FrameworkElement>());
}

HMODULE GetTaskbarViewModule() {
    HMODULE module = GetModuleHandle(L"Taskbar.View.dll");
    if (!module) {
        module = GetModuleHandle(L"ExplorerExtensions.dll");
    }
    return module;
}

bool HookTaskbarViewSymbols(HMODULE module) {
    WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskListButton::UpdateVisualStates(void))"},
            &TaskListButton_UpdateVisualStates_Original,
            TaskListButton_UpdateVisualStates_Hook,
        },
        {
            {LR"(protected: virtual void __cdecl winrt::Taskbar::implementation::ExperienceToggleButton::UpdateVisualStates(void))"},
            &ExperienceToggleButton_UpdateVisualStates_Original,
            ExperienceToggleButton_UpdateVisualStates_Hook,
        },
        {
            {LR"(public: static double __cdecl winrt::Taskbar::implementation::TaskbarConfiguration::GetFrameSize(enum winrt::WindowsUdk::UI::Shell::TaskbarSize))"},
            &TaskbarConfiguration_GetFrameSize_Original,
            TaskbarConfiguration_GetFrameSize_Hook,
        },
    };

    if (!WindhawkUtils::HookSymbols(module, hooks, ARRAYSIZE(hooks))) {
        Wh_Log(L"Failed to hook TaskListButton::UpdateVisualStates");
        return false;
    }

    Wh_Log(L"Hooked native TaskListButton label presentation");
    return true;
}

using LoadLibraryExW_t = decltype(&LoadLibraryExW);
LoadLibraryExW_t LoadLibraryExW_Original;

HMODULE WINAPI LoadLibraryExW_Hook(LPCWSTR fileName,
                                   HANDLE file,
                                   DWORD flags) {
    HMODULE module = LoadLibraryExW_Original(fileName, file, flags);
    if (module && !g_taskbarViewHooked && GetTaskbarViewModule() == module &&
        !g_taskbarViewHooked.exchange(true)) {
        Wh_Log(L"Taskbar view module loaded: %s", fileName);
        if (HookTaskbarViewSymbols(module)) {
            Wh_ApplyHookOperations();
        }
    }

    return module;
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

    int width = MulDiv(kVerticalTaskbarWidth, dpi, 96);
    if (data->uEdge == ABE_LEFT) {
        data->rc.right = data->rc.left + width;
    } else {
        data->rc.left = data->rc.right - width;
    }

    return result;
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
    Wh_Log(L"Initializing compact native vertical taskbar behavior");

    if (!HookTaskbarDllSymbols()) {
        return FALSE;
    }

    bool delayedTaskbarViewHookNeeded = false;
    if (HMODULE taskbarView = GetTaskbarViewModule()) {
        g_taskbarViewHooked = true;
        if (!HookTaskbarViewSymbols(taskbarView)) {
            return FALSE;
        }
    } else {
        Wh_Log(L"Taskbar.View.dll isn't loaded yet");
        delayedTaskbarViewHookNeeded = true;
    }

    HMODULE kernelBase = GetModuleHandle(L"kernelbase.dll");
    auto regGetValueW = reinterpret_cast<RegGetValueW_t>(
        GetProcAddress(kernelBase, "RegGetValueW"));
    if (!regGetValueW ||
        !WindhawkUtils::SetFunctionHook(regGetValueW, RegGetValueW_Hook,
                                        &RegGetValueW_Original)) {
        Wh_Log(L"Failed to hook native taskbar grouping settings");
        return FALSE;
    }

    if (!WindhawkUtils::SetFunctionHook(SHAppBarMessage,
                                        SHAppBarMessage_Hook,
                                        &SHAppBarMessage_Original)) {
        Wh_Log(L"Failed to hook SHAppBarMessage");
        return FALSE;
    }

    if (delayedTaskbarViewHookNeeded) {
        auto loadLibraryExW = reinterpret_cast<LoadLibraryExW_t>(
            GetProcAddress(kernelBase, "LoadLibraryExW"));
        if (!loadLibraryExW ||
            !WindhawkUtils::SetFunctionHook(loadLibraryExW,
                                            LoadLibraryExW_Hook,
                                            &LoadLibraryExW_Original)) {
            Wh_Log(L"Failed to hook delayed Taskbar.View.dll loading");
            return FALSE;
        }
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
