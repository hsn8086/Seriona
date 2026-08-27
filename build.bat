@echo off
setlocal

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-package-windows.ps1" %*
set "SERIONA_BUILD_EXIT=%ERRORLEVEL%"

if not "%SERIONA_BUILD_EXIT%"=="0" (
    echo.
    echo Seriona Windows x64 build failed with exit code %SERIONA_BUILD_EXIT%.
    echo Review the error above and dist\logs for complete command output.
    if not defined SERIONA_NO_PAUSE pause
)

exit /b %SERIONA_BUILD_EXIT%
