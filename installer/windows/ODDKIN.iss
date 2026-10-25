; ODDKIN Windows installer (Inno Setup 6)
; Build:  iscc /DAppVersion=1.0.0 installer\windows\ODDKIN.iss
; Output: dist\ODDKIN-Installer-Windows.exe
;
; Installs the VST3 to C:\Program Files\Common Files\VST3\ODDKIN.vst3 (where Ableton looks),
; registers an uninstaller in Settings > Apps, and adds Start-menu shortcuts.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define VST3Source "..\..\build\ODDKIN_artefacts\Release\VST3\ODDKIN.vst3"

[Setup]
; never change AppId: it's how upgrades and the uninstaller find each other
AppId={{5931A605-C3F1-41F6-9648-FD3E3E256EEF}
AppName=ODDKIN
AppVersion={#AppVersion}
AppVerName=ODDKIN {#AppVersion}
AppPublisher=Oddkin
VersionInfoVersion={#AppVersion}
VersionInfoDescription=ODDKIN guitar sound explorer installer
DefaultDirName={commonpf64}\Oddkin\ODDKIN
DisableDirPage=yes
UsePreviousAppDir=no
DefaultGroupName=ODDKIN
DisableProgramGroupPage=yes
DisableReadyPage=no
DisableWelcomePage=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
OutputDir=..\..\dist
OutputBaseFilename=ODDKIN-Installer-Windows
SetupIconFile=oddkin.ico
UninstallDisplayIcon={app}\oddkin.ico
UninstallDisplayName=ODDKIN (VST3)
WizardStyle=modern
WizardImageFile=wizard.bmp,wizard@2x.bmp
WizardSmallImageFile=small.bmp,small@2x.bmp
Compression=lzma2/max
SolidCompression=yes
CloseApplications=yes
UsedUserAreasWarning=no
RestartApplications=no

[Messages]
WelcomeLabel2=This installs the ODDKIN guitar sound explorer (VST3) for Ableton Live and other music apps.%n%nIt goes into C:\Program Files\Common Files\VST3, where Ableton looks for plug-ins.%n%nPlease close Ableton before continuing.
FinishedLabelNoIcons=ODDKIN is installed.%n%nIn Ableton: Options > Preferences > Plug-Ins, turn on "Use VST3 Plug-In System Folders" and click Rescan. Then find it under Plug-Ins > Oddkin > ODDKIN.%n%nTo remove it later: Settings > Apps > ODDKIN, or Start menu > ODDKIN > Uninstall ODDKIN.
FinishedLabel=ODDKIN is installed.%n%nIn Ableton: Options > Preferences > Plug-Ins, turn on "Use VST3 Plug-In System Folders" and click Rescan. Then find it under Plug-Ins > Oddkin > ODDKIN.%n%nTo remove it later: Settings > Apps > ODDKIN, or Start menu > ODDKIN > Uninstall ODDKIN.

[Files]
Source: "{#VST3Source}\*"; DestDir: "{commoncf64}\VST3\ODDKIN.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "oddkin.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "Read Me.txt"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
; saved patches live here; never removed automatically (the uninstaller asks)
Name: "{userdocs}\ODDKIN\Presets"; Flags: uninsneveruninstall

[Icons]
Name: "{group}\Uninstall ODDKIN"; Filename: "{uninstallexe}"
Name: "{group}\ODDKIN Presets Folder"; Filename: "{userdocs}\ODDKIN\Presets"
Name: "{group}\ODDKIN Read Me"; Filename: "{app}\Read Me.txt"

[InstallDelete]
; ODDKIN used to be called JACKPOT: same plug-in underneath, so remove the old copy
; (otherwise hosts list it twice). Old saved patches are copied over by the plug-in itself.
Type: filesandordirs; Name: "{commoncf64}\VST3\JACKPOT.vst3"
Type: filesandordirs; Name: "{commonpf64}\Jackpot Audio"
Type: filesandordirs; Name: "{commonprograms}\JACKPOT"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\ODDKIN.vst3"
Type: dirifempty; Name: "{app}"
Type: dirifempty; Name: "{commonpf64}\Oddkin"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Patches: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    Patches := ExpandConstant('{userdocs}\ODDKIN');
    if DirExists(Patches) and not UninstallSilent then
      if MsgBox('ODDKIN has been removed.' + #13#10#13#10 +
                'Do you also want to delete your saved patches?' + #13#10 + '(' + Patches + ')',
                mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(Patches, True, True, True);
  end;
end;
