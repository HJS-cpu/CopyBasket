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
- **Dateioperationen:** IFileOperation + CFileOperationProgressSink (nicht-blockierend via Hintergrund-Thread, Korb-Entfernung gesammelt in FinishOperations)
- **Thread-Sicherheit:** `g_cRef` als `volatile LONG` mit `InterlockedIncrement/Decrement`
- **Datei-Erkennung:** Primaer CF_HDROP, Fallback via `SHCreateShellItemArrayFromDataObject` (fuer Navigationsbereich, virtuelle Ordner etc.) mit `SFGAO_FOLDER`-Pruefung fuer korrekte Ordner-Ziel-Erkennung

## Quelldateien

| Datei | Inhalt |
|-------|--------|
| CopyBasket.cpp | DllMain, COM Boilerplate, DllRegisterServer/DllUnregisterServer |
| ShellExt.cpp | IShellExtInit::Initialize, IContextMenu (QueryContextMenu, InvokeCommand, GetCommandString) |
| BasketStore.h/.cpp | Korb-Datei lesen/schreiben/leeren, Duplikat-Pruefung (GetBasketFilePath intern/static) |
| FileOps.h/.cpp | IFileOperation + CFileOperationProgressSink (async, Korb-Update gesammelt in FinishOperations), BrowseForFolder (IFileDialog, mit Re-Entrance-Guard) |
| BasketDialog.h/.cpp | ListView-Dialog fuer Korb-Inhalt mit Entfernen-Funktion |
| Strings.h/.cpp | i18n String-Tabellen (DE/EN), LoadLanguageSetting(), SaveLanguageSetting() |
| SettingsDialog.h/.cpp | Einstellungen-Dialog mit Tab Control (Sprache / \u00DCber mit Website-Link) |
| Version.h | Zentrale Versionskonstanten (COPYBASKET_VERSION_*) |
| CopyBasket.h | Klassendeklarationen (CShellExtClassFactory, CShellExt), CmdOffset Enum |
| GUID.h | CLSID_CopyBasket Definition |
| CopyBasket.def | DLL Exports (DllCanUnloadNow, DllGetClassObject, DllRegisterServer, DllUnregisterServer) |
| resource.h | Resource-IDs (IDI_BASKET, IDR_VERSION1) |
| CopyBasket.rc | Versioninfo-Resource |
| res/basket.ico | Menu-Icon Resource |
| installer/CopyBasket.nsi | NSIS Installer-Skript (regsvr32, x64/x86 Erkennung) |
| .github/workflows/release.yml | GitHub Actions Build & Release Workflow |

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
- Korb hierher kopieren — kopiert Korb-Inhalt in aktuellen Ordner (async)
- Kopieren nach... — Ordnerauswahl-Dialog, dann async kopieren
- Korb hierher verschieben — verschiebt Korb-Inhalt in aktuellen Ordner (async)
- Verschieben nach... — Ordnerauswahl-Dialog, dann async verschieben
- ---Separator---
- Korb leeren
- ---Separator---
- Einstellungen — oeffnet Tab-Dialog (Sprache / \u00DCber)

Items sind grayed wenn der Korb leer ist. "Zum Korb hinzufuegen" und "Pfad kopieren" sind bei Hintergrund-Klick ausgeblendet. "Einstellungen" ist immer aktiv. Funktioniert auch im Navigationsbereich (linke Seite) des Explorers.

"Kopieren nach..." und "Verschieben nach..." sind auch bei leerem Korb aktiv, wenn Dateien/Ordner selektiert sind — operieren dann direkt auf der Auswahl statt auf dem Korb.

Alle Dateioperationen (Kopieren/Verschieben) laufen nicht-blockierend auf einem Hintergrund-Thread (`_beginthreadex`) via `IFileOperation`. Erfolgreich verarbeitete Dateien werden in-memory aus dem Korb entfernt (`CFileOperationProgressSink::PostCopyItem`/`PostMoveItem`), der Korb wird einmalig in `FinishOperations` auf Platte geschrieben (O(N) statt O(N\u00B2)). Bei Abbruch oder Fehler bleiben nur die noch nicht verarbeiteten Dateien im Korb. Bei Fallback auf selektierte Dateien (leerer Korb) wird der Korb nicht angefasst.

### Ordner-Ziel bei "hierher"-Befehlen

Rechtsklick auf einen einzelnen Ordner: `m_szFolder` wird in `Initialize()` auf den angeklickten Ordner gesetzt (nicht auf das uebergeordnete Verzeichnis). Bei Rechtsklick auf Dateien oder mehrere Elemente bleibt `m_szFolder` das aktuelle Verzeichnis (Elternordner). Die IShellItemArray-Fallback-Logik (Navigationsbereich, virtuelle Ordner etc.) spiegelt dieselbe Logik: Elternverzeichnis aus erstem Item extrahieren, nur bei einzelnem Ordner (`SFGAO_FOLDER`-Pruefung) auf den angeklickten Ordner ueberschreiben.

### BrowseForFolder Guard

"Kopieren nach..." und "Verschieben nach..." verwenden `BrowseForFolder()` mit Re-Entrance-Guard:
- **InterlockedCompareExchange** verhindert gleichzeitige Dialog-Oeffnungen (Re-Entrance waehrend modaler Messagepump)
- **Zeitfenster (500ms)** verhindert sequentielle Mehrfach-Oeffnungen (Explorer ruft InvokeCommand ggf. pro selektiertem Item auf)
- Erster Aufruf erhaelt alle Dateien via HDROP, nachfolgende werden unterdrueckt

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
- **Fenster-Style:** Resizable, kein Minimize/Maximize (`WS_OVERLAPPEDWINDOW & ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX)`)
- **Resize-Layout:** `LayoutControls()` verwendet `BeginDeferWindowPos`/`DeferWindowPos`/`EndDeferWindowPos` fuer atomares Repositionieren aller Child-Controls (ListView + Buttons) plus `InvalidateRect` fuer sauberes Neuzeichnen
- **Icon:** Basket-Icon in Titelleiste via `WM_SETICON` (ICON_SMALL + ICON_BIG, `LR_SHARED`)
- **Persistenz:** Fenstergroesse und Spaltenbreiten werden in Registry gespeichert (`HKCU\Software\CopyBasket\DialogWidth/Height/ColWidth0/ColWidth1`)
- **Pfad-Spalte:** Zweite Spalte zeigt den vollen Dateipfad (nicht nur den Ordner)
- **Spalten-Sortierung:** Klick auf Spaltenueberschrift sortiert alphabetisch (case-insensitive via `_wcsicmp`), erneuter Klick kehrt Richtung um. Sortier-Pfeil im Header via `HDF_SORTUP`/`HDF_SORTDOWN` (natives Windows-Theme). Implementiert mit `ListView_SortItemsEx` und `LVN_COLUMNCLICK`
- **Strg+A:** Selektiert alle Eintraege im ListView (via `LVN_KEYDOWN` + `ListView_SetItemState` mit Index -1)
- **Initialer Fokus:** ListView erhaelt beim Oeffnen sofort den Fokus (`SetFocus` nach `ShowWindow`), sodass Tastaturkuerzel (Strg+A, Entf) direkt nutzbar sind
- **Statusbar:** Native Windows-Statusbar (`STATUSCLASSNAMEW`) am unteren Fensterrand mit `SBARS_SIZEGRIP`. Zeigt Dateianzahl im Korb (z.B. "5 Dateien" / "5 files"). Aktualisiert sich automatisch beim Entfernen von Eintraegen. Layout wird in `LayoutControls()` beruecksichtigt (Statusbar-Hoehe von verfuegbarer Flaeche abgezogen)

### Einstellungen-Dialog

- **Tab Control:** Zwei Registerkarten (Sprache / \u00DCber)
- **Sprache-Tab:** ComboBox mit Deutsch/English, speichert via `SaveLanguageSetting()`
- **\u00DCber-Tab:** Titel (bold, 3px groesser), klickbarer Website-Link (SysLink/WC_LINK), Copyright
- **Website-Link:** `hjs.page.gd/cb` — zentriert via `LM_GETIDEALSIZE`, oeffnet Browser via `ShellExecuteW`, schliesst Dialog nach Klick
- **Bold-Font:** Eigener `LOGFONT` mit `FW_BOLD` + vergroesserter Hoehe (+3px), Freigabe in `WM_DESTROY`

## i18n

- **Architektur:** `StringTable`-Struct mit `const wchar_t*`/`const char*`-Membern, zwei statische Instanzen (s_DE/s_EN), globaler Pointer auf aktive Tabelle
- **Zugriff:** `GetStrings().MemberName` — typ-sicher, Zero-Overhead
- **Spracheinstellung:** Registry `HKCU\Software\CopyBasket\Language` = `"de"` oder `"en"` (Default: Deutsch)
- **Laden:** `LoadLanguageSetting()` im CShellExt-Konstruktor
- **Aendern:** Einstellungen-Dialog → `SaveLanguageSetting()` → sofort wirksam beim naechsten Rechtsklick

## Installer

- **Technologie:** NSIS 3.x (Unicode), Modern UI 2
- **Skript:** `installer\CopyBasket.nsi`
- **Architektur-Erkennung:** Automatisch x64/x86 via `x64.nsh` (`${RunningX64}`)
- **Registrierung:** `regsvr32 /s` wird automatisch beim Installieren ausgefuehrt
- **Deregistrierung:** `regsvr32 /u /s` wird automatisch beim Deinstallieren ausgefuehrt
- **FS-Redirection:** `${DisableX64FSRedirection}` fuer korrekten 64-Bit regsvr32-Aufruf aus 32-Bit NSIS
- **Installationsverzeichnis:** `$PROGRAMFILES64\CopyBasket` (64-Bit) bzw. `$PROGRAMFILES\CopyBasket` (32-Bit)
- **Version:** Automatisch aus `Version.h` extrahiert via `!searchparse`
- **Inhalt:** CopyBasket.dll (architektur-passend), basket.ico, Uninstall.exe (kein CB-CMT.exe — nur in Portable-Version)
- **Deinstallation:** Entfernt Benutzereinstellungen (`HKCU\Software\CopyBasket`) und Benutzerdaten (`%APPDATA%\CopyBasket`)
- **Add/Remove Programs:** Eintrag unter `HKLM\...\Uninstall\CopyBasket`
- **Shell-Benachrichtigung:** `SHChangeNotify` nach (De-)Registrierung
- **Sprachen:** Deutsch + Englisch
- **Output:** `build\CopyBasket-X.Y.Z-setup.exe`
- **Build:** Nur via GitHub Actions (NSIS lokal nicht verfuegbar), getriggert durch Tag-Push (`git tag vX.Y.Z && git push origin vX.Y.Z`)

### Release-Assets (GitHub)

- `CopyBasket_vX.Y.Z.zip` — Portable (DLLs + CB-CMT.exe fuer manuelle Registrierung)
- `CopyBasket-X.Y.Z-setup.exe` — NSIS Installer (regsvr32 automatisch, kein CB-CMT.exe)

### CB-CMT.exe (CopyBasket Context Menu Tool)

- **Zweck:** GUI-Tool zur manuellen Aktivierung/Deaktivierung der Shell Extension (nur Portable-Version)
- **Quellcode:** `regsvr Tool\CopyBasketContextMenu\`
- **Funktionen:**
  - Radio-Buttons: Activate / Deactivate (Statuserkennung via Registry, Vorauswahl)
  - Checkbox: "Delete User Settings" — loescht `HKCU\Software\CopyBasket` und `%APPDATA%\CopyBasket\` (ausgegraut wenn kein Schluessel vorhanden)
  - Warnhinweis bei Checkbox-Aktivierung, Sicherheitsabfrage vor Loeschung
  - DLL-Existenzpruefung vor regsvr32-Aufruf
- **Build:** `MSYS_NO_PATHCONV=1 "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe" "regsvr Tool/CopyBasketContextMenu.sln" /p:Configuration=Release /p:Platform=x64 /verbosity:minimal`
- **Output:** `regsvr Tool\bin\Release\CB-CMT.exe`

## Konventionen

- Kein Precompiled Header
- ASCII-sichere Bezeichner im Code, Umlaute via Unicode-Escapes (z.B. `\u00FC` fuer ue)
- `g_cRef` immer mit `InterlockedIncrement`/`InterlockedDecrement` zugreifen (Data-Race-Schutz wegen Hintergrund-Threads)
- Gemeinsame Logik in Helper-Funktionen extrahieren (z.B. `ExecuteFileOpCOM` fuer IFileOperation)
- Referenzprojekt: `C:\Users\HJS\Claude.ai\wscitecm\` (SciTE Context Menu Extension)

## GitHub

Repository: https://github.com/HJS-cpu/CopyBasket
