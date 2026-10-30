@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" call INSTALL.bat
if errorlevel 1 exit /b 1
".venv\Scripts\python.exe" -m pip install "pyinstaller==6.22.3"
if errorlevel 1 exit /b 1
".venv\Scripts\python.exe" -m PyInstaller --noconfirm --clean OJM_DDE_Virtual_ECU.spec
if errorlevel 1 exit /b 1
echo Done: dist\OJM_DDE_Virtual_ECU.exe
pause
