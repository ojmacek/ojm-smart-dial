@echo off
setlocal
cd /d "%~dp0"

echo Installing or updating PyInstaller...
py -m pip install --upgrade pyinstaller
if errorlevel 1 goto :failed

echo.
echo Building OJM Vehicle ECU Simulator...
py -m PyInstaller ^
  --noconfirm ^
  --clean ^
  --onefile ^
  --windowed ^
  --name "OJM_Vehicle_ECU_Simulator" ^
  --add-data "OJM_Logo.png;." ^
  --add-data "OJM_Splash.png;." ^
  --hidden-import can.interfaces.slcan ^
  --hidden-import serial ^
  OJM_Vehicle_ECU_Simulator.py
if errorlevel 1 goto :failed

echo.
echo Build complete:
echo %CD%\dist\OJM_Vehicle_ECU_Simulator.exe
pause
exit /b 0

:failed
echo.
echo Build failed. The error is shown above.
pause
exit /b 1
