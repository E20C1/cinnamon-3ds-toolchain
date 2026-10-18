@echo off
setlocal
cd /d "%~dp0"

where pwsh.exe >nul 2>&1
if %errorlevel%==0 (
  set "PS_EXE=pwsh.exe"
) else (
  set "PS_EXE=powershell.exe"
)

"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\setup-windows.ps1"
set "SETUP_EXIT=%errorlevel%"

echo.
if not "%SETUP_EXIT%"=="0" (
  echo Cinnamon 3DS environment setup failed with exit code %SETUP_EXIT%.
) else (
  echo Cinnamon 3DS environment setup completed successfully.
)
echo.
pause
exit /b %SETUP_EXIT%
