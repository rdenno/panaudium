; Panaudium Windows installer (Inno Setup 6). Build with:
;   ISCC.exe /DAppVersion=0.2.0 /DBuildDir=..\build installer\panaudium.iss
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build"
#endif
#define Artefacts BuildDir + "\Panaudium_artefacts\Release"

[Setup]
AppId={{6E4B1C2A-7D3F-4B8E-9A51-2C0D9F3E8A71}
AppName=Panaudium
AppVersion={#AppVersion}
AppPublisher=Panaudium (open source)
AppPublisherURL=https://github.com/rdenno/panaudium
DefaultDirName={autopf}\Panaudium
DefaultGroupName=Panaudium
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=..\dist
OutputBaseFilename=Panaudium-{#AppVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Writing to C:\Program Files\Common Files\VST3 (the folder every DAW scans) needs admin rights.
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=Panaudium {#AppVersion}

[Types]
Name: "full"; Description: "VST3 plugin and standalone app"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (installed to Common Files\VST3)"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone application"; Types: full

[Files]
Source: "{#Artefacts}\VST3\Panaudium.vst3\*"; DestDir: "{commoncf64}\VST3\Panaudium.vst3"; Excludes: "*.pdb"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\Panaudium.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\Panaudium"; Filename: "{app}\Panaudium.exe"; Components: standalone
Name: "{group}\Uninstall Panaudium"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Panaudium.vst3"
