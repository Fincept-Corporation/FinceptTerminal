[Setup]
AppName=Fincept Terminal
AppVersion=0.1.4
DefaultDirName={autopf}\Fincept Terminal
DefaultGroupName=Fincept Terminal
OutputBaseFilename=FinceptTerminal_Setup
Compression=lzma
SolidCompression=yes

[Files]
Source: "dist\FinceptTerminal.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\README.md"; DestDir: "{app}"

[Icons]
Name: "{group}\Fincept Terminal"; Filename: "{app}\FinceptTerminal.exe"
Name: "{group}\Uninstall Fincept Terminal"; Filename: "{uninstallexe}"
Name: "{commondesktop}\Fincept Terminal"; Filename: "{app}\FinceptTerminal.exe"

[Run]
Filename: "{app}\FinceptTerminal.exe"; Description: "Launch Fincept Terminal"; Flags: nowait postinstall

[UninstallDelete]
Type: filesandordirs; Name: "{app}\themes"
