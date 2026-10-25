@echo off
setlocal
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0windows-release.ps1" %*
exit /b %errorlevel%
