; Comp Sage - Windows installer (Inno Setup)
; Built by GitHub Actions on windows-latest

#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif

[Setup]
AppId={{B2C3D4E5-CMPS-4A5B-9C0D-FEARESQ100}}
AppName=Comp Sage
AppVersion={#MyAppVersion}
AppVerName=Comp Sage {#MyAppVersion} by Fear Escape
AppPublisher=Fear Escape
DefaultDirName={autopf}\Fear Escape\CompSage
DefaultGroupName=Fear Escape\CompSage
OutputDir=..\dist-win
OutputBaseFilename=CompSage-{#MyAppVersion}-Windows-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ShowLanguageDialog=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\SmartComp_artefacts\Release\VST3\SmartComp.vst3\*"; DestDir: "{commoncf64}\VST3\SmartComp.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\SmartComp_artefacts\Release\Standalone\SmartComp.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Comp Sage (Standalone)"; Filename: "{app}\SmartComp.exe"
Name: "{group}\Uninstall Comp Sage"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\SmartComp.exe"; Description: "Launch Comp Sage standalone"; Flags: nowait postinstall skipifsilent unchecked
