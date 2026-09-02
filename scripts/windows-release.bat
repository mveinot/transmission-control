@echo off
setlocal

REM --------------------------------------------------
REM Configuration
REM --------------------------------------------------

set QT_ROOT=C:\Qt\6.11.2\msvc2022_64
set BUILD_DIR=build-release
set DIST_DIR=dist

REM --------------------------------------------------
REM Bump version
REM --------------------------------------------------

powershell -ExecutionPolicy Bypass -File make_version.ps1

if errorlevel 1 (
echo Version bump failed
exit /b 1
)

REM --------------------------------------------------
REM Clean
REM --------------------------------------------------

if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
if exist %DIST_DIR% rmdir /s /q %DIST_DIR%

mkdir %BUILD_DIR%
mkdir %DIST_DIR%

REM --------------------------------------------------
REM Configure
REM --------------------------------------------------

cmake -S . -B %BUILD_DIR% ^
-DCMAKE_BUILD_TYPE=Release

if errorlevel 1 exit /b 1

REM --------------------------------------------------
REM Build
REM --------------------------------------------------

cmake --build %BUILD_DIR% --config Release

if errorlevel 1 exit /b 1

REM --------------------------------------------------
REM Stage
REM --------------------------------------------------

mkdir %DIST_DIR%\Planetary

copy %BUILD_DIR%\Release\Planetary.exe %DIST_DIR%\Planetary\

REM --------------------------------------------------
REM Deploy Qt runtime
REM --------------------------------------------------

"%QT_ROOT%\bin\windeployqt.exe" ^
--release ^
%DIST_DIR%\Planetary\Planetary.exe

if errorlevel 1 exit /b 1

REM --------------------------------------------------
REM Copy extras
REM --------------------------------------------------

if exist LICENSE copy LICENSE %DIST_DIR%\Planetary
if exist THIRD_PARTY_NOTICES.txt copy THIRD_PARTY_NOTICES.txt %DIST_DIR%\Planetary\

REM --------------------------------------------------
REM Create ZIP
REM --------------------------------------------------

powershell -Command ^
"Compress-Archive -Path '%DIST_DIR%\Planetary*' -DestinationPath '%DIST_DIR%\Planetary-Windows.zip' -Force"

echo.
echo Release package created:
echo %DIST_DIR%\Planetary-Windows.zip
echo.

endlocal
