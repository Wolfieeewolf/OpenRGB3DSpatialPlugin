@echo off
REM Quiet Windows clean for Qt Creator / jom.
REM Avoids the default qmake "del file1 file2 ... fileN" recipe that floods
REM Compile Output and triggers: Discarding excessive amount of pending output.
setlocal
set "OUT=%~1"
if "%OUT%"=="" set "OUT=%CD%"

if exist "%OUT%\_intermediate_release" rmdir /s /q "%OUT%\_intermediate_release"
if exist "%OUT%\_intermediate_debug" rmdir /s /q "%OUT%\_intermediate_debug"
if exist "%OUT%\release\*.obj" del /f /q "%OUT%\release\*.obj" 2>nul
if exist "%OUT%\release\*.exp" del /f /q "%OUT%\release\*.exp" 2>nul
if exist "%OUT%\release\*.lib" del /f /q "%OUT%\release\*.lib" 2>nul
if exist "%OUT%\release\*.pdb" del /f /q "%OUT%\release\*.pdb" 2>nul
if exist "%OUT%\debug\*.obj" del /f /q "%OUT%\debug\*.obj" 2>nul
if exist "%OUT%\debug\*.exp" del /f /q "%OUT%\debug\*.exp" 2>nul
if exist "%OUT%\debug\*.lib" del /f /q "%OUT%\debug\*.lib" 2>nul
if exist "%OUT%\debug\*.pdb" del /f /q "%OUT%\debug\*.pdb" 2>nul
echo Clean done.
exit /b 0
