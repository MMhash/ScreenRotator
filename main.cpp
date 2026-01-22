#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>
#include <shlwapi.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WM_TRAYICON (WM_USER + 1)

NOTIFYICONDATA nid;
HMENU hPopMenu;
HWND hMainWnd;
WCHAR szIniFile[MAX_PATH];

HBRUSH hBlackBrush;
HBRUSH hInputDarkBrush;
HBRUSH hWhiteBrush;

WNDPROC wpOrigEditProc;

UINT modUp = MOD_CONTROL | MOD_ALT; UINT vkUp = VK_UP;
UINT modDown = MOD_CONTROL | MOD_ALT; UINT vkDown = VK_DOWN;
UINT modLeft = MOD_CONTROL | MOD_ALT; UINT vkLeft = VK_LEFT;
UINT modRight = MOD_CONTROL | MOD_ALT; UINT vkRight = VK_RIGHT;

std::wstring FormatHotkeyString(UINT vk, UINT mod) {
    if (vk == 0) return L"None";

    std::wstring text = L"";
    if (mod & MOD_CONTROL) text += L"Ctrl + ";
    if (mod & MOD_SHIFT)   text += L"Shift + ";
    if (mod & MOD_ALT)     text += L"Alt + ";

    WCHAR keyName[128];
    UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);

    switch (vk) {
    case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
    case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
    case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
        scanCode |= 0x100;
        break;
    }

    if (GetKeyNameText(scanCode << 16, keyName, 128)) {
        text += keyName;
    }
    else {
        text += L"Unknown";
    }
    return text;
}

void GetIniPath() {
    GetModuleFileName(NULL, szIniFile, MAX_PATH);
    PathRemoveFileSpec(szIniFile);
    PathAppend(szIniFile, L"ScreenRotator.ini");
}

void LoadSettings() {
    GetIniPath();
    vkUp = GetPrivateProfileInt(L"Keys", L"UpVK", VK_UP, szIniFile);
    modUp = GetPrivateProfileInt(L"Keys", L"UpMod", MOD_CONTROL | MOD_ALT, szIniFile);

    vkDown = GetPrivateProfileInt(L"Keys", L"DownVK", VK_DOWN, szIniFile);
    modDown = GetPrivateProfileInt(L"Keys", L"DownMod", MOD_CONTROL | MOD_ALT, szIniFile);

    vkLeft = GetPrivateProfileInt(L"Keys", L"LeftVK", VK_LEFT, szIniFile);
    modLeft = GetPrivateProfileInt(L"Keys", L"LeftMod", MOD_CONTROL | MOD_ALT, szIniFile);

    vkRight = GetPrivateProfileInt(L"Keys", L"RightVK", VK_RIGHT, szIniFile);
    modRight = GetPrivateProfileInt(L"Keys", L"RightMod", MOD_CONTROL | MOD_ALT, szIniFile);
}

void SaveSettings() {
    WritePrivateProfileString(L"Keys", L"UpVK", std::to_wstring(vkUp).c_str(), szIniFile);
    WritePrivateProfileString(L"Keys", L"UpMod", std::to_wstring(modUp).c_str(), szIniFile);
    WritePrivateProfileString(L"Keys", L"DownVK", std::to_wstring(vkDown).c_str(), szIniFile);
    WritePrivateProfileString(L"Keys", L"DownMod", std::to_wstring(modDown).c_str(), szIniFile);
    WritePrivateProfileString(L"Keys", L"LeftVK", std::to_wstring(vkLeft).c_str(), szIniFile);
    WritePrivateProfileString(L"Keys", L"LeftMod", std::to_wstring(modLeft).c_str(), szIniFile);
    WritePrivateProfileString(L"Keys", L"RightVK", std::to_wstring(vkRight).c_str(), szIniFile);
    WritePrivateProfileString(L"Keys", L"RightMod", std::to_wstring(modRight).c_str(), szIniFile);
}

bool IsStartupEnabled() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR szPath[MAX_PATH];
        DWORD dwSize = sizeof(szPath);
        if (RegQueryValueEx(hKey, L"ScreenRotator", NULL, NULL, (LPBYTE)szPath, &dwSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }
        RegCloseKey(hKey);
    }
    return false;
}

void SetStartup(bool enable) {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            WCHAR szPath[MAX_PATH];
            GetModuleFileName(NULL, szPath, MAX_PATH);
            RegSetValueEx(hKey, L"ScreenRotator", 0, REG_SZ, (LPBYTE)szPath, (lstrlen(szPath) + 1) * sizeof(WCHAR));
        }
        else {
            RegDeleteValue(hKey, L"ScreenRotator");
        }
        RegCloseKey(hKey);
    }
}

void RotateScreen(DWORD angle) {
    DEVMODE dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    if (0 != EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        DWORD currentOrientation = dm.dmDisplayOrientation;
        bool isCurrentPortrait = (currentOrientation == DMDO_90 || currentOrientation == DMDO_270);
        bool isTargetPortrait = (angle == DMDO_90 || angle == DMDO_270);

        if (isCurrentPortrait != isTargetPortrait) {
            DWORD temp = dm.dmPelsHeight;
            dm.dmPelsHeight = dm.dmPelsWidth;
            dm.dmPelsWidth = temp;
        }
        dm.dmDisplayOrientation = angle;
        dm.dmFields = DM_DISPLAYORIENTATION | DM_PELSWIDTH | DM_PELSHEIGHT;
        ChangeDisplaySettings(&dm, 0);
    }
}

void UpdateHotkeys(HWND hWnd) {
    UnregisterHotKey(hWnd, 1);
    UnregisterHotKey(hWnd, 2);
    UnregisterHotKey(hWnd, 3);
    UnregisterHotKey(hWnd, 4);

    RegisterHotKey(hWnd, 1, modUp | MOD_NOREPEAT, vkUp);
    RegisterHotKey(hWnd, 2, modDown | MOD_NOREPEAT, vkDown);
    RegisterHotKey(hWnd, 3, modLeft | MOD_NOREPEAT, vkLeft);
    RegisterHotKey(hWnd, 4, modRight | MOD_NOREPEAT, vkRight);
}

LRESULT CALLBACK HotkeyInputProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU)
            return 0;

        UINT vk = (UINT)wParam;
        UINT mod = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
        if (GetKeyState(VK_SHIFT) & 0x8000)   mod |= MOD_SHIFT;
        if (GetKeyState(VK_MENU) & 0x8000)    mod |= MOD_ALT;

        std::wstring txt = FormatHotkeyString(vk, mod);
        SetWindowText(hWnd, txt.c_str());

        int id = GetDlgCtrlID(hWnd);
        if (id == IDC_HOTKEY_UP) { vkUp = vk; modUp = mod; }
        if (id == IDC_HOTKEY_DOWN) { vkDown = vk; modDown = mod; }
        if (id == IDC_HOTKEY_LEFT) { vkLeft = vk; modLeft = mod; }
        if (id == IDC_HOTKEY_RIGHT) { vkRight = vk; modRight = mod; }
    }
    return 0;

    case WM_CHAR:
        return 0;

    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }
    return CallWindowProc(wpOrigEditProc, hWnd, uMsg, wParam, lParam);
}

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
    {
        BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(hDlg, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON));
        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        SetWindowTheme(GetDlgItem(hDlg, IDOK), L"Explorer", NULL);
        SetWindowTheme(GetDlgItem(hDlg, IDCANCEL), L"Explorer", NULL);

        SetWindowTheme(GetDlgItem(hDlg, IDC_STARTUP), L"", L"");

        HWND hCtrls[] = { GetDlgItem(hDlg, IDC_HOTKEY_UP), GetDlgItem(hDlg, IDC_HOTKEY_DOWN),
                          GetDlgItem(hDlg, IDC_HOTKEY_LEFT), GetDlgItem(hDlg, IDC_HOTKEY_RIGHT) };

        for (HWND h : hCtrls) {
            SetWindowTheme(h, L"", L"");
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(h, GWLP_WNDPROC, (LONG_PTR)HotkeyInputProc);
        }

        RECT rcOwner, rcDlg;
        GetWindowRect(GetDesktopWindow(), &rcOwner);
        GetWindowRect(hDlg, &rcDlg);
        int width = rcDlg.right - rcDlg.left;
        int height = rcDlg.bottom - rcDlg.top;
        SetWindowPos(hDlg, NULL, (rcOwner.right - width) / 2, (rcOwner.bottom - height) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        SetDlgItemText(hDlg, IDC_HOTKEY_UP, FormatHotkeyString(vkUp, modUp).c_str());
        SetDlgItemText(hDlg, IDC_HOTKEY_DOWN, FormatHotkeyString(vkDown, modDown).c_str());
        SetDlgItemText(hDlg, IDC_HOTKEY_LEFT, FormatHotkeyString(vkLeft, modLeft).c_str());
        SetDlgItemText(hDlg, IDC_HOTKEY_RIGHT, FormatHotkeyString(vkRight, modRight).c_str());

        if (IsStartupEnabled()) {
            CheckDlgButton(hDlg, IDC_STARTUP, BST_CHECKED);
        }
    }
    return (INT_PTR)TRUE;

    case WM_CTLCOLORDLG:
        return (INT_PTR)hBlackBrush;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        return (INT_PTR)hBlackBrush;
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;

        if (GetFocus() == hCtrl) {
            SetTextColor(hdc, RGB(0, 0, 0));
            SetBkColor(hdc, RGB(255, 255, 255));
            return (INT_PTR)hWhiteBrush;
        }
        else {
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkColor(hdc, RGB(64, 64, 64));
            return (INT_PTR)hInputDarkBrush;
        }
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            struct HotkeyDef { UINT vk; UINT mod; const wchar_t* name; };
            HotkeyDef keys[4] = {
                { vkUp, modUp, L"Rotate Normal" },
                { vkDown, modDown, L"Rotate 180" },
                { vkLeft, modLeft, L"Rotate Left" },
                { vkRight, modRight, L"Rotate Right" }
            };

            for (int i = 0; i < 4; i++) {
                if (keys[i].vk == 0) continue;
                for (int j = i + 1; j < 4; j++) {
                    if (keys[j].vk == 0) continue;
                    if (keys[i].vk == keys[j].vk && keys[i].mod == keys[j].mod) {
                        std::wstring msg = L"Duplicate hotkey detected:\n'";
                        msg += keys[i].name;
                        msg += L"' and '";
                        msg += keys[j].name;
                        msg += L"' are set to the same key.\n\nPlease assign unique keys.";
                        MessageBox(hDlg, msg.c_str(), L"Settings Error", MB_OK | MB_ICONEXCLAMATION);
                        return (INT_PTR)TRUE;
                    }
                }
            }

            bool start = IsDlgButtonChecked(hDlg, IDC_STARTUP) == BST_CHECKED;
            SetStartup(start);
            SaveSettings();
            UpdateHotkeys(hMainWnd);
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            LoadSettings();
            UpdateHotkeys(hMainWnd);
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        hPopMenu = CreatePopupMenu();
        AppendMenu(hPopMenu, MF_STRING, ID_SETTINGS_ITEM, L"Settings");
        AppendMenu(hPopMenu, MF_STRING, ID_EXIT, L"Exit");
        break;

    case WM_TRAYICON:
        if (lParam == WM_LBUTTONUP) {
            SendMessage(hWnd, WM_COMMAND, ID_SETTINGS_ITEM, 0);
        }
        else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hPopMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_EXIT) {
            PostQuitMessage(0);
        }
        if (LOWORD(wParam) == ID_SETTINGS_ITEM) {
            UnregisterHotKey(hWnd, 1);
            UnregisterHotKey(hWnd, 2);
            UnregisterHotKey(hWnd, 3);
            UnregisterHotKey(hWnd, 4);
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SETTINGS), hWnd, SettingsDlgProc);
        }
        break;

    case WM_HOTKEY:
        if (wParam == 1) RotateScreen(DMDO_DEFAULT);
        if (wParam == 2) RotateScreen(DMDO_180);
        if (wParam == 3) RotateScreen(DMDO_90);
        if (wParam == 4) RotateScreen(DMDO_270);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_HOTKEY_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
    hInputDarkBrush = CreateSolidBrush(RGB(64, 64, 64));
    hWhiteBrush = CreateSolidBrush(RGB(255, 255, 255));

    LoadSettings();

    WNDCLASSEX wx = { 0 };
    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = WndProc;
    wx.hInstance = hInstance;
    wx.lpszClassName = L"DUMMY_CLASS";
    RegisterClassEx(&wx);

    hMainWnd = CreateWindowEx(0, L"DUMMY_CLASS", L"ScreenRotator", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hMainWnd;
    nid.uID = 100;
    nid.uVersion = NOTIFYICON_VERSION;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wcscpy_s(nid.szTip, L"Screen Rotator");
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;

    Shell_NotifyIcon(NIM_ADD, &nid);

    UpdateHotkeys(hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Shell_NotifyIcon(NIM_DELETE, &nid);
    DeleteObject(hBlackBrush);
    DeleteObject(hInputDarkBrush);
    DeleteObject(hWhiteBrush);
    return 0;
}