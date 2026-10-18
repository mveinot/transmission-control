$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Match the Qt Creator LLVM-MinGW kit. All paths may be overridden for CI.
$repoRoot = Split-Path -Parent $PSScriptRoot
$qtRoot = if ($env:QT_ROOT) { $env:QT_ROOT } else { 'C:\Qt\6.12.0\llvm-mingw_64' }
$toolchainRoot = if ($env:LLVM_MINGW_ROOT) { $env:LLVM_MINGW_ROOT } else { 'C:\Qt\Tools\llvm-mingw2217_64' }
$cmake = if ($env:CMAKE_EXE) { $env:CMAKE_EXE } else { 'C:\Qt\Tools\CMake_64\bin\cmake.exe' }
$compilerBin = Join-Path $toolchainRoot 'bin'
$deployQt = Join-Path $qtRoot 'bin\windeployqt.exe'
$clang = Join-Path $compilerBin 'clang.exe'
$clangxx = Join-Path $compilerBin 'clang++.exe'
$make = Join-Path $compilerBin 'mingw32-make.exe'
$buildDir = Join-Path $repoRoot 'build-release\llvm-mingw'
$distDir = Join-Path $repoRoot 'dist'
$stageDir = Join-Path $distDir 'Planetary'
$zipPath = Join-Path $distDir 'Planetary-Windows.zip'

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

try {
    foreach ($tool in @($cmake, $deployQt, $clang, $clangxx, $make)) {
        if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Required tool not found: $tool" }
    }
    $env:PATH = "$compilerBin;$(Join-Path $qtRoot 'bin');$env:PATH"
    Invoke-Checked $cmake @('-S', $repoRoot, '-B', $buildDir, '-G', 'MinGW Makefiles',
        '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_PREFIX_PATH=$qtRoot",
        "-DCMAKE_C_COMPILER=$clang", "-DCMAKE_CXX_COMPILER=$clangxx", "-DCMAKE_MAKE_PROGRAM=$make")
    Invoke-Checked $cmake @('--build', $buildDir, '--config', 'Release', '--target', 'Planetary', '--parallel')

    $exe = Join-Path $buildDir 'Planetary.exe'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Built executable not found: $exe" }
    # Only replace our fixed staging folder; preserve other dist artifacts and all builds.
    if (Test-Path -LiteralPath $stageDir) {
        $resolvedStage = (Resolve-Path -LiteralPath $stageDir).Path
        $expectedStage = [IO.Path]::GetFullPath((Join-Path $repoRoot 'dist\Planetary'))
        if ($resolvedStage -ne $expectedStage -or
            ((Get-Item -LiteralPath $stageDir).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to remove unexpected staging path: $resolvedStage"
        }
        Remove-Item -LiteralPath $resolvedStage -Recurse -Force
    }
    New-Item -ItemType Directory -Path $stageDir -Force | Out-Null
    Copy-Item -LiteralPath $exe -Destination $stageDir
    # This Qt kit's plugins are misclassified as debug when --release is forced.
    # Auto-detection deploys the matching unsuffixed Qt DLLs and plugins correctly.
    Invoke-Checked $deployQt @('--compiler-runtime', (Join-Path $stageDir 'Planetary.exe'))

    $geoipDir = New-Item -ItemType Directory -Path (Join-Path $stageDir 'geoip') -Force
    $licenseDir = New-Item -ItemType Directory -Path (Join-Path $stageDir 'licenses') -Force
    Copy-Item -LiteralPath (Join-Path $repoRoot 'Resources\geoip\country.mmdb') -Destination $geoipDir.FullName
    foreach ($file in @('LICENSE', 'Resources\licenses\THIRD_PARTY_NOTICES.txt',
            'Resources\licenses\qt-open-source-license.txt', 'Resources\licenses\flag-icons.txt')) {
        Copy-Item -LiteralPath (Join-Path $repoRoot $file) -Destination $licenseDir.FullName
    }
    foreach ($file in @('libmaxminddb-LICENSE.txt', 'libmaxminddb-NOTICE.txt',
            'libmaxminddb-compat-util.h', 'miniz-LICENSE.txt')) {
        Copy-Item -LiteralPath (Join-Path $buildDir "generated\licenses\$file") -Destination $licenseDir.FullName
    }
    foreach ($file in @('Planetary.exe', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll',
            'Qt6Network.dll', 'Qt6Svg.dll', 'libc++.dll', 'libunwind.dll',
            'platforms\qwindows.dll', 'tls\qschannelbackend.dll', 'geoip\country.mmdb')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stageDir $file) -PathType Leaf)) {
            throw "Required staged file missing: $file"
        }
    }
    Compress-Archive -LiteralPath $stageDir -DestinationPath $zipPath -Force
    Write-Host "Release package created: $zipPath"
    exit 0
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
