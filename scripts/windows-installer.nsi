; NSIS script for the Mnemosyne Windows installer. Not run directly — see
; scripts/package-windows.sh, which stages a working copy of the app (via
; windeployqt6 + DLL bundling) and then invokes makensis against this
; script, passing STAGE_DIR/VERSION/OUT_FILE as command-line defines.
;
; Produces a per-machine installer (Program Files, admin required) with a
; Start Menu shortcut, an optional Desktop shortcut, and a proper
; uninstaller registered in Add/Remove Programs.

!ifndef STAGE_DIR
  !error "STAGE_DIR must be defined (see scripts/package-windows.sh)"
!endif
!ifndef VERSION
  !error "VERSION must be defined (see scripts/package-windows.sh)"
!endif
!ifndef OUT_FILE
  !error "OUT_FILE must be defined (see scripts/package-windows.sh)"
!endif

!include "MUI2.nsh"

Name "Mnemosyne"
OutFile "${OUT_FILE}"
InstallDir "$PROGRAMFILES64\Mnemosyne"
InstallDirRegKey HKLM "Software\Mnemosyne" "InstallDir"
RequestExecutionLevel admin
Unicode true

!define MUI_ICON "..\resources\icons\mnemosyne.ico"
!define MUI_UNICON "..\resources\icons\mnemosyne.ico"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "Mnemosyne"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "GPLv3"
VIAddVersionKey "FileDescription" "Mnemosyne Installer"

Section "Mnemosyne" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${STAGE_DIR}\*.*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\Mnemosyne"
  CreateShortCut "$SMPROGRAMS\Mnemosyne\Mnemosyne.lnk" "$INSTDIR\Mnemosyne.exe"
  CreateShortCut "$SMPROGRAMS\Mnemosyne\Uninstall Mnemosyne.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "Software\Mnemosyne" "InstallDir" "$INSTDIR"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "DisplayName" "Mnemosyne"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "Publisher" "Mnemosyne"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "DisplayIcon" "$INSTDIR\Mnemosyne.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "QuietUninstallString" "$INSTDIR\Uninstall.exe /S"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "NoRepair" 1
SectionEnd

Section "Desktop Shortcut" SecDesktop
  CreateShortCut "$DESKTOP\Mnemosyne.lnk" "$INSTDIR\Mnemosyne.exe"
SectionEnd

Section "Uninstall"
  RMDir /r "$INSTDIR"
  RMDir /r "$SMPROGRAMS\Mnemosyne"
  Delete "$DESKTOP\Mnemosyne.lnk"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne"
  DeleteRegKey HKLM "Software\Mnemosyne"
SectionEnd
