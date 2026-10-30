@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" call INSTALL.bat
if errorlevel 1 exit /b 1
".venv\Scripts\python.exe" main.py --port COM8
if errorlevel 1 pause
