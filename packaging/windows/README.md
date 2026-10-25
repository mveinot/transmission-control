# Windows packaging handoff

For copyable local and SSH build commands and a Codex CLI handoff, see
[BUILD-HANDOFF.md](BUILD-HANDOFF.md).

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
checks essential staged files, and creates the Inno Setup installer when Inno
Setup 7 is installed. If `PLANETARY_SIGN_THUMBPRINT` identifies a certificate
with a private key in `CurrentUser\My`, the application and LLVM-MinGW runtime
DLLs are signed before installer creation; the installer and embedded
uninstaller are signed as well. Any failed tool or copy operation causes a
nonzero exit status.

Build files live in `build-release\llvm-mingw`; existing build trees are preserved.
Only `dist\Planetary` is replaced during staging; other distribution artifacts
are preserved. Only a zero exit status and the success message indicate a
successful new release.
CMake generates the application version from repository version and Git metadata.

## Validation checklist

1. Confirm the release script exits successfully.
2. Inspect `dist\Planetary` for `Planetary.exe`, Qt DLLs, platform and image
   plugins, translations, compiler runtime DLLs, GeoIP data, and licenses.
3. Install and launch the installer on a clean Windows VM without Qt or
   development tools. Confirm LLVM-MinGW runtime dependencies are complete.
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
After the release script stages the application, compile the installer
separately from the repository root with:

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

## Signing and remote Windows builds

Run a complete signed build, from any working directory:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\Users\Mark\Documents\transmission-control\scripts\windows-release.ps1
```

From macOS/Linux over SSH (replace WINDOWS_HOST with this computer's address):

```sh
ssh -T Mark@WINDOWS_HOST 'powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\Users\Mark\Documents\transmission-control\scripts\windows-release.ps1'
```

Use `-CheckOnly` for a fast configuration check, `-Jobs 4` to limit compilation
parallelism, or `-Unsigned` only when intentionally building an unsigned package.
The batch entry point forwards the same options. Exit code 0 means success;
errors from build, signing, deployment, or Inno propagate as nonzero exits.
The process runs no installer and launches no GUI. Concurrent release builds in
the same checkout are rejected by an exclusive lock in dist.

Signing is required by default and validated before compilation. The default
machine-local config is `scripts/windows-release.local.json` (ignored by Git).
Copy `scripts/windows-release.example.json` and edit the paths on another machine.
An alternate JSON path can be supplied with `-SigningConfig`. Supported config
fields are PLANETARY_SIGN_PFX, PLANETARY_SIGN_PASSWORD_FILE,
PLANETARY_SIGN_THUMBPRINT, PLANETARY_SIGNTOOL, and PLANETARY_TIMESTAMP_URL.
The password file contains the raw PFX password on one line. Paths should be
absolute. Environment variables can supply the same settings; local JSON values
override them. Build tool overrides remain environment variables: QT_ROOT,
LLVM_MINGW_ROOT, CMAKE_EXE, and INNO_SETUP_COMPILER.

This computer uses a new self-signed test PFX and password file under
`C:\Users\Mark\Documents\Codex\windows-signing`, outside the repository. The
folder is restricted to the current Windows user, Administrators, and SYSTEM.
Back up both files securely. No certificate trust settings were changed. The
SSH user must be this Windows user or separately have access to signing material.
The old test PFX was not overwritten; its password was unavailable.
PFX signing works without importing a key into the user's certificate store.
Alternatively, use PLANETARY_SIGN_THUMBPRINT to select a certificate with its
private key in the SSH user's CurrentUser\My store. The key provider must support
unattended signing; hardware tokens may require interactive authentication.

The pipeline signs Planetary.exe and the LLVM runtime DLLs, preserving Qt and
Microsoft vendor signatures. It then signs Setup and the embedded uninstaller,
using a fresh uninstaller cache directory each build. The Inno Setup installer
is the sole Windows distribution package. Outputs in dist:

- Planetary-<version>-Windows-x64-Setup.exe
- Planetary-Windows-hashes.json (SHA-256 for the installer)

The test certificate is self-signed and untrusted by default; the build accepts
that trust result while rejecting missing or mismatched signatures. Production
signing needs a trusted certificate and a timestamp URL. With no timestamp URL,
no timestamp is requested. Never commit PFX files, passwords, or the local config.

The existing `scripts/build-windows-remote.sh` launcher additionally fetches and
builds the exact pushed local commit and copies the installer back to the Mac.
It requires clean local/remote checkouts and SSH key authentication. Set
PLANETARY_WINDOWS_SSH_TARGET; the Windows local signing config is used unless a
certificate-store thumbprint is supplied. The direct SSH command above builds
the current Windows checkout without fetching or changing branches.

## Windows application icon

`packaging/windows/planetary.ico` contains 16 through 256 pixel versions of the
existing Planetary artwork. `planetary.rc` embeds it in the Windows executable,
so Explorer, shortcuts, and pinned taskbar entries have a native icon. The Windows
Qt application icon uses the existing PNG resource; other platforms retain their
previous icon selection. Inno uses the same ICO for Setup and Uninstall.
After installing an icon update, fully exit and relaunch Planetary. An existing
pinned shortcut may need to be unpinned and pinned again to refresh Windows' cache.
