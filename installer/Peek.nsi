; Peek Network Security Monitor - NSIS Installer
; Version 2.0.0

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"

; Configuration
Name "Peek Network Security Monitor"
OutFile "Peek-2.0.0-Setup.exe"
InstallDir "$PROGRAMFILES\Peek"
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Peek" "InstallLocation"

; Windows Requirements
RequestExecutionLevel admin

; MUI Settings
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

; Uninstaller Pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Installer Sections
Section "Peek Core" SecCore
  SectionIn RO

  SetOutPath "$INSTDIR"

  ; Copy main executable
  File "peek.exe"
  ${If} ${Errors}
    MessageBox MB_OK "Error copying peek.exe"
  ${EndIf}

  ; Copy daemon
  File "peekd.exe"
  ${If} ${Errors}
    MessageBox MB_OK "Error copying peekd.exe"
  ${EndIf}

  ; Copy updater
  File "updater.exe"
  ${If} ${Errors}
    MessageBox MB_OK "Error copying updater.exe"
  ${EndIf}

  ; Create uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Create registry entries for Add/Remove Programs
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Peek" \
    "DisplayName" "Peek Network Security Monitor"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Peek" \
    "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Peek" \
    "DisplayIcon" "$INSTDIR\peek.exe,0"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Peek" \
    "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Peek" \
    "DisplayVersion" "2.0.0"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Peek" \
    "Publisher" "Théo V."
  WriteRegDWord HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Peek" \
    "NoModify" 1
  WriteRegDWord HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Peek" \
    "NoRepair" 1
SectionEnd

Section "Daemon Service" SecService
  SetOutPath "$INSTDIR"

  ; Install Windows service
  ExecWait '"$INSTDIR\peekd.exe" --install-service'

  ; Start service
  ExecWait 'net start PeekDaemon'
SectionEnd

Section "Start Menu & Desktop Shortcuts" SecShortcuts
  ; Create Start Menu folder
  CreateDirectory "$SMPROGRAMS\Peek"

  ; Create Start Menu shortcuts
  CreateShortCut "$SMPROGRAMS\Peek\Peek.lnk" "$INSTDIR\peek.exe"
  CreateShortCut "$SMPROGRAMS\Peek\Uninstall Peek.lnk" "$INSTDIR\uninstall.exe"

  ; Create Desktop shortcut (optional)
  CreateShortCut "$DESKTOP\Peek.lnk" "$INSTDIR\peek.exe"
SectionEnd

Section "Start with Windows" SecAutoStart
  ; Add to startup registry
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" \
    "Peek" "$INSTDIR\peek.exe"
SectionEnd

Section "WebView2 Runtime Check" SecWebView2
  ; Check if WebView2 is installed
  ReadRegStr $0 HKLM "Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" "pv"

  ${If} $0 == ""
    MessageBox MB_YESNO "WebView2 Runtime is required but not installed.$\n$\nWould you like to download it now?" \
      IDYES DownloadWebView2 IDNO SkipWebView2

    DownloadWebView2:
      ExecShell "open" "https://go.microsoft.com/fwlink/p/?LinkId=2124703"

    SkipWebView2:
  ${EndIf}
SectionEnd

; Section Descriptions
LangString DESC_SecCore ${LANG_ENGLISH} "Core application files"
LangString DESC_SecService ${LANG_ENGLISH} "Install background daemon as Windows service"
LangString DESC_SecShortcuts ${LANG_ENGLISH} "Create Start Menu and Desktop shortcuts"
LangString DESC_SecAutoStart ${LANG_ENGLISH} "Launch Peek at startup"
LangString DESC_SecWebView2 ${LANG_ENGLISH} "Check for WebView2 Runtime"

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} $(DESC_SecCore)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecService} $(DESC_SecService)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} $(DESC_SecShortcuts)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAutoStart} $(DESC_SecAutoStart)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecWebView2} $(DESC_SecWebView2)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Uninstaller Section
Section "Uninstall"
  ; Stop service
  ExecWait 'net stop PeekDaemon'

  ; Remove service
  ExecWait '"$INSTDIR\peekd.exe" --remove-service'

  ; Wait for files to be released
  Sleep 1000

  ; Delete files
  Delete "$INSTDIR\peek.exe"
  Delete "$INSTDIR\peekd.exe"
  Delete "$INSTDIR\updater.exe"
  Delete "$INSTDIR\uninstall.exe"

  ; Remove directory
  RMDir "$INSTDIR"

  ; Remove shortcuts
  Delete "$SMPROGRAMS\Peek\Peek.lnk"
  Delete "$SMPROGRAMS\Peek\Uninstall Peek.lnk"
  RMDir "$SMPROGRAMS\Peek"
  Delete "$DESKTOP\Peek.lnk"

  ; Remove startup entry
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "Peek"

  ; Remove registry entries
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Peek"
SectionEnd

; Version info
VIProductVersion "2.0.0.0"
VIAddVersionKey "ProductName" "Peek"
VIAddVersionKey "ProductVersion" "2.0.0"
VIAddVersionKey "CompanyName" "Théo V."
VIAddVersionKey "FileDescription" "Professional Network Security Monitor"
VIAddVersionKey "FileVersion" "2.0.0"
VIAddVersionKey "LegalCopyright" "(C) 2025"
