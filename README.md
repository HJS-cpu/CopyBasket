# CopyBasket

[![Build & Release](https://github.com/HJS-cpu/CopyBasket/actions/workflows/release.yml/badge.svg)](https://github.com/HJS-cpu/CopyBasket/actions/workflows/release.yml)
[![Latest Release](https://img.shields.io/github/v/release/HJS-cpu/CopyBasket)](https://github.com/HJS-cpu/CopyBasket/releases/latest)

A Windows Explorer shell extension that lets you collect files into a virtual "basket" and then copy or move them all at once to a target directory.

<!-- ![CopyBasket Screenshot](screenshot.png) -->

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| **Virtual Basket** | Collect files and folders from anywhere into a persistent basket |
| **Copy & Move** | Copy or move basket contents to the current folder or a chosen directory |
| **Copy Path** | Copy selected file/folder paths to the clipboard |
| **Browse Dialog** | Choose a target folder via modern IFileDialog |
| **Basket Viewer** | ListView dialog with file name and full path columns, remove individual items |
| **Direct Operations** | "Copy to..." / "Move to..." work on selected files when the basket is empty |
| **Async File Ops** | All copy/move operations run on a background thread — Explorer stays responsive |
| **i18n** | German and English UI, switchable via Settings dialog |
| **Auto-Clear** | Basket is automatically cleared after a successful operation |

---

## 📥 Download

**[⬇️ Download Latest Release](https://github.com/HJS-cpu/CopyBasket/releases/latest)**

Each release includes:
- **CopyBasket_x64.dll** — 64-bit build
- **CopyBasket_x86.dll** — 32-bit build

---

## 🖥️ System Requirements

- Windows 10 / 11
- No external dependencies

---

## 🔧 Installation

1. Download the DLL matching your system architecture (x64 or x86).
2. Place it in a permanent location (e.g. `C:\Program Files\CopyBasket\`).
3. Open an **elevated** Command Prompt and register the DLL:

```cmd
regsvr32 "C:\Program Files\CopyBasket\CopyBasket.dll"
```

To unregister:

```cmd
regsvr32 /u "C:\Program Files\CopyBasket\CopyBasket.dll"
```

---

## 📋 Context Menu

Right-click any file, folder, or the folder background to access the **CopyBasket** submenu:

| Item | Action |
|------|--------|
| **Add to Basket** | Add selected files/folders to the basket |
| **Copy Path** | Copy selected paths to the clipboard |
| **Show Basket (N files)** | Open the basket viewer dialog |
| **Copy Basket Here** | Copy basket contents to the current folder |
| **Copy to...** | Choose a folder, then copy |
| **Move Basket Here** | Move basket contents to the current folder |
| **Move to...** | Choose a folder, then move |
| **Clear Basket** | Remove all items from the basket |
| **Settings** | Change language (German / English) |

Items are grayed out when the basket is empty. "Add to Basket" and "Copy Path" are hidden on background clicks.

---

## 🛠️ Building from Source

### Prerequisites

- Visual Studio 2022 Build Tools (Toolset v143)
- Windows SDK

### Build

```bash
# x64 Release
MSBuild.exe CopyBasket.sln /p:Configuration=Release /p:Platform=x64

# x86 Release
MSBuild.exe CopyBasket.sln /p:Configuration=Release /p:Platform=Win32
```

Output: `x64\Release\CopyBasket.dll` / `Release\CopyBasket.dll`

---

## ⚙️ How It Works

CopyBasket is a COM DLL implementing `IShellExtInit` and `IContextMenu`. It registers as a context menu handler for files, directories, and the directory background.

- **Basket Storage:** `%APPDATA%\CopyBasket\basket.txt` (UTF-16LE with BOM)
- **File Operations:** `SHFileOperationW` on a background thread (`_beginthreadex`)
- **Settings:** Language preference stored in `HKCU\Software\CopyBasket`
- **Dialog Persistence:** Window size and column widths saved in Registry

---

## 📄 License

This project is provided as-is. See the [LICENSE](LICENSE) file for details.

---

## 📝 Changelog

### v1.0.0
- Initial release
- Virtual basket for collecting files and folders
- Copy / Move to current folder or chosen directory
- Copy path to clipboard
- ListView basket viewer with remove function
- Direct copy/move on selection when basket is empty
- Non-blocking async file operations
- German and English UI with settings dialog
- Custom basket icon in context menu
