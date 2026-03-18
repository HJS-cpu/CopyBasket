#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace FileOps {
    BOOL CopyFilesToFolder(HWND hwnd, const std::vector<std::wstring>& files, const std::wstring& destFolder);
    BOOL MoveFilesToFolder(HWND hwnd, const std::vector<std::wstring>& files, const std::wstring& destFolder);
    BOOL BrowseForFolder(HWND hwnd, std::wstring& folderOut);

    void CopyFilesToFolderAsync(const std::vector<std::wstring>& files, const std::wstring& destFolder);
    void MoveFilesToFolderAsync(const std::vector<std::wstring>& files, const std::wstring& destFolder);
}
