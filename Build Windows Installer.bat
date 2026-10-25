@echo off
setlocal EnableDelayedExpansion
REM ===========================================================================
REM  Double-click me to build ODDKIN-Installer-Windows.exe
REM  Installs anything missing (Git, CMake, Visual Studio C++ Build Tools,
REM  Inno Setup) using winget, which comes with Windows 10/11.
REM  First run: 20-40 minutes (mostly downloading the C++ tools).
REM ===========================================================================
cd /d "%~dp0"
title Building ODDKIN

where winget >nul 2>nul || (
  echo winget is missing. Update "App Installer" from the Microsoft Store, then run this again.
  pause & exit /b 1
)

echo.
echo == Checking tools ==
where git >nul 2>nul || (
  echo Installing Git...
  winget install --id Git.Git -e --silent --accept-package-agreements --accept-source-agreements
  set "PATH=%PATH%;%ProgramFiles%\Git\cmd"
)
where cmake >nul 2>nul || (
  echo Installing CMake...
  winget install --id Kitware.CMake -e --silent --accept-package-agreements --accept-source-agreements
  set "PATH=%PATH%;%ProgramFiles%\CMake\bin"
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "HAVE_VS="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "HAVE_VS=%%i"
)
if not defined HAVE_VS (
  echo Installing Visual Studio C++ Build Tools - this is the big one, please wait...
  winget install --id Microsoft.VisualStudio.2022.BuildTools -e --silent --accept-package-agreements --accept-source-agreements --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
)

set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%LocalAppData%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
  echo Installing Inno Setup...
  winget install --id JRSoftware.InnoSetup -e --silent --accept-package-agreements --accept-source-agreements
  set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
  if not exist "!ISCC!" set "ISCC=%LocalAppData%\Programs\Inno Setup 6\ISCC.exe"
)

where git >nul 2>nul || set "PATH=%PATH%;%ProgramFiles%\Git\cmd"
where cmake >nul 2>nul || set "PATH=%PATH%;%ProgramFiles%\CMake\bin"
where cmake >nul 2>nul || (echo CMake still not found - close this window and run it again. & pause & exit /b 1)

echo.
echo == Getting JUCE ==
if not exist JUCE (
  git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE || (pause & exit /b 1)
) else (
  echo Already here
)

echo.
echo == Building ODDKIN (VST3) ==
cmake -B build -DCMAKE_BUILD_TYPE=Release || (pause & exit /b 1)
cmake --build build --config Release --target ODDKIN_VST3 --parallel || (pause & exit /b 1)

echo.
echo == Making the installer ==
set VERSION=1.0.0
for /f "tokens=3" %%v in ('findstr /b /c:"project(ODDKIN VERSION" CMakeLists.txt') do set VERSION=%%v
if not exist "%ISCC%" (echo Inno Setup not found - close this window and run it again. & pause & exit /b 1)
"%ISCC%" /Q /DAppVersion=%VERSION% installer\windows\ODDKIN.iss || (pause & exit /b 1)

echo.
echo == Done! ==
echo dist\ODDKIN-Installer-Windows.exe is what you give people.
start "" "%~dp0dist"
pause
