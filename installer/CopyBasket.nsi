;--------------------------------
; CopyBasket - NSIS Installer Script
; Modern NSIS 3.x Unicode build
;--------------------------------

Unicode True

!include "MUI2.nsh"
!include "x64.nsh"

;--------------------------------
; Version from source

!searchparse /file "..\CopyBasket\Version.h" '#define COPYBASKET_VERSION_MAJOR ' VER_MAJOR
!searchparse /file "..\CopyBasket\Version.h" '#define COPYBASKET_VERSION_MINOR ' VER_MINOR
!searchparse /file "..\CopyBasket\Version.h" '#define COPYBASKET_VERSION_PATCH ' VER_PATCH

;--------------------------------
; General

!define PRODUCT_NAME "CopyBasket"
!define PRODUCT_PUBLISHER "HJS"
!define PRODUCT_WEB_SITE "https://github.com/HJS-cpu/CopyBasket"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

SetCompressor lzma
RequestExecutionLevel admin

; Name and file
Name "${PRODUCT_NAME} v${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}"
OutFile "..\build\CopyBasket-${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}-setup.exe"

; Default installation folder
InstallDir "$PROGRAMFILES\CopyBasket"

; Get installation folder from registry if available
InstallDirRegKey HKLM "Software\${PRODUCT_NAME}" "InstallDir"

;--------------------------------
; Interface Settings

!define MUI_ABORTWARNING
!define MUI_ICON "..\Res\basket.ico"
!define MUI_UNICON "..\Res\basket.ico"

;--------------------------------
; Pages

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
; Languages

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "German"

;--------------------------------
; Installer Initialization

Function .onInit
  ${If} ${RunningX64}
    StrCpy $INSTDIR "$PROGRAMFILES64\CopyBasket"
  ${EndIf}
FunctionEnd

;--------------------------------
; Installer Section

Section "CopyBasket" SecCore

  SectionIn RO
  SetOutPath "$INSTDIR"

  ; Unregister previous version (if any, /s suppresses errors)
  IfFileExists "$INSTDIR\CopyBasket.dll" 0 skip_unreg
    ${If} ${RunningX64}
      ${DisableX64FSRedirection}
      ExecWait 'regsvr32 /u /s "$INSTDIR\CopyBasket.dll"'
      ${EnableX64FSRedirection}
    ${Else}
      ExecWait 'regsvr32 /u /s "$INSTDIR\CopyBasket.dll"'
    ${EndIf}
  skip_unreg:

  ; Install architecture-appropriate DLL
  ${If} ${RunningX64}
    File /oname=CopyBasket.dll "..\x64\Release\CopyBasket.dll"
  ${Else}
    File /oname=CopyBasket.dll "..\Release\CopyBasket.dll"
  ${EndIf}

  ; Register shell extension
  ${If} ${RunningX64}
    ${DisableX64FSRedirection}
    ExecWait 'regsvr32 /s "$INSTDIR\CopyBasket.dll"'
    ${EnableX64FSRedirection}
  ${Else}
    ExecWait 'regsvr32 /s "$INSTDIR\CopyBasket.dll"'
  ${EndIf}

  ; Notify shell of changes
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0x0000, p 0, p 0)'

  ; Icon
  File "..\Res\basket.ico"

  ; Store installation folder
  WriteRegStr HKLM "Software\${PRODUCT_NAME}" "InstallDir" $INSTDIR

  ; Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Add/Remove Programs entry
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME} v${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\basket.ico"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoRepair" 1

  ; Estimate installed size (in KB)
  SectionGetSize ${SecCore} $0
  WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "EstimatedSize" $0

SectionEnd

;--------------------------------
; Uninstaller Section

Section "Uninstall"

  ; Unregister shell extension
  ${If} ${RunningX64}
    ${DisableX64FSRedirection}
    ExecWait 'regsvr32 /u /s "$INSTDIR\CopyBasket.dll"'
    ${EnableX64FSRedirection}
  ${Else}
    ExecWait 'regsvr32 /u /s "$INSTDIR\CopyBasket.dll"'
  ${EndIf}

  ; Notify shell of changes
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0x0000, p 0, p 0)'

  ; Remove files
  Delete "$INSTDIR\CopyBasket.dll"
  Delete "$INSTDIR\basket.ico"
  Delete "$INSTDIR\Uninstall.exe"

  ; Remove user data (%APPDATA%\CopyBasket)
  RMDir /r "$APPDATA\CopyBasket"

  ; Remove registry keys
  DeleteRegKey HKCU "Software\${PRODUCT_NAME}"
  DeleteRegKey HKLM "Software\${PRODUCT_NAME}"
  DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"

  ; Remove installation directory (only if empty)
  RMDir "$INSTDIR"

SectionEnd
