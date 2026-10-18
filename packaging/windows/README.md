# Windows packaging handoff

## Current Windows toolchain

Planetary's Windows release uses the same toolchain as the working Qt Creator kit:

- Qt 6.12.0 LLVM-MinGW x64: `C:\Qt\6.12.0\llvm-mingw_64`;
- LLVM-MinGW 22.1.7: `C:\Qt\Tools\llvm-mingw2217_64`;
- CMake: `C:\Qt\Tools\CMake_64\bin\cmake.exe`;
- CMake generator: `MinGW Makefiles`;
- Git, available on PATH for version metadata.

MSVC and Visual Studio Build Tools are not required for this configuration.
Do not mix this build with the MSVC or GCC MinGW Qt libraries.

## Build and package

Run `scripts\windows-release.bat` from a command prompt. It locates the repository
relative to the script, so the caller's current directory does not matter.
The batch file invokes `scripts\windows-release.ps1` using Windows PowerShell.

Optional environment overrides:

```bat
set QT_ROOT=C:\Qt\6.12.0\llvm-mingw_64
set LLVM_MINGW_ROOT=C:\Qt\Tools\llvm-mingw2217_64
set CMAKE_EXE=C:\Qt\Tools\CMake_64\bin\cmake.exe
scripts\windows-release.bat
```

The script configures Release with explicit Clang compilers and MinGW Makefiles,
builds only Planetary, and stages it using the matching kit's `windeployqt --compiler-runtime`. It copies GeoIP data and all required license materials,
checks essential staged files, and creates `dist\Planetary-Windows.zip`.
Any failed tool, copy, or archive operation causes a nonzero exit status.

Build files live in `build-release\llvm-mingw`; existing build trees are preserved.
Only `dist\Planetary` is replaced during staging; other distribution artifacts are
preserved. An older ZIP may remain if a run fails before archiving: only a zero
exit status and the success message indicate a successful new release.
CMake generates the application version from repository version and Git metadata.

## Validation checklist

1. Confirm the release script exits successfully.
2. Inspect the ZIP for `Planetary\Planetary.exe`, Qt DLLs, platform and image
   plugins, translations, compiler runtime DLLs, GeoIP data, and licenses.
3. Extract and launch the ZIP on a clean Windows VM without Qt or development
   tools installed. Confirm LLVM-MinGW runtime dependencies are complete.
4. Confirm GeoIP loads from `geoip\country.mmdb` relative to the executable.
5. Test HTTPS update checking and HTTPS server connections on the clean VM.
6. Inspect available TLS backends. A Qt OpenSSL plugin alone does not establish
   that compatible OpenSSL runtime DLLs are deployed. If OpenSSL is needed,
   record bundled DLL provenance and licenses and repeat HTTPS validation.

The scripted file checks do not replace startup and TLS tests on Windows.
The VC++ Redistributable is not the compiler-runtime prerequisite for this
LLVM-MinGW build; verify actual binary dependencies before adding prerequisites.

## Inno Setup 7 installer

The installer source is `packaging\windows\Planetary.iss`.
Build the portable release first, then compile from the repository root:

```powershell
& 'C:\Program Files\Inno Setup 7\ISCC.exe' packaging\windows\Planetary.iss
```

The compiler reads `dist\Planetary` and the generated version in
`build-release\llvm-mingw\planetary-version.txt`. Output is
`dist\Planetary-<version>-Windows-x64-Setup.exe`.
The source also accepts preprocessor overrides `/DStageDir=...`,
`/DAppVersion=major.minor.patch.build`, and `/DInstallerOutputDir=...`.
When overriding the staging directory, supply the version belonging to that payload.

The installer supports Windows 10 or later on x64-compatible systems. It defaults
to a per-user install and offers an all-users installation with elevation.
It provides a Start Menu shortcut, an optional desktop shortcut, license and
third-party notice pages, and optional .torrent/magnet handler registration.
Windows controls the default-app selection; registration does not overwrite an
existing handler or UserChoice. Choose Planetary in Windows Default Apps after
installing if desired. The quoted open commands forward the file or URL as one
argument to the application, including its existing single-instance handling.

The stable AppId identifies future upgrades. Keep it unchanged. Use the same
install scope for upgrades; switching per-user/all-users can produce separate
installations. Inno tracks installed files and registry entries for uninstall.
Application settings and credentials are retained; no broad directory or settings
removal is requested. Install, upgrade, uninstall, and association behavior still
need an interactive Windows test; compilation alone does not validate them.

The package uses the validated Windows Schannel TLS backend. OpenSSL DLLs and
its Qt backend are excluded; no OpenSSL or VC++ Redistributable prerequisite is
installed for this LLVM-MinGW payload.

The default build is unsigned. Before production distribution, configure
signing for the application payload, installer, and uninstaller. A trusted
certificate or hosted signing service is required for publicly trusted signing.
Do not modify files after signing. The Windows SDK is needed if using signtool,
not for compiling with LLVM-MinGW.

## Scope boundary

Do not alter macOS behavior or packaging while implementing Windows support.
Keep Windows-specific changes in Windows scripts, packaging files, and
appropriately guarded platform-specific code.

## Qt 6.12 LLVM-MinGW deployment quirk

The installed deployment tool misclassifies this kit's plugins as debug when
release mode is forced and reports a missing platform plugin. Leave deployment
mode on automatic detection; the application is still built with CMake Release.
The package explicitly checks libc++, libunwind, and the Schannel TLS plugin.

## Local self-signed test build

The initial signing test uses a workspace PFX, not a certificate trusted by Windows.
No root trust was added. The certificate and private key are not repository files.
Only the public .cer is a deliverable. Keep the private PFX and its password local.
The public certificate can identify the signer but does not establish public trust.

`scripts\windows-sign-file.ps1` accepts a file path and reads signing configuration
from environment variables: PLANETARY_SIGNTOOL (full signtool.exe path),
PLANETARY_SIGN_PFX (private certificate path), and PLANETARY_SIGN_PASSWORD.
Do not commit or publish private keys or passwords.

After staging, sign Planetary.exe, libc++.dll, and libunwind.dll with that helper.
Preserve existing valid Qt and Microsoft signatures. Recreate the portable ZIP
from the signed staging folder. A fresh windows-release.bat run replaces staged
files, so signing must follow each release build.

Compile with `/DSigningEnabled` and configure an Inno Sign Tool named `planetary`
to invoke the signing helper with `$f` as its file argument. For example in PowerShell,
after setting the environment variables above:

```powershell
$helper = Join-Path $PWD 'scripts\windows-sign-file.ps1'
$command = 'planetary=powershell.exe -NoProfile -ExecutionPolicy Bypass -File $q' + $helper + '$q $f'
& 'C:\Program Files\Inno Setup 7\ISCC.exe' /DSigningEnabled ('/S' + $command) packaging\windows\Planetary.iss
```

This signs Setup and the embedded uninstaller. Cached signed uninstallers are in
`dist\signed-uninstaller`; clear this cache when changing signing identity so a
previous identity is not reused. This local test has no timestamp configured and
the certificate expires after one year. Trusted production signing should include
a supported timestamp service and verification under Windows Authenticode policy.

## Windows application icon

`packaging/windows/planetary.ico` contains 16 through 256 pixel versions of the
existing Planetary artwork. `planetary.rc` embeds it in the Windows executable,
so Explorer, shortcuts, and pinned taskbar entries have a native icon. The Windows
Qt application icon uses the existing PNG resource; other platforms retain their
previous icon selection. Inno uses the same ICO for Setup and Uninstall.
After installing an icon update, fully exit and relaunch Planetary. An existing
pinned shortcut may need to be unpinned and pinned again to refresh Windows' cache.
