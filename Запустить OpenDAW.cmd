@echo off
if exist "%~dp0build\windows\OpenDAW_artefacts\Release\OpenDAW.exe" (
    start "" "%~dp0build\windows\OpenDAW_artefacts\Release\OpenDAW.exe"
    exit /b 0
)
if exist "%~dp0build\windows\OpenDAW_artefacts\Debug\OpenDAW.exe" (
    start "" "%~dp0build\windows\OpenDAW_artefacts\Debug\OpenDAW.exe"
    exit /b 0
)
echo Build OpenDAW first: powershell -File scripts\build.ps1 -Configuration Release
pause
exit /b 1
