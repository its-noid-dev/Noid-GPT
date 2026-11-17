; Inno Setup script voor NoidGPT
; Bouwen:  ISCC.exe installer\NoidGPT.iss   (of gebruik Build-Release.ps1)
; Verwacht dat NoidGPT.exe in dist\app\ staat (Build-Release.ps1 doet dit).

#define MyAppName "NoidGPT"
#define MyAppVersion "1.3.0"
#define MyAppExeName "NoidGPT.exe"

[Setup]
AppId={{B6A3F0C2-5E1D-4B7A-9C48-3D2E7A1F9B50}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=NoidGPT
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=NoidGPT-Setup
SetupIconFile=..\NoidGPT\NoidGPT.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
CloseApplications=force
CloseApplicationsFilter=NoidGPT.exe,open-webui.exe,ollama.exe,python.exe,uv.exe
RestartApplications=no
Uninstallable=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
PrivilegesRequired=lowest

[Languages]
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

; Schone herinstallatie van uitsluitend door NoidGPT beheerde programma-/runtimebestanden.
; Ollama-programma en modellen blijven behouden. Oude Open WebUI/Python/uv/WebView2-runtime en bijbehorende data worden verwijderd.
[InstallDelete]
Type: filesandordirs; Name: "{app}\scripts"
Type: files; Name: "{app}\NoidGPT.exe"
Type: filesandordirs; Name: "{localappdata}\NoidGPT\open-webui-venv"
Type: filesandordirs; Name: "{localappdata}\NoidGPT\python-runtime"
Type: filesandordirs; Name: "{localappdata}\NoidGPT\uv-cache"
Type: filesandordirs; Name: "{localappdata}\NoidGPT\tools\uv"
Type: filesandordirs; Name: "{localappdata}\NoidGPT\tmp-openwebui-setup"
Type: filesandordirs; Name: "{localappdata}\NoidGPT\WebView2Profile"
Type: files; Name: "{localappdata}\NoidGPT\webui.db"
Type: filesandordirs; Name: "{localappdata}\NoidGPT\uploads"
Type: filesandordirs; Name: "{localappdata}\NoidGPT\cache"
Type: filesandordirs; Name: "{localappdata}\NoidGPT\vector_db"
Type: filesandordirs; Name: "{localappdata}\NoidGPT\ollama"
Type: files; Name: "{localappdata}\NoidGPT\ollama.zip"

[Files]
Source: "..\dist\app\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

; Bewaar gebruikersdata in %LOCALAPPDATA%\NoidGPT tijdens upgrades.
