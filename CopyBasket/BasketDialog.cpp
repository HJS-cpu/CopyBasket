#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "BasketDialog.h"
#include "BasketStore.h"
#include "Strings.h"
#include "resource.h"

extern HINSTANCE g_hModule;

#pragma comment(lib, "comctl32.lib")

namespace BasketDialog {

static const WCHAR* WND_CLASS = L"CopyBasketDlg";
static const int IDC_LISTVIEW = 1001;
static const int IDC_REMOVE = 1002;
static const int IDC_CLOSE = 1003;
static const int MIN_WIDTH = 450;
static const int MIN_HEIGHT = 300;

static const WCHAR* REG_KEY = L"Software\\CopyBasket";

struct DlgData {
    HWND hList;
    HWND hBtnRemove;
    HWND hBtnClose;
};

static DWORD RegReadDword(const WCHAR* name, DWORD def) {
    DWORD val = def;
    DWORD cb = sizeof(val);
    RegGetValueW(HKEY_CURRENT_USER, REG_KEY, name,
                  RRF_RT_REG_DWORD, NULL, &val, &cb);
    return val;
}

static void RegWriteDword(HKEY hKey, const WCHAR* name, DWORD val) {
    RegSetValueExW(hKey, name, 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
}

static void SaveDialogState(HWND hwnd, DlgData* dd) {
    RECT rc;
    GetWindowRect(hwnd, &rc);

    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, NULL,
                         0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegWriteDword(hKey, L"DialogWidth", rc.right - rc.left);
        RegWriteDword(hKey, L"DialogHeight", rc.bottom - rc.top);
        if (dd->hList) {
            RegWriteDword(hKey, L"ColWidth0", ListView_GetColumnWidth(dd->hList, 0));
            RegWriteDword(hKey, L"ColWidth1", ListView_GetColumnWidth(dd->hList, 1));
        }
        RegCloseKey(hKey);
    }
}

static void PopulateListView(HWND hList, const std::vector<std::wstring>& files) {
    ListView_DeleteAllItems(hList);
    for (int i = 0; i < (int)files.size(); i++) {
        const std::wstring& path = files[i];

        // Split into filename (column 0) and full path (column 1)
        std::wstring name = path;
        size_t pos = path.rfind(L'\\');
        if (pos != std::wstring::npos) {
            name = path.substr(pos + 1);
        }

        LVITEMW lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)name.c_str();
        ListView_InsertItem(hList, &lvi);

        ListView_SetItemText(hList, i, 1, (LPWSTR)path.c_str());
    }
}

static void RemoveSelected(HWND hList) {
    std::vector<std::wstring> remaining;
    int count = ListView_GetItemCount(hList);

    for (int i = 0; i < count; i++) {
        if (ListView_GetItemState(hList, i, LVIS_SELECTED) & LVIS_SELECTED)
            continue;

        WCHAR szPath[MAX_PATH] = {};
        ListView_GetItemText(hList, i, 1, szPath, MAX_PATH);
        if (szPath[0] != L'\0') {
            remaining.push_back(szPath);
        }
    }

    BasketStore::WriteBasket(remaining);
    PopulateListView(hList, remaining);
}

static void LayoutControls(HWND hwnd, DlgData* dd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int btnW = 100;
    int btnH = 28;
    int margin = 8;

    MoveWindow(dd->hList, margin, margin, w - 2 * margin, h - btnH - 3 * margin, TRUE);
    MoveWindow(dd->hBtnRemove, w - 2 * btnW - 2 * margin, h - btnH - margin, btnW, btnH, TRUE);
    MoveWindow(dd->hBtnClose, w - btnW - margin, h - btnH - margin, btnW, btnH, TRUE);
}

static LRESULT CALLBACK DlgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DlgData* dd = (DlgData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        dd = (DlgData*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)dd);

        dd->hList = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            0, 0, 0, 0,
            hwnd, (HMENU)(INT_PTR)IDC_LISTVIEW, GetModuleHandle(NULL), NULL);
        ListView_SetExtendedListViewStyle(dd->hList,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

        // Columns with saved widths
        int col0w = (int)RegReadDword(L"ColWidth0", 240);
        int col1w = (int)RegReadDword(L"ColWidth1", 320);

        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.cx = col0w;
        col.pszText = (LPWSTR)GetStrings().DlgColFilename;
        col.iSubItem = 0;
        ListView_InsertColumn(dd->hList, 0, &col);

        col.cx = col1w;
        col.pszText = (LPWSTR)GetStrings().DlgColPath;
        col.iSubItem = 1;
        ListView_InsertColumn(dd->hList, 1, &col);

        dd->hBtnRemove = CreateWindowExW(
            0, L"BUTTON", GetStrings().DlgBtnRemove,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0,
            hwnd, (HMENU)(INT_PTR)IDC_REMOVE, GetModuleHandle(NULL), NULL);

        dd->hBtnClose = CreateWindowExW(
            0, L"BUTTON", GetStrings().DlgBtnClose,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0,
            hwnd, (HMENU)(INT_PTR)IDC_CLOSE, GetModuleHandle(NULL), NULL);

        // Set default font
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessage(dd->hBtnRemove, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(dd->hBtnClose, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Populate
        std::vector<std::wstring> files = BasketStore::ReadBasket();
        PopulateListView(dd->hList, files);

        LayoutControls(hwnd, dd);

        // Set window icon from DLL resource
        HICON hIconSmall = (HICON)LoadImageW(g_hModule, MAKEINTRESOURCE(IDI_BASKET),
                                              IMAGE_ICON,
                                              GetSystemMetrics(SM_CXSMICON),
                                              GetSystemMetrics(SM_CYSMICON),
                                              LR_SHARED);
        HICON hIconBig = (HICON)LoadImageW(g_hModule, MAKEINTRESOURCE(IDI_BASKET),
                                            IMAGE_ICON,
                                            GetSystemMetrics(SM_CXICON),
                                            GetSystemMetrics(SM_CYICON),
                                            LR_SHARED);
        if (hIconSmall) SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
        if (hIconBig)   SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);

        return 0;
    }

    case WM_SIZE:
        if (dd) LayoutControls(hwnd, dd);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = MIN_WIDTH;
        mmi->ptMinTrackSize.y = MIN_HEIGHT;
        return 0;
    }

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_REMOVE) {
            if (dd) RemoveSelected(dd->hList);
        } else if (LOWORD(wParam) == IDC_CLOSE || LOWORD(wParam) == IDCANCEL) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_NOTIFY: {
        NMHDR* nm = (NMHDR*)lParam;
        if (nm->idFrom == IDC_LISTVIEW && nm->code == LVN_KEYDOWN) {
            NMLVKEYDOWN* kd = (NMLVKEYDOWN*)lParam;
            if (kd->wVKey == VK_DELETE && dd) {
                RemoveSelected(dd->hList);
            }
        }
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (dd) SaveDialogState(hwnd, dd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Show(HWND hwndParent) {
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = DlgWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = WND_CLASS;
    RegisterClassW(&wc);

    DlgData dd = {};

    // Load saved window size or use defaults
    int ww = (int)RegReadDword(L"DialogWidth", 620);
    int wh = (int)RegReadDword(L"DialogHeight", 420);

    // Center on screen
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - ww) / 2;
    int y = (sh - wh) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        WND_CLASS, GetStrings().DlgBasketTitle,
        (WS_OVERLAPPEDWINDOW & ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) | WS_CLIPCHILDREN,
        x, y, ww, wh,
        hwndParent, NULL, GetModuleHandle(NULL), &dd);

    if (!hwnd) return;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnregisterClassW(WND_CLASS, GetModuleHandle(NULL));
}

} // namespace BasketDialog
