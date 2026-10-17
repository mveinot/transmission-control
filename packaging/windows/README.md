# Windows packaging handoff

This document is a handoff for continuing Planetary's Windows distribution work
on a real Windows system or Windows VM. The current Linux workspace has not run
the Windows build or deployment script.

## Current state

The source builds on Windows through CMake and uses Qt Widgets. The current
release script is:

```text
scripts/windows-release.bat
```

It now:

- uses `C:\Qt\6.12.0\msvc2022_64` as the default Qt kit;
- allows the kit to be overridden with `QT_ROOT`;
- verifies that `%QT_ROOT%\bin\windeployqt.exe` exists;
- configures with CMake;
- builds only the `Planetary` target;
- copies `Planetary.exe` into a staging directory;
- runs `windeployqt --release`;
- copies the GeoIP database and license files;
- creates `dist\Planetary-Windows.zip`.

The version-bump step was removed because the referenced `make_version.ps1`
does not exist. CMake already generates the application version from the
repository version and Git metadata.

## Recommended Windows environment

Install:

- Visual Studio Build Tools with the MSVC C++ workload;
- Qt 6.12 MSVC 2022 x64;
- CMake;
- Git;
- Inno Setup;
- Windows SDK, for `signtool.exe`.

Run from a Visual Studio Developer Command Prompt or an environment with MSVC
available:

```bat
set QT_ROOT=C:\Qt\6.12.0\msvc2022_64
scripts\windows-release.bat
```

The first validation should be performed on a clean Windows VM without Qt or
Visual Studio installed, using the generated ZIP only.

## Immediate validation checklist

1. Confirm the batch file completes without errors.
2. Confirm `dist\Planetary-Windows.zip` contains `Planetary.exe`.
3. Launch the staged application on a clean VM.
4. Confirm the GeoIP database loads from:

   ```text
   geoip\country.mmdb
   ```

5. Confirm Qt platform plugins, image plugins, and translations are present.
6. Test HTTPS update checking and HTTPS server connections.
7. Test both a machine with the VC++ runtime already installed and a clean
   machine without it.
8. Check whether OpenSSL DLLs are present and whether TLS works without Qt or
   development tools installed.

## Runtime decisions still needed

### Visual C++ runtime

`windeployqt` is the correct tool for Qt dependencies. Decide whether the
release should:

- install the official Microsoft VC++ Redistributable as an installer
  prerequisite; or
- remain a portable ZIP with application-local runtime DLLs.

For an Inno Setup installer, prefer the official redistributable prerequisite
because it can receive servicing updates. Do not copy arbitrary runtime DLLs
from a developer machine without checking their deployment authorization.

### OpenSSL

Qt's TLS plugin is separate from the OpenSSL implementation. The deployed
application may contain a Qt TLS plugin but still need OpenSSL runtime DLLs.
Verify HTTPS on a clean machine. If required, bundle the compatible OpenSSL
DLLs in the application directory and record their provenance/license.

## Inno Setup direction

The likely production format is an Inno Setup installer built from the staged
application directory. It should eventually provide:

- application installation and clean uninstall;
- Start Menu shortcut;
- optional desktop shortcut;
- `.torrent` file association;
- `magnet:` URL association;
- per-user or administrator installation choice;
- VC++ Redistributable prerequisite handling;
- version-aware upgrades;
- license and third-party notice display;
- signed installer and uninstaller.

Keep the portable ZIP as a secondary artifact if it remains useful.

## Signing

No paid Microsoft Visual Studio subscription is required for signing. The
Windows SDK provides `signtool.exe`, but public trust requires a trusted code
signing certificate or a hosted signing service. A self-signed certificate is
appropriate only for local testing.

Sign the application binaries before embedding them in the installer, then
sign the final installer and uninstaller. Do not modify signed files afterward.

## Important scope boundary

Do not alter macOS behavior or packaging while implementing Windows support.
Windows-specific changes should remain in the Windows release script,
Windows packaging files, and appropriately guarded platform-specific code.
