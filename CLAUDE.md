# CopyBasket

Windows Explorer Shell Extension (COM DLL) zum Sammeln von Dateien in einen virtuellen "Korb" und anschließendem Kopieren/Verschieben an ein Zielverzeichnis.

## Build

Voraussetzung: Visual Studio 2022 Build Tools (Toolset v143).

```bash
# Release x64
MSYS_NO_PATHCONV=1 "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe" CopyBasket.sln /p:Configuration=Release /p:Platform=x64 /verbosity:minimal

# Release Win32
MSYS_NO_PATHCONV=1 "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe" CopyBasket.sln /p:Configuration=Release /p:Platform=Win32 /verbosity:minimal
```

Output: `x64\Release\CopyBasket.dll` bzw. `Release\CopyBasket.dll`

## Registrierung (als Admin)

```cmd
regsvr32 x64\Release\CopyBasket.dll
regsvr32 /u x64\Release\CopyBasket.dll
```

## Architektur

- **Sprache:** C++ mit Unicode durchgehend (WCHAR/std::wstring)
- **COM-Interfaces:** IShellExtInit + IContextMenu
- **CLSID:** {F2D8A4E6-3B7C-4D1E-9F5A-8C6E2A4B0D12} (GUID.h)
- **Korb-Speicher:** `%APPDATA%\CopyBasket\basket.txt` (UTF-16LE mit BOM)
- **Dateioperationen:** SHFileOperationW (mit Fortschrittsdialog)

## Quelldateien

| Datei | Inhalt |
|-------|--------|
| CopyBasket.cpp | DllMain, COM Boilerplate, DllRegisterServer/DllUnregisterServer |
| ShellExt.cpp | IShellExtInit::Initialize, IContextMenu (QueryContextMenu, InvokeCommand, GetCommandString) |
| BasketStore.h/.cpp | Korb-Datei lesen/schreiben/leeren, Duplikat-Pruefung |
| FileOps.h/.cpp | SHFileOperationW Wrapper fuer Kopieren und Verschieben, BrowseForFolder (IFileDialog) |
| BasketDialog.h/.cpp | ListView-Dialog fuer Korb-Inhalt mit Entfernen-Funktion |
| Strings.h/.cpp | i18n String-Tabellen (DE/EN), LoadLanguageSetting(), SaveLanguageSetting() |
| SettingsDialog.h/.cpp | Modaler Einstellungen-Dialog mit Sprachauswahl (ComboBox) |
| CopyBasket.h | Klassendeklarationen (CShellExtClassFactory, CShellExt), CmdOffset Enum |
| GUID.h | CLSID_CopyBasket Definition |
| CopyBasket.def | DLL Exports (DllCanUnloadNow, DllGetClassObject, DllRegisterServer, DllUnregisterServer) |
| CopyBasket.rc | Versioninfo-Resource |

## Registry-Punkte (3 Registrierungen)

- `HKCR\*\shellex\ContextMenuHandlers\CopyBasket` — Rechtsklick auf Dateien
- `HKCR\Directory\shellex\ContextMenuHandlers\CopyBasket` — Rechtsklick auf Ordner
- `HKCR\Directory\Background\shellex\ContextMenuHandlers\CopyBasket` — Rechtsklick auf Hintergrund

## Kontextmenu

Popup-Submenu "CopyBasket" mit:
- Zum Korb hinzufuegen (nur bei Datei/Ordner-Klick)
- Pfad kopieren (nur bei Datei/Ordner-Klick) — kopiert Pfad(e) in die Zwischenablage
- Korb anzeigen (X Dateien) — oeffnet ListView-Dialog mit Entfernen-Funktion
- ---Separator---
- Korb hierher kopieren
- Kopieren nach... — Ordnerauswahl-Dialog (IFileDialog)
- Korb hierher verschieben
- Verschieben nach... — Ordnerauswahl-Dialog (IFileDialog)
- ---Separator---
- Korb leeren
- ---Separator---
- Einstellungen — oeffnet Dialog zur Sprachauswahl (Deutsch/English)

Items sind grayed wenn der Korb leer ist. "Zum Korb hinzufuegen" und "Pfad kopieren" sind bei Hintergrund-Klick ausgeblendet. "Einstellungen" ist immer aktiv.

### Menu-Icon

Das Hauptmenu-Item "CopyBasket" zeigt ein eigenes Icon:
- **Resource:** `Res\basket.ico` eingebunden als `IDI_BASKET` (resource.h), geladen via `LoadImageW(g_hModule, ...)`
- **Konvertierung:** Hilfsfunktion `IconToBitmap()` wandelt HICON in 32-Bit-ARGB-HBITMAP (fuer Transparenz)
- **Groesse:** Systemmetriken `SM_CXSMICON`/`SM_CYSMICON` fuer konsistente Darstellung
- **Zuweisung:** Via `MENUITEMINFOW` mit `MIIM_BITMAP`-Flag und `hbmpItem` in `QueryContextMenu`
- **Lebenszyklus:** Bitmap wird im Konstruktor erstellt (`m_hMenuBitmap`) und im Destruktor mit `DeleteObject` freigegeben

### Korb-Dialog

- **Nicht-modal:** Explorer bleibt bedienbar waehrend der Dialog offen ist
- **Auto-Close:** Dialog schliesst sich bei Fokusverlust (WM_ACTIVATE/WA_INACTIVE)
- **Persistenz:** Fenstergroesse und Spaltenbreiten werden in Registry gespeichert (`HKCU\Software\CopyBasket\DialogWidth/Height/ColWidth0/ColWidth1`)
- **Pfad-Spalte:** Zweite Spalte zeigt den vollen Dateipfad (nicht nur den Ordner)

## i18n

- **Architektur:** `StringTable`-Struct mit `const wchar_t*`/`const char*`-Membern, zwei statische Instanzen (s_DE/s_EN), globaler Pointer auf aktive Tabelle
- **Zugriff:** `GetStrings().MemberName` — typ-sicher, Zero-Overhead
- **Spracheinstellung:** Registry `HKCU\Software\CopyBasket\Language` = `"de"` oder `"en"` (Default: Deutsch)
- **Laden:** `LoadLanguageSetting()` im CShellExt-Konstruktor
- **Aendern:** Einstellungen-Dialog → `SaveLanguageSetting()` → sofort wirksam beim naechsten Rechtsklick

## TODO

- [x] **"Korb anzeigen" durch ListView-Dialog ersetzen:** ListView-Dialog mit zwei Spalten (Dateiname/Ordner), Entfernen-Button + Delete-Taste, BasketStore::WriteBasket()
- [x] **"Kopieren nach..." / "Verschieben nach...":** Ordnerauswahl via IFileDialog, neue Menu-Items CMD_COPY_TO und CMD_MOVE_TO
- [x] **"Pfad kopieren":** Pfad(e) der Auswahl via Clipboard API (CF_UNICODETEXT) in die Zwischenablage kopieren
- [x] **"Einstellungen":** Modaler Dialog mit ComboBox zur Sprachauswahl, Persistenz in Registry
- [x] **i18n:** Alle UI-Strings ueber StringTable (Deutsch/English), struct-basiert

## Konventionen

- Kein Precompiled Header
- ASCII-sichere Bezeichner im Code, Umlaute via Unicode-Escapes (z.B. `\u00FC` fuer ue)
- Referenzprojekt: `C:\Users\HJS\Claude.ai\wscitecm\` (SciTE Context Menu Extension)
