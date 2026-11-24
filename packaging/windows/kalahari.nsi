; The installer of Kalahari for Windows (NSIS 3, Unicode). scripts/package.py compiles it from the
; folder of the program (cmake --install) with:
;
;   makensis -INPUTCHARSET UTF8 -DVERSION=0.4.0-alpha -DVERSION_NUMBER=0.4.0.0
;            -DSTAGE=<folder of the program> -DOUTFILE=<installer.exe>
;            -DFILES=<install-files.nsh> -DUNFILES=<uninstall-files.nsh> kalahari.nsi
;
; FILES and UNFILES list the files of the folder (package.py writes them), so the uninstaller
; removes only what the installer put there. The installer speaks Polish or English, as Windows
; does. It installs for the writer alone (no administrator: %LOCALAPPDATA%\Programs\Kalahari)
; or for all users (Program Files), adds Kalahari to the Start menu, optionally to the desktop,
; and makes it the program of its files: the books (.klh) and the documents (.kchapter). /S
; installs silently; /AllUsers or /CurrentUser choose the installation without the page.
;
; The files of Kalahari are registered as Microsoft describes it for Windows 8 and later: a ProgID
; for each type, the extension with that ProgID and in its OpenWithProgids, the Capabilities of
; the program and its name in RegisteredApplications (Settings > Apps > Default apps > Kalahari).
; The program writes the same values for the writer (Settings > Files > File Associations,
; file_associations.cpp); the uninstaller removes both.

!ifndef VERSION | VERSION_NUMBER | STAGE | OUTFILE | FILES | UNFILES
  !error "Give VERSION, VERSION_NUMBER, STAGE, OUTFILE, FILES and UNFILES (scripts/package.py)"
!endif

Unicode true
SetCompressor /SOLID lzma
ManifestDPIAware true

!define APP_NAME "Kalahari"
!define APP_EXE "kalahari.exe"
!define PUBLISHER "Bartosz W. Warzocha & Kalahari Team"
!define URL "https://github.com/bartoszwarzocha/kalahari"
!define REGISTRY_KEY "Software\Kalahari"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kalahari"
!define BOOK_TYPE "Kalahari.Book"
!define DOCUMENT_TYPE "Kalahari.Document"
!define CAPABILITIES_KEY "${REGISTRY_KEY}\Capabilities"
!define REGISTERED_APPLICATIONS_KEY "Software\RegisteredApplications"

Name "${APP_NAME}"
OutFile "${OUTFILE}"
BrandingText "${APP_NAME} ${VERSION}"

; ----------------------------------------------------------------------------------------------
; For the writer alone or for all users
; ----------------------------------------------------------------------------------------------
!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_MUI
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_INSTALLMODE_DEFAULT_CURRENTUSER
!define MULTIUSER_USE_PROGRAMFILES64
!define MULTIUSER_INSTALLMODE_INSTDIR "${APP_NAME}"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_KEY "${REGISTRY_KEY}"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_VALUENAME "InstallLocation"
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_KEY "${REGISTRY_KEY}"
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_VALUENAME "InstallMode"
!include "MultiUser.nsh"
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

; ----------------------------------------------------------------------------------------------
; Pages
; ----------------------------------------------------------------------------------------------
!define MUI_ICON "${__FILEDIR__}\..\icons\kalahari.ico"
!define MUI_UNICON "${__FILEDIR__}\..\icons\kalahari.ico"
!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_NODESC

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}\LICENSE.txt"
!insertmacro MULTIUSER_PAGE_INSTALLMODE
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; English first: the language of a Windows the installer has no texts for
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Polish"

LangString SECTION_PROGRAM ${LANG_ENGLISH} "Kalahari"
LangString SECTION_PROGRAM ${LANG_POLISH} "Kalahari"
LangString SECTION_DESKTOP ${LANG_ENGLISH} "Shortcut on the desktop"
LangString SECTION_DESKTOP ${LANG_POLISH} "Skrót na pulpicie"
LangString SECTION_BOOKS ${LANG_ENGLISH} "Open books (.klh) and documents (.kchapter) with Kalahari"
LangString SECTION_BOOKS ${LANG_POLISH} "Otwieranie książek (.klh) i dokumentów (.kchapter) w programie Kalahari"
LangString BOOK_TYPE_NAME ${LANG_ENGLISH} "Kalahari book"
LangString BOOK_TYPE_NAME ${LANG_POLISH} "Książka programu Kalahari"
LangString DOCUMENT_TYPE_NAME ${LANG_ENGLISH} "Kalahari document"
LangString DOCUMENT_TYPE_NAME ${LANG_POLISH} "Dokument programu Kalahari"
LangString APP_DESCRIPTION ${LANG_ENGLISH} "Write, plan and publish books"
LangString APP_DESCRIPTION ${LANG_POLISH} "Pisanie, planowanie i wydawanie książek"
LangString NEEDS_64_BIT ${LANG_ENGLISH} "Kalahari needs a 64-bit Windows."
LangString NEEDS_64_BIT ${LANG_POLISH} "Kalahari wymaga 64-bitowego systemu Windows."
LangString PROGRAM_RUNS ${LANG_ENGLISH} "Kalahari is running. Close it and click Retry."
LangString PROGRAM_RUNS ${LANG_POLISH} "Program Kalahari działa. Zamknij go i kliknij Ponów."

VIProductVersion "${VERSION_NUMBER}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "${APP_NAME}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "${APP_NAME} ${VERSION} Setup"
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "${PUBLISHER}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "${PUBLISHER}"

; ----------------------------------------------------------------------------------------------
; Functions
; ----------------------------------------------------------------------------------------------

; Kalahari keeps its files open while it runs: the installer and the uninstaller wait for it
!macro WAIT_FOR_PROGRAM
  ${If} ${FileExists} "$INSTDIR\${APP_EXE}"
    ${Do}
      ClearErrors
      FileOpen $0 "$INSTDIR\${APP_EXE}" a
      ${IfNot} ${Errors}
        FileClose $0
        ${Break}
      ${EndIf}
      ; Retry jumps over Abort to the end of the loop: the program file is tried again
      MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "$(PROGRAM_RUNS)" /SD IDCANCEL IDRETRY +2
      Abort
    ${Loop}
  ${EndIf}
!macroend

;  A type of the files of Kalahari: extension EXT (".klh"), ProgID TYPE and its name NAME
!macro REGISTER_FILE_TYPE EXT TYPE NAME
  WriteRegStr SHCTX "Software\Classes\${TYPE}" "" "${NAME}"
  WriteRegStr SHCTX "Software\Classes\${TYPE}\DefaultIcon" "" "$INSTDIR\${APP_EXE},0"
  WriteRegStr SHCTX "Software\Classes\${TYPE}\shell\open\command" "" '"$INSTDIR\${APP_EXE}" "%1"'
  WriteRegStr SHCTX "Software\Classes\${EXT}" "" "${TYPE}"
  WriteRegStr SHCTX "Software\Classes\${EXT}\OpenWithProgids" "${TYPE}" ""
  WriteRegStr SHCTX "${CAPABILITIES_KEY}\FileAssociations" "${EXT}" "${TYPE}"
!macroend

; The type TYPE of extension EXT, under ROOT (SHCTX, or HKCU: what the program wrote for the
; writer): the extension goes when it is still Kalahari's, else only its entry of Kalahari
!macro UNREGISTER_FILE_TYPE ROOT EXT TYPE
  ReadRegStr $0 ${ROOT} "Software\Classes\${EXT}" ""
  ${If} $0 == "${TYPE}"
    DeleteRegKey ${ROOT} "Software\Classes\${EXT}"
  ${Else}
    DeleteRegValue ${ROOT} "Software\Classes\${EXT}\OpenWithProgids" "${TYPE}"
  ${EndIf}
  DeleteRegKey ${ROOT} "Software\Classes\${TYPE}"
!macroend

; All the registration of the files of Kalahari under ROOT
!macro UNREGISTER_FILE_TYPES ROOT
  !insertmacro UNREGISTER_FILE_TYPE ${ROOT} ".klh" "${BOOK_TYPE}"
  !insertmacro UNREGISTER_FILE_TYPE ${ROOT} ".kchapter" "${DOCUMENT_TYPE}"
  DeleteRegValue ${ROOT} "${REGISTERED_APPLICATIONS_KEY}" "${APP_NAME}"
  DeleteRegKey ${ROOT} "${CAPABILITIES_KEY}"
!macroend

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "$(NEEDS_64_BIT)" /SD IDOK
    Abort
  ${EndIf}
  SetRegView 64
  !insertmacro MULTIUSER_INIT
FunctionEnd

Function un.onInit
  SetRegView 64
  !insertmacro MULTIUSER_UNINIT
FunctionEnd

; ----------------------------------------------------------------------------------------------
; Installation
; ----------------------------------------------------------------------------------------------
Section "!$(SECTION_PROGRAM)" SectionProgram
  SectionIn RO
  !insertmacro WAIT_FOR_PROGRAM

  ; The files of an earlier version go first, by its own list
  ${If} ${FileExists} "$INSTDIR\uninstall.exe"
    ExecWait '"$INSTDIR\uninstall.exe" /S /$MultiUser.InstallMode _?=$INSTDIR'
  ${EndIf}

  !include "${FILES}"

  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateShortCut "$SMPROGRAMS\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"

  WriteRegStr SHCTX "${REGISTRY_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr SHCTX "${REGISTRY_KEY}" "InstallMode" "$MultiUser.InstallMode"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE},0"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "URLInfoAbout" "${URL}"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe" /$MultiUser.InstallMode'
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S /$MultiUser.InstallMode'
  WriteRegDWORD SHCTX "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD SHCTX "${UNINSTALL_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  WriteRegDWORD SHCTX "${UNINSTALL_KEY}" "EstimatedSize" "$0"
SectionEnd

Section /o "$(SECTION_DESKTOP)" SectionDesktop
  CreateShortCut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
SectionEnd

Section "$(SECTION_BOOKS)" SectionBooks
  !insertmacro REGISTER_FILE_TYPE ".klh" "${BOOK_TYPE}" "$(BOOK_TYPE_NAME)"
  !insertmacro REGISTER_FILE_TYPE ".kchapter" "${DOCUMENT_TYPE}" "$(DOCUMENT_TYPE_NAME)"
  WriteRegStr SHCTX "${CAPABILITIES_KEY}" "ApplicationName" "${APP_NAME}"
  WriteRegStr SHCTX "${CAPABILITIES_KEY}" "ApplicationDescription" "$(APP_DESCRIPTION)"
  WriteRegStr SHCTX "${REGISTERED_APPLICATIONS_KEY}" "${APP_NAME}" "${CAPABILITIES_KEY}"
  WriteRegStr SHCTX "${REGISTRY_KEY}" "BookFiles" "1"
  ; Explorer shows the new types of the files at once
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

; ----------------------------------------------------------------------------------------------
; Uninstallation
; ----------------------------------------------------------------------------------------------
Section "Uninstall"
  !insertmacro WAIT_FOR_PROGRAM

  Delete "$SMPROGRAMS\${APP_NAME}.lnk"
  Delete "$DESKTOP\${APP_NAME}.lnk"

  ; The types of the files; for all users also what the program wrote for the writer who
  ; uninstalls it (for the writer alone SHCTX is HKCU already)
  !insertmacro UNREGISTER_FILE_TYPES SHCTX
  ${If} $MultiUser.InstallMode == "AllUsers"
    !insertmacro UNREGISTER_FILE_TYPES HKCU
  ${EndIf}
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'

  !include "${UNFILES}"
  Delete "$INSTDIR\uninstall.exe"
  ; The log of the program, when it wrote it here (an installation for the writer alone)
  Delete "$INSTDIR\kalahari.log"
  RMDir "$INSTDIR"

  DeleteRegKey SHCTX "${UNINSTALL_KEY}"
  DeleteRegKey SHCTX "${REGISTRY_KEY}"
SectionEnd
