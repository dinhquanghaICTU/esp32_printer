@echo off
powershell.exe -ExecutionPolicy Bypass -NoProfile -File "%~dp0esp32_run.ps1" %*
pause
