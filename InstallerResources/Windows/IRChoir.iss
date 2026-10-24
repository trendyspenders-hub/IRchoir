#define ProductVersion "0.3.2"
[Setup]
AppId={{8D0F66C1-AED5-48F1-B706-85E9035CE551}
AppName=I.R.Choir
AppVersion={#ProductVersion}
AppPublisher=IR Audio
DefaultDirName={autopf}\IR Audio\I.R.Choir
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
OutputDir=..\..\dist-windows
OutputBaseFilename=IRChoir-0.3.2-Windows-x64-Setup-unsigned
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\..\LICENSE.txt
UninstallDisplayName=I.R.Choir VST3
[Files]
Source: "..\..\build-windows\IRChoir_artefacts\Release\VST3\IRChoir.vst3\*"; DestDir: "{commoncf64}\VST3\IRChoir.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\Documentation\WINDOWS-INSTALL.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\windows-source\*"; DestDir: "{app}\Source"; Flags: ignoreversion recursesubdirs createallsubdirs
