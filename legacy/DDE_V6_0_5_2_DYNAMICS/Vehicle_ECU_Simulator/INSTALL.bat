@echo off
cd /d "%~dp0"
py -3 -m venv .venv
if errorlevel 1 goto fail
".venv\Scripts\python.exe" -m pip install -r requirements.txt
if errorlevel 1 goto fail
echo Installation complete. Run START.bat or DEMO.bat.
pause
exit /b 0
:fail
echo Install Python 3.10+ with Tkinter and the Windows py launcher.
pause
exit /b 1
