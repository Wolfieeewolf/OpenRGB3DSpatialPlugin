@echo off
REM Invoked from Makefile after qmake_all — keeps the make recipe to one short line.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0patch-makefile-clean.ps1" "%~1" "%~dp0qt-win-clean.cmd"
exit /b %ERRORLEVEL%
