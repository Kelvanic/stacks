Unicode true
!include "MUI2.nsh"
Name "Stacks"
OutFile "@OUTPUT@"
InstallDir "$LOCALAPPDATA\Programs\Stacks"
InstallDirRegKey HKCU "Software\Stacks" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
!define MUI_ICON "@ICON@"
!define MUI_UNICON "@ICON@"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Stacks"
    SetShellVarContext current
    SetOutPath "$INSTDIR"
    File /r "@STAGE@\*"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    CreateDirectory "$SMPROGRAMS\Stacks"
    CreateShortcut "$SMPROGRAMS\Stacks\Stacks.lnk" "$INSTDIR\stacks.exe"
    CreateShortcut "$SMPROGRAMS\Stacks\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    CreateShortcut "$DESKTOP\Stacks.lnk" "$INSTDIR\stacks.exe"
    WriteRegStr HKCU "Software\Stacks" "InstallDir" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Stacks" "DisplayName" "Stacks"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Stacks" "DisplayVersion" "@VERSION@"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Stacks" "DisplayIcon" "$INSTDIR\stacks.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Stacks" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Stacks" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Stacks" "NoRepair" 1
SectionEnd

Section "Uninstall"
    SetShellVarContext current
    ; Remove only files shipped by this installer, never user data or arbitrary
    ; files added to the directory. Stacks' library lives in AppData, not here.
@UNINSTALL_FILES@
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    Delete "$DESKTOP\Stacks.lnk"
    Delete "$SMPROGRAMS\Stacks\Stacks.lnk"
    Delete "$SMPROGRAMS\Stacks\Uninstall.lnk"
    RMDir "$SMPROGRAMS\Stacks"
    DeleteRegKey HKCU "Software\Stacks"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Stacks"
SectionEnd
