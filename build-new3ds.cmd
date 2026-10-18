@echo off
setlocal
cd /d "%~dp0"

where pwsh.exe >nul 2>&1
if %errorlevel%==0 (
  set "PS_EXE=pwsh.exe"
) else (
  set "PS_EXE=powershell.exe"
)

"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-windows-n3ds.ps1" -NoPause
set "BUILD_EXIT=%errorlevel%"

echo.
if not "%BUILD_EXIT%"=="0" (
  echo Cinnamon New 3DS build failed with exit code %BUILD_EXIT%.
) else (
  echo Cinnamon New 3DS build completed successfully.
)
echo.
pause
exit /b %BUILD_EXIT%
