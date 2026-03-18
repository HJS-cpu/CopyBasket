#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <process.h>
#include "FileOps.h"
#include "BasketStore.h"
#include "Strings.h"

extern volatile LONG g_cRef;

namespace FileOps {

// Build double-null-terminated string for SHFileOperation
static std::wstring BuildFileList(const std::vector<std::wstring>& files) {
    std::wstring result;
    for (const auto& f : files) {
        result += f;
        result += L'\0';
    }
    result += L'\0';
    return result;
}

static BOOL ExecuteFileOp(HWND hwnd, UINT wFunc, const std::vector<std::wstring>& files, const std::wstring& destFolder) {
    if (files.empty() || destFolder.empty()) return FALSE;

    std::wstring from = BuildFileList(files);
    std::wstring to = destFolder + L'\0';

    SHFILEOPSTRUCTW op = {};
    op.hwnd = hwnd;
    op.wFunc = wFunc;
    op.pFrom = from.c_str();
    op.pTo = to.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR;

    return (SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted);
}

BOOL CopyFilesToFolder(HWND hwnd, const std::vector<std::wstring>& files, const std::wstring& destFolder) {
    return ExecuteFileOp(hwnd, FO_COPY, files, destFolder);
}

BOOL MoveFilesToFolder(HWND hwnd, const std::vector<std::wstring>& files, const std::wstring& destFolder) {
    return ExecuteFileOp(hwnd, FO_MOVE, files, destFolder);
}

BOOL BrowseForFolder(HWND hwnd, std::wstring& folderOut) {
    folderOut.clear();

    IFileDialog* pfd = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pfd));
    if (FAILED(hr)) return FALSE;

    DWORD dwOptions;
    pfd->GetOptions(&dwOptions);
    pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    pfd->SetTitle(GetStrings().BrowseTitle);

    hr = pfd->Show(hwnd);
    if (SUCCEEDED(hr)) {
        IShellItem* psi = NULL;
        hr = pfd->GetResult(&psi);
        if (SUCCEEDED(hr)) {
            PWSTR pszPath = NULL;
            hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
            if (SUCCEEDED(hr)) {
                folderOut = pszPath;
                CoTaskMemFree(pszPath);
            }
            psi->Release();
        }
    }

    pfd->Release();
    return !folderOut.empty() ? TRUE : FALSE;
}

// --- Async file operations (background thread) ---

struct FileOpParams {
    UINT wFunc;
    std::vector<std::wstring> files;
    std::wstring destFolder;
};

static unsigned __stdcall FileOpThreadProc(void* pArg) {
    FileOpParams* params = (FileOpParams*)pArg;

    // Initialize COM for this thread (needed for shell operations)
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (ExecuteFileOp(NULL, params->wFunc, params->files, params->destFolder)) {
        BasketStore::ClearBasket();
    }

    CoUninitialize();
    delete params;
    InterlockedDecrement(&g_cRef);
    return 0;
}

static void LaunchFileOp(UINT wFunc, const std::vector<std::wstring>& files, const std::wstring& destFolder) {
    if (files.empty() || destFolder.empty()) return;

    FileOpParams* params = new FileOpParams();
    params->wFunc = wFunc;
    params->files = files;
    params->destFolder = destFolder;

    InterlockedIncrement(&g_cRef);
    unsigned threadId;
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, FileOpThreadProc, params, 0, &threadId);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        // Thread creation failed — clean up
        delete params;
        InterlockedDecrement(&g_cRef);
    }
}

void CopyFilesToFolderAsync(const std::vector<std::wstring>& files, const std::wstring& destFolder) {
    LaunchFileOp(FO_COPY, files, destFolder);
}

void MoveFilesToFolderAsync(const std::vector<std::wstring>& files, const std::wstring& destFolder) {
    LaunchFileOp(FO_MOVE, files, destFolder);
}

} // namespace FileOps
