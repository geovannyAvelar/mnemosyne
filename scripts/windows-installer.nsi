; NSIS script for the Mnemosyne Windows installer. Not run directly — see
; scripts/package-windows.sh, which stages a working copy of the app (via
; windeployqt6 + DLL bundling) and then invokes makensis against this
; script, passing STAGE_DIR/VERSION/OUT_FILE as command-line defines.
;
; Uses MultiUser.nsh so the installer works two ways:
;  - No admin rights: installs per-user under
;    $LOCALAPPDATA\Programs\Mnemosyne, registers in HKCU. No UAC prompt.
;  - Run elevated / as admin: installer offers an "all users" mode into
;    Program Files, registered in HKLM.
; Either way it produces a Start Menu shortcut, an optional Desktop
; shortcut (Components page), and a proper uninstaller registered in
; Add/Remove Programs.

!ifndef STAGE_DIR
  !error "STAGE_DIR must be defined (see scripts/package-windows.sh)"
!endif
!ifndef VERSION
  !error "VERSION must be defined (see scripts/package-windows.sh)"
!endif
!ifndef OUT_FILE
  !error "OUT_FILE must be defined (see scripts/package-windows.sh)"
!endif

; --- MultiUser setup (must come before MultiUser.nsh / MUI2.nsh) ---
!define MULTIUSER_EXECUTIONLEVEL Standard
!define MULTIUSER_INSTALLMODE_ALLOW_ELEVATION 1
!define MULTIUSER_INSTALLMODE_ALLOW_ELEVATION_IF_SILENT 0
!define MULTIUSER_INSTALLMODE_DEFAULT_CURRENTUSER
!define MULTIUSER_MUI
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_INSTALLMODE_INSTDIR "Mnemosyne"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_KEY "Software\Mnemosyne"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_VALUENAME "InstallDir"
!include "MultiUser.nsh"
!include "MUI2.nsh"
!include "LogicLib.nsh"

Name "Mnemosyne"
OutFile "${OUT_FILE}"
Unicode true

!define MUI_ICON "..\resources\icons\mnemosyne.ico"
!define MUI_UNICON "..\resources\icons\mnemosyne.ico"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MULTIUSER_PAGE_INSTALLMODE
!insertmacro MUI_PAGE_COMPONENTS
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

Function .onInit
  !insertmacro MULTIUSER_INIT
FunctionEnd

Function un.onInit
  !insertmacro MULTIUSER_UNINIT
FunctionEnd

Section "Mnemosyne" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${STAGE_DIR}\*.*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\Mnemosyne"
  CreateShortCut "$SMPROGRAMS\Mnemosyne\Mnemosyne.lnk" "$INSTDIR\Mnemosyne.exe"
  CreateShortCut "$SMPROGRAMS\Mnemosyne\Uninstall Mnemosyne.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr SHCTX "Software\Mnemosyne" "InstallDir" "$INSTDIR"

  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "DisplayName" "Mnemosyne"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "DisplayVersion" "${VERSION}"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "Publisher" "Mnemosyne"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "DisplayIcon" "$INSTDIR\Mnemosyne.exe"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "QuietUninstallString" "$INSTDIR\Uninstall.exe /S"
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "NoModify" 1
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne" "NoRepair" 1
SectionEnd

Section "Desktop Shortcut" SecDesktop
  CreateShortCut "$DESKTOP\Mnemosyne.lnk" "$INSTDIR\Mnemosyne.exe"
SectionEnd

; Best-effort: on Windows 10/11 this wins only if the user has never picked
; a default for .pdf before. Once a UserChoice hash exists in
; HKCU\...\FileExts\.pdf, only Settings > Default apps (or the user
; clicking through the "How do you want to open this?" prompt) can change
; it — that hash is signed per-user and installers can't write it.
Section /o "Set as default PDF reader" SecPdfDefault
  WriteRegStr SHCTX "Software\Classes\Mnemosyne.pdf" "" "Mnemosyne PDF Document"
  WriteRegStr SHCTX "Software\Classes\Mnemosyne.pdf\DefaultIcon" "" "$INSTDIR\Mnemosyne.exe,0"
  WriteRegStr SHCTX "Software\Classes\Mnemosyne.pdf\shell\open\command" "" '"$INSTDIR\Mnemosyne.exe" "%1"'
  WriteRegStr SHCTX "Software\Classes\.pdf" "" "Mnemosyne.pdf"

  ; Per-machine installs (SHCTX = HKLM) also need the per-user override
  ; cleared, since Windows prefers HKCU\Software\Classes when present.
  DeleteRegKey HKCU "Software\Classes\.pdf"

  ; Tell Explorer the association changed (SHCNE_ASSOCCHANGED, SHCNF_IDLIST).
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
SectionEnd

Section "Uninstall"
  RMDir /r "$INSTDIR"
  RMDir /r "$SMPROGRAMS\Mnemosyne"
  Delete "$DESKTOP\Mnemosyne.lnk"
  DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Mnemosyne"
  DeleteRegKey SHCTX "Software\Mnemosyne"

  ; Only remove the .pdf association if it's still ours — don't clobber
  ; another reader the user may have set as default since.
  ReadRegStr $0 SHCTX "Software\Classes\.pdf" ""
  ${If} $0 == "Mnemosyne.pdf"
    DeleteRegKey SHCTX "Software\Classes\.pdf"
  ${EndIf}
  DeleteRegKey SHCTX "Software\Classes\Mnemosyne.pdf"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
SectionEnd
