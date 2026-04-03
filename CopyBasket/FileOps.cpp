#include <windows.h>
#include <shobjidl.h>
#include <process.h>
#include "FileOps.h"
#include "BasketStore.h"
#include "Strings.h"

extern volatile LONG g_cRef;

namespace FileOps {

// ---------------------------------------------------------------------------
// CFileOperationProgressSink — removes files from basket after each success
// ---------------------------------------------------------------------------
class CFileOperationProgressSink : public IFileOperationProgressSink {
    volatile LONG m_cRef;
    bool m_removeFromBasket;
    std::vector<std::wstring> m_basket;  // in-memory basket, written once in FinishOperations

    void TrackSuccess(IShellItem* psiItem) {
        if (!m_removeFromBasket || !psiItem) return;
        PWSTR pszPath = NULL;
        if (SUCCEEDED(psiItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
            // Remove from in-memory basket (case-insensitive)
            for (auto it = m_basket.begin(); it != m_basket.end(); ++it) {
                if (_wcsicmp(it->c_str(), pszPath) == 0) {
                    m_basket.erase(it);
                    break;
                }
            }
            CoTaskMemFree(pszPath);
        }
    }

public:
    CFileOperationProgressSink(bool removeFromBasket)
        : m_cRef(1), m_removeFromBasket(removeFromBasket) {
        if (removeFromBasket) m_basket = BasketStore::ReadBasket();
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        if (riid == IID_IUnknown || riid == IID_IFileOperationProgressSink) {
            *ppv = static_cast<IFileOperationProgressSink*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef()  { return InterlockedIncrement(&m_cRef); }
    IFACEMETHODIMP_(ULONG) Release() {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }

    IFACEMETHODIMP StartOperations()    { return S_OK; }
    IFACEMETHODIMP FinishOperations(HRESULT) {
        if (m_removeFromBasket) BasketStore::WriteBasket(m_basket);
        return S_OK;
    }
    IFACEMETHODIMP PreRenameItem(DWORD, IShellItem*, LPCWSTR) { return S_OK; }
    IFACEMETHODIMP PostRenameItem(DWORD, IShellItem*, LPCWSTR, HRESULT, IShellItem*) { return S_OK; }
    IFACEMETHODIMP PreMoveItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) { return S_OK; }
    IFACEMETHODIMP PreCopyItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) { return S_OK; }
    IFACEMETHODIMP PreDeleteItem(DWORD, IShellItem*) { return S_OK; }
    IFACEMETHODIMP PostDeleteItem(DWORD, IShellItem*, HRESULT, IShellItem*) { return S_OK; }
    IFACEMETHODIMP PreNewItem(DWORD, IShellItem*, LPCWSTR) { return S_OK; }
    IFACEMETHODIMP PostNewItem(DWORD, IShellItem*, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem*) { return S_OK; }
    IFACEMETHODIMP UpdateProgress(UINT, UINT) { return S_OK; }
    IFACEMETHODIMP ResetTimer() { return S_OK; }
    IFACEMETHODIMP PauseTimer() { return S_OK; }
    IFACEMETHODIMP ResumeTimer() { return S_OK; }

    IFACEMETHODIMP PostCopyItem(DWORD, IShellItem* psiItem, IShellItem*, LPCWSTR, HRESULT hrCopy, IShellItem*) {
        if (SUCCEEDED(hrCopy)) TrackSuccess(psiItem);
        return S_OK;
    }
    IFACEMETHODIMP PostMoveItem(DWORD, IShellItem* psiItem, IShellItem*, LPCWSTR, HRESULT hrMove, IShellItem*) {
        if (SUCCEEDED(hrMove)) TrackSuccess(psiItem);
        return S_OK;
    }
};

// ---------------------------------------------------------------------------
// ExecuteFileOpCOM — IFileOperation-based file copy/move
// ---------------------------------------------------------------------------
static BOOL ExecuteFileOpCOM(UINT wFunc, const std::vector<std::wstring>& files,
                             const std::wstring& destFolder, bool removeFromBasket) {
    if (files.empty() || destFolder.empty()) return FALSE;

    IFileOperation* pfo = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileOperation, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pfo));
    if (FAILED(hr)) return FALSE;

    pfo->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR);

    IShellItem* psiDest = NULL;
    hr = SHCreateItemFromParsingName(destFolder.c_str(), NULL, IID_PPV_ARGS(&psiDest));
    if (FAILED(hr)) {
        pfo->Release();
        return FALSE;
    }

    for (const auto& filePath : files) {
        IShellItem* psiFrom = NULL;
        hr = SHCreateItemFromParsingName(filePath.c_str(), NULL, IID_PPV_ARGS(&psiFrom));
        if (SUCCEEDED(hr)) {
            if (wFunc == FO_COPY)
                pfo->CopyItem(psiFrom, psiDest, NULL, NULL);
            else
                pfo->MoveItem(psiFrom, psiDest, NULL, NULL);
            psiFrom->Release();
        }
    }

    CFileOperationProgressSink* pSink = new CFileOperationProgressSink(removeFromBasket);
    DWORD dwCookie = 0;
    pfo->Advise(pSink, &dwCookie);

    hr = pfo->PerformOperations();

    BOOL fAborted = FALSE;
    pfo->GetAnyOperationsAborted(&fAborted);

    pfo->Unadvise(dwCookie);
    pSink->Release();
    psiDest->Release();
    pfo->Release();

    return SUCCEEDED(hr) && !fAborted;
}

BOOL BrowseForFolder(HWND hwnd, std::wstring& folderOut) {
    // Guard: Explorer may call InvokeCommand once per selected item,
    // which would open multiple dialogs. Prevent re-entrant and rapid
    // sequential calls so only one dialog is shown.
    static volatile LONG s_dialogOpen = 0;
    static DWORD s_lastCloseTick = 0;

    if (InterlockedCompareExchange(&s_dialogOpen, 1, 0) != 0)
        return FALSE;

    DWORD now = GetTickCount();
    if ((now - s_lastCloseTick) < 500) {
        InterlockedExchange(&s_dialogOpen, 0);
        return FALSE;
    }

    folderOut.clear();

    IFileDialog* pfd = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pfd));
    if (FAILED(hr)) {
        InterlockedExchange(&s_dialogOpen, 0);
        return FALSE;
    }

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
    s_lastCloseTick = GetTickCount();
    InterlockedExchange(&s_dialogOpen, 0);
    return !folderOut.empty() ? TRUE : FALSE;
}

// --- Async file operations (background thread) ---

struct FileOpParams {
    UINT wFunc;
    std::vector<std::wstring> files;
    std::wstring destFolder;
    bool removeFromBasket;
};

static unsigned __stdcall FileOpThreadProc(void* pArg) {
    FileOpParams* params = (FileOpParams*)pArg;

    // Initialize COM for this thread (needed for shell operations)
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    ExecuteFileOpCOM(params->wFunc, params->files, params->destFolder, params->removeFromBasket);

    CoUninitialize();
    delete params;
    InterlockedDecrement(&g_cRef);
    return 0;
}

static void LaunchFileOp(UINT wFunc, const std::vector<std::wstring>& files,
                         const std::wstring& destFolder, bool removeFromBasket) {
    if (files.empty() || destFolder.empty()) return;

    FileOpParams* params = new FileOpParams();
    params->wFunc = wFunc;
    params->files = files;
    params->destFolder = destFolder;
    params->removeFromBasket = removeFromBasket;

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

void CopyFilesToFolderAsync(const std::vector<std::wstring>& files,
                            const std::wstring& destFolder, bool removeFromBasket) {
    LaunchFileOp(FO_COPY, files, destFolder, removeFromBasket);
}

void MoveFilesToFolderAsync(const std::vector<std::wstring>& files,
                            const std::wstring& destFolder, bool removeFromBasket) {
    LaunchFileOp(FO_MOVE, files, destFolder, removeFromBasket);
}

} // namespace FileOps
