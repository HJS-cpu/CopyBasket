#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include "resource.h"

// Mutex name for single instance check
static const wchar_t* MUTEX_NAME = L"CopyBasketContextMenuMutex";

// DLL filename (located in same directory as EXE)
static const wchar_t* DLL_NAME = L"CopyBasket.dll";

// Registry path to check if DLL is registered
static const wchar_t* REG_PATH = L"SOFTWARE\\Classes\\*\\shellex\\ContextMenuHandlers\\CopyBasket";

// User settings registry key (HKCU)
static const wchar_t* SETTINGS_KEY = L"Software\\CopyBasket";

// Check if the CopyBasket context menu handler is registered
BOOL IsCopyBasketRegistered()
{
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_PATH, 0, KEY_READ, &hKey);
    if (result == ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return TRUE;
    }
    return FALSE;
}

// Delete CopyBasket user settings (registry + %APPDATA%\CopyBasket folder)
static BOOL DeleteUserSettings()
{
    BOOL success = RegDeleteKeyW(HKEY_CURRENT_USER, SETTINGS_KEY) == ERROR_SUCCESS;

    // Delete %APPDATA%\CopyBasket\ folder (contains basket.txt)
    wchar_t appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData)))
    {
        wchar_t basketDir[MAX_PATH];
        wsprintfW(basketDir, L"%s\\CopyBasket", appData);

        // SHFileOperation requires double-null-terminated string
        wchar_t from[MAX_PATH + 1] = {};
        wcscpy_s(from, basketDir);

        SHFILEOPSTRUCTW op = {};
        op.wFunc = FO_DELETE;
        op.pFrom = from;
        op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
        SHFileOperationW(&op);
    }

    return success;
}

// Dialog procedure
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // Set dialog icon
        HICON hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_APPICON));
        SendMessageW(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        // Select based on current registration status
        if (IsCopyBasketRegistered())
            CheckRadioButton(hDlg, IDC_RADIO_ACTIVATE, IDC_RADIO_DEACTIVATE, IDC_RADIO_DEACTIVATE);
        else
            CheckRadioButton(hDlg, IDC_RADIO_ACTIVATE, IDC_RADIO_DEACTIVATE, IDC_RADIO_ACTIVATE);

        // Set warning text (hidden by default)
        SetDlgItemTextW(hDlg, IDC_WARNING_TEXT, L"\x26A0 Deletes all program settings permanently");

        // Disable checkbox if no user settings exist yet
        HKEY hSettingsKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY, 0, KEY_READ, &hSettingsKey) == ERROR_SUCCESS)
            RegCloseKey(hSettingsKey);
        else
            EnableWindow(GetDlgItem(hDlg, IDC_CHECK_CLEANREG), FALSE);

        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_CHECK_CLEANREG:
        {
            // Toggle warning text visibility
            BOOL checked = IsDlgButtonChecked(hDlg, IDC_CHECK_CLEANREG) == BST_CHECKED;
            ShowWindow(GetDlgItem(hDlg, IDC_WARNING_TEXT), checked ? SW_SHOW : SW_HIDE);
            return TRUE;
        }

        case IDOK:
        {
            // Handle "Delete User Settings" checkbox
            if (IsDlgButtonChecked(hDlg, IDC_CHECK_CLEANREG) == BST_CHECKED)
            {
                int confirm = MessageBoxW(hDlg,
                    L"This will permanently delete all CopyBasket user settings.\n\n"
                    L"Are you sure?",
                    L"Confirm Deletion",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

                if (confirm == IDYES)
                {
                    DeleteUserSettings();
                }
                else
                {
                    return TRUE; // User cancelled, stay in dialog
                }
            }

            // Get the directory where the EXE is located
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);

            // Extract directory from path
            wchar_t* lastSlash = wcsrchr(exePath, L'\\');
            if (lastSlash)
                *lastSlash = L'\0';

            // Build full DLL path
            wchar_t dllPath[MAX_PATH];
            wsprintfW(dllPath, L"%s\\%s", exePath, DLL_NAME);

            // Check if DLL exists
            if (GetFileAttributesW(dllPath) == INVALID_FILE_ATTRIBUTES)
            {
                MessageBoxW(hDlg,
                    L"CopyBasket.dll not found.\n"
                    L"The DLL must be in the same directory as CB-CMT.exe.",
                    L"Error",
                    MB_OK | MB_ICONERROR);
                EndDialog(hDlg, IDOK);
                return TRUE;
            }

            // Build regsvr32 parameters
            wchar_t params[MAX_PATH + 20];
            BOOL activate = IsDlgButtonChecked(hDlg, IDC_RADIO_ACTIVATE) == BST_CHECKED;

            if (activate)
            {
                // Register: regsvr32 /s "path\to\dll"
                wsprintfW(params, L"/s \"%s\"", dllPath);
            }
            else
            {
                // Unregister: regsvr32 /u /s "path\to\dll"
                wsprintfW(params, L"/u /s \"%s\"", dllPath);
            }

            // Execute regsvr32
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = L"open";
            sei.lpFile = L"regsvr32.exe";
            sei.lpParameters = params;
            sei.nShow = SW_HIDE;

            if (ShellExecuteExW(&sei))
            {
                // Wait for regsvr32 to complete
                if (sei.hProcess)
                {
                    WaitForSingleObject(sei.hProcess, INFINITE);

                    DWORD exitCode = 0;
                    GetExitCodeProcess(sei.hProcess, &exitCode);
                    CloseHandle(sei.hProcess);

                    if (exitCode == 0)
                    {
                        MessageBoxW(hDlg,
                            activate ? L"CopyBasket Context Menu activated successfully."
                                     : L"CopyBasket Context Menu deactivated successfully.",
                            L"Success",
                            MB_OK | MB_ICONINFORMATION);
                    }
                    else
                    {
                        MessageBoxW(hDlg,
                            L"Operation failed. Please ensure CopyBasket.dll exists.",
                            L"Error",
                            MB_OK | MB_ICONERROR);
                    }
                }
            }
            else
            {
                MessageBoxW(hDlg,
                    L"Failed to execute regsvr32.",
                    L"Error",
                    MB_OK | MB_ICONERROR);
            }

            EndDialog(hDlg, IDOK);
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}

// Application entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // Single instance check
    HANDLE hMutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (hMutex)
            CloseHandle(hMutex);
        return 0;
    }

    // Show the dialog
    DialogBoxW(hInstance, MAKEINTRESOURCEW(IDD_MAIN_DIALOG), NULL, DialogProc);

    // Cleanup
    if (hMutex)
        CloseHandle(hMutex);

    return 0;
}
