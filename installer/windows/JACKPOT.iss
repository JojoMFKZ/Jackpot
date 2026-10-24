; JACKPOT Windows installer (Inno Setup 6)
; Build:  iscc /DAppVersion=1.0.0 installer\windows\JACKPOT.iss
; Output: dist\JACKPOT-Installer-Windows.exe
;
; Installs the VST3 to C:\Program Files\Common Files\VST3\JACKPOT.vst3 (where Ableton looks),
; registers an uninstaller in Settings > Apps, and adds Start-menu shortcuts.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define VST3Source "..\..\build\JACKPOT_artefacts\Release\VST3\JACKPOT.vst3"

[Setup]
; never change AppId: it's how upgrades and the uninstaller find each other
AppId={{5931A605-C3F1-41F6-9648-FD3E3E256EEF}
AppName=JACKPOT
AppVersion={#AppVersion}
AppVerName=JACKPOT {#AppVersion}
AppPublisher=Jackpot Audio
VersionInfoVersion={#AppVersion}
VersionInfoDescription=JACKPOT guitar sound explorer installer
DefaultDirName={commonpf64}\Jackpot Audio\JACKPOT
DisableDirPage=yes
DefaultGroupName=JACKPOT
DisableProgramGroupPage=yes
DisableReadyPage=no
DisableWelcomePage=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
OutputDir=..\..\dist
OutputBaseFilename=JACKPOT-Installer-Windows
SetupIconFile=jackpot.ico
UninstallDisplayIcon={app}\jackpot.ico
UninstallDisplayName=JACKPOT (VST3)
WizardStyle=modern
WizardImageFile=wizard.bmp,wizard@2x.bmp
WizardSmallImageFile=small.bmp,small@2x.bmp
Compression=lzma2/max
SolidCompression=yes
CloseApplications=yes
UsedUserAreasWarning=no
RestartApplications=no

[Messages]
WelcomeLabel2=This installs the JACKPOT guitar sound explorer (VST3) for Ableton Live and other music apps.%n%nIt goes into C:\Program Files\Common Files\VST3, where Ableton looks for plug-ins.%n%nPlease close Ableton before continuing.
FinishedLabelNoIcons=JACKPOT is installed.%n%nIn Ableton: Options > Preferences > Plug-Ins, turn on "Use VST3 Plug-In System Folders" and click Rescan. Then find it under Plug-Ins > Jackpot Audio > JACKPOT.%n%nTo remove it later: Settings > Apps > JACKPOT, or Start menu > JACKPOT > Uninstall JACKPOT.
FinishedLabel=JACKPOT is installed.%n%nIn Ableton: Options > Preferences > Plug-Ins, turn on "Use VST3 Plug-In System Folders" and click Rescan. Then find it under Plug-Ins > Jackpot Audio > JACKPOT.%n%nTo remove it later: Settings > Apps > JACKPOT, or Start menu > JACKPOT > Uninstall JACKPOT.

[Files]
Source: "{#VST3Source}\*"; DestDir: "{commoncf64}\VST3\JACKPOT.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "jackpot.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "Read Me.txt"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
; saved patches live here; never removed automatically (the uninstaller asks)
Name: "{userdocs}\JACKPOT\Presets"; Flags: uninsneveruninstall

[Icons]
Name: "{group}\Uninstall JACKPOT"; Filename: "{uninstallexe}"
Name: "{group}\JACKPOT Presets Folder"; Filename: "{userdocs}\JACKPOT\Presets"
Name: "{group}\JACKPOT Read Me"; Filename: "{app}\Read Me.txt"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\JACKPOT.vst3"
Type: dirifempty; Name: "{app}"
Type: dirifempty; Name: "{commonpf64}\Jackpot Audio"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Patches: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    Patches := ExpandConstant('{userdocs}\JACKPOT');
    if DirExists(Patches) and not UninstallSilent then
      if MsgBox('JACKPOT has been removed.' + #13#10#13#10 +
                'Do you also want to delete your saved patches?' + #13#10 + '(' + Patches + ')',
                mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(Patches, True, True, True);
  end;
end;
