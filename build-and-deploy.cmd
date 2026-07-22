@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-and-deploy.ps1" %*
if errorlevel 1 (
    echo.
    echo Build or deployment failed. See the error above.
    pause
    exit /b 1
)
echo.
echo Build and deployment completed.
pause
