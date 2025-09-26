@echo off
REM Change directory to the project root (one level up from scripts/)
cd /d "%~dp0\.."

REM Path to Premake executable
set PREMAKE="%CD%\vendor\bin\premake\premake5.exe"

REM Path to Premake script (optional if it's named premake5.lua in root)
set SCRIPT="%CD%\premake5.lua"

REM Run Premake to generate Visual Studio 2022 solution
%PREMAKE% vs2022 --file=%SCRIPT%

pause
