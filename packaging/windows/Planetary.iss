; Build with Inno Setup 7 after scripts\windows-release.bat.
; Example: ISCC.exe packaging\windows\Planetary.iss
; Override inputs with /DStageDir=... /DAppVersion=... /DInstallerOutputDir=...
#define RepoRoot AddBackslash(SourcePath) + "..\.."
#ifndef StageDir
  #define StageDir RepoRoot + "\dist\Planetary"
#endif
#ifndef AppVersion
  #define VersionFile FileOpen(RepoRoot + "\build-release\llvm-mingw\planetary-version.txt")
  #if VersionFile == 0
    #error Build the Windows release first, or supply /DAppVersion=major.minor.patch.build
  #endif
  #define AppVersion Trim(FileRead(VersionFile))
  #expr FileClose(VersionFile)
#endif
#ifndef InstallerOutputDir
  #define InstallerOutputDir RepoRoot + "\dist"
#endif
#if !FileExists(StageDir + "\Planetary.exe")
  #error Staged Planetary.exe missing. Run scripts\windows-release.bat first.
#endif
#if !FileExists(StageDir + "\tls\qschannelbackend.dll")
  #error Staged Schannel TLS backend missing.
#endif
#if !FileExists(StageDir + "\geoip\country.mmdb")
  #error Staged GeoIP database missing.
#endif

[Setup]
; Keep this identity stable across future versions to support upgrades.
AppId={{69BBAC64-422E-414E-AF63-4D924D74A7B8}
AppName=Planetary
AppVersion={#AppVersion}
AppPublisher=mvgrafx
AppPublisherURL=https://planetary.mvgrafx.net/
AppSupportURL=https://planetary.mvgrafx.net/
AppUpdatesURL=https://planetary.mvgrafx.net/
VersionInfoVersion={#AppVersion}
SetupArchitecture=x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
DefaultDirName={autopf}\Planetary
DefaultGroupName=Planetary
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UsePreviousPrivileges=yes
LicenseFile={#StageDir}\licenses\LICENSE
InfoAfterFile={#StageDir}\licenses\THIRD_PARTY_NOTICES.txt
SetupIconFile={#SourcePath}planetary.ico
UninstallDisplayIcon={app}\Planetary.exe
OutputDir={#InstallerOutputDir}
OutputBaseFilename=Planetary-{#AppVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes
RestartApplications=no
#ifdef SigningEnabled
SignTool=planetary
SignedUninstaller=yes
SignedUninstallerDir={#InstallerOutputDir}\signed-uninstaller
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked
Name: "associations"; Description: "Register Planetary for .torrent files and magnet links"; GroupDescription: "File and link handling:"; Flags: unchecked

[Files]
; Copy the validated staging tree, including plugins, translations, data and licenses.
; TLS uses Windows Schannel; do not introduce an OpenSSL installation dependency.
Source: "{#StageDir}\*"; DestDir: "{app}"; Excludes: "qopensslbackend.dll,libssl*.dll,libcrypto*.dll"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Planetary"; Filename: "{app}\Planetary.exe"; WorkingDir: "{app}"
Name: "{group}\Third-party notices"; Filename: "{app}\licenses\THIRD_PARTY_NOTICES.txt"
Name: "{autodesktop}\Planetary"; Filename: "{app}\Planetary.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]
; HKA selects HKCU for per-user installs and HKLM for all-users installs.
; Register owned ProgIDs and capabilities without replacing Windows UserChoice
; or another application's global magnet command. Users choose defaults in Windows.
Root: HKA; Subkey: "Software\Classes\Planetary.Torrent"; ValueType: string; ValueName: ""; ValueData: "Planetary torrent file"; Flags: uninsdeletekey; Tasks: associations
Root: HKA; Subkey: "Software\Classes\Planetary.Torrent\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Planetary.exe,0"; Tasks: associations
Root: HKA; Subkey: "Software\Classes\Planetary.Torrent\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Planetary.exe"" ""%1"""; Tasks: associations
Root: HKA; Subkey: "Software\Classes\.torrent\OpenWithProgids"; ValueType: string; ValueName: "Planetary.Torrent"; ValueData: ""; Flags: uninsdeletevalue uninsdeletekeyifempty; Tasks: associations
Root: HKA; Subkey: "Software\Classes\Planetary.Magnet"; ValueType: string; ValueName: ""; ValueData: "Planetary magnet link"; Flags: uninsdeletekey; Tasks: associations
Root: HKA; Subkey: "Software\Classes\Planetary.Magnet"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""; Tasks: associations
Root: HKA; Subkey: "Software\Classes\Planetary.Magnet\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Planetary.exe,0"; Tasks: associations
Root: HKA; Subkey: "Software\Classes\Planetary.Magnet\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Planetary.exe"" ""%1"""; Tasks: associations
Root: HKA; Subkey: "Software\Planetary\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "Planetary"; Flags: uninsdeletekey; Tasks: associations
Root: HKA; Subkey: "Software\Planetary\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Remote torrent client"; Tasks: associations
Root: HKA; Subkey: "Software\Planetary\Capabilities\FileAssociations"; ValueType: string; ValueName: ".torrent"; ValueData: "Planetary.Torrent"; Tasks: associations
Root: HKA; Subkey: "Software\Planetary\Capabilities\URLAssociations"; ValueType: string; ValueName: "magnet"; ValueData: "Planetary.Magnet"; Tasks: associations
Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "Planetary"; ValueData: "Software\Planetary\Capabilities"; Flags: uninsdeletevalue; Tasks: associations

[Run]
Filename: "{app}\Planetary.exe"; Description: "Launch Planetary"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent runasoriginaluser

