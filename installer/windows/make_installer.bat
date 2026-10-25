@echo off
REM Builds dist\ODDKIN-Installer-Windows.exe
REM Run after build_windows.bat has built the plug-in.
REM Needs Inno Setup 6: https://jrsoftware.org/isdl.php
cd /d "%~dp0\..\.."

if not exist "build\ODDKIN_artefacts\Release\VST3\ODDKIN.vst3" (
  echo Build the plug-in first: build_windows.bat
  pause & exit /b 1
)

set ISCC=
where iscc >nul 2>nul && set ISCC=iscc
if "%ISCC%"=="" if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set ISCC="%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if "%ISCC%"=="" if exist "%LocalAppData%\Programs\Inno Setup 6\ISCC.exe" set ISCC="%LocalAppData%\Programs\Inno Setup 6\ISCC.exe"
if "%ISCC%"=="" (
  echo Inno Setup 6 not found. Install it from https://jrsoftware.org/isdl.php
  pause & exit /b 1
)

set VERSION=1.0.0
for /f "tokens=3" %%v in ('findstr /b /c:"project(ODDKIN VERSION" CMakeLists.txt') do set VERSION=%%v

%ISCC% /DAppVersion=%VERSION% installer\windows\ODDKIN.iss || (pause & exit /b 1)
echo.
echo Done: dist\ODDKIN-Installer-Windows.exe
pause
