#include <windows.h>
#include "SettingsDialog.h"
#include "Strings.h"

namespace SettingsDialog {

static const WCHAR* WND_CLASS = L"CopyBasketSettings";
static const int IDC_COMBO = 2001;
static const int IDC_OK = 2002;
static const int IDC_CANCEL_BTN = 2003;
static const int IDC_LABEL = 2004;

struct DlgData {
    HWND hLabel;
    HWND hCombo;
    HWND hBtnOK;
    HWND hBtnCancel;
    int initialSel;
};

static int GetCurrentLangIndex() {
    WCHAR szLang[16] = {};
    DWORD cbData = sizeof(szLang);
    LSTATUS res = RegGetValueW(HKEY_CURRENT_USER, L"Software\\CopyBasket",
                                L"Language", RRF_RT_REG_SZ, NULL, szLang, &cbData);
    if (res == ERROR_SUCCESS && _wcsicmp(szLang, L"en") == 0) {
        return 1;
    }
    return 0;
}

static LRESULT CALLBACK DlgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DlgData* dd = (DlgData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        dd = (DlgData*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)dd);

        const StringTable& S = GetStrings();
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        dd->hLabel = CreateWindowExW(
            0, L"STATIC", S.SettingsLanguageLabel,
            WS_CHILD | WS_VISIBLE,
            16, 20, 80, 20,
            hwnd, (HMENU)(INT_PTR)IDC_LABEL, GetModuleHandle(NULL), NULL);
        SendMessage(dd->hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        dd->hCombo = CreateWindowExW(
            0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            100, 17, 180, 120,
            hwnd, (HMENU)(INT_PTR)IDC_COMBO, GetModuleHandle(NULL), NULL);
        SendMessage(dd->hCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(dd->hCombo, CB_ADDSTRING, 0, (LPARAM)L"Deutsch");
        SendMessageW(dd->hCombo, CB_ADDSTRING, 0, (LPARAM)L"English");
        SendMessageW(dd->hCombo, CB_SETCURSEL, dd->initialSel, 0);

        dd->hBtnOK = CreateWindowExW(
            0, L"BUTTON", S.SettingsOK,
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            110, 60, 85, 28,
            hwnd, (HMENU)(INT_PTR)IDC_OK, GetModuleHandle(NULL), NULL);
        SendMessage(dd->hBtnOK, WM_SETFONT, (WPARAM)hFont, TRUE);

        dd->hBtnCancel = CreateWindowExW(
            0, L"BUTTON", S.SettingsCancel,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            200, 60, 85, 28,
            hwnd, (HMENU)(INT_PTR)IDC_CANCEL_BTN, GetModuleHandle(NULL), NULL);
        SendMessage(dd->hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_OK) {
            if (dd) {
                int sel = (int)SendMessageW(dd->hCombo, CB_GETCURSEL, 0, 0);
                const wchar_t* lang = (sel == 1) ? L"en" : L"de";
                SaveLanguageSetting(lang);
            }
            DestroyWindow(hwnd);
        } else if (LOWORD(wParam) == IDC_CANCEL_BTN || LOWORD(wParam) == IDCANCEL) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Show(HWND hwndParent) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DlgWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = WND_CLASS;
    RegisterClassW(&wc);

    DlgData dd = {};
    dd.initialSel = GetCurrentLangIndex();

    const StringTable& S = GetStrings();

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 320, wh = 140;
    int x = (sw - ww) / 2;
    int y = (sh - wh) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        WND_CLASS, S.SettingsTitle,
        WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, ww, wh,
        hwndParent, NULL, GetModuleHandle(NULL), &dd);

    if (!hwnd) return;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    if (hwndParent) EnableWindow(hwndParent, FALSE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (hwndParent) {
        EnableWindow(hwndParent, TRUE);
        SetForegroundWindow(hwndParent);
    }

    UnregisterClassW(WND_CLASS, GetModuleHandle(NULL));
}

} // namespace SettingsDialog
