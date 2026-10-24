#!/bin/bash
# Double-click me (or run: bash "Build Mac Installer.command")
# Builds JACKPOT and makes dist/JACKPOT-<version>-mac.dmg (installer + uninstaller).
# First run takes ~15-20 minutes. Nothing is installed system-wide except
# Apple's free Command Line Tools if you don't already have them.
cd "$(dirname "$0")" || exit 1
xattr -dr com.apple.quarantine . 2>/dev/null || true   # files from a downloaded zip
set -e

say_step() { printf "\n\033[1;35m== %s ==\033[0m\n" "$1"; }

# 1. Apple's compiler
say_step "Checking Apple Command Line Tools"
if ! xcode-select -p >/dev/null 2>&1 || ! command -v clang++ >/dev/null 2>&1; then
  echo "Installing Apple's Command Line Tools — click 'Install' in the window that pops up."
  xcode-select --install || true
  echo ""
  echo "When that install finishes, double-click this file again."
  read -r -p "Press Return to close." _
  exit 0
fi
echo "OK"

# 2. CMake (uses yours if you have it, otherwise downloads a private copy into this folder)
say_step "Checking CMake"
CMAKE=""
if command -v cmake >/dev/null 2>&1; then
  CMAKE="cmake"
elif [ -x "tools-cache/cmake/CMake.app/Contents/bin/cmake" ]; then
  CMAKE="$PWD/tools-cache/cmake/CMake.app/Contents/bin/cmake"
else
  echo "Downloading CMake (one time, ~50 MB)..."
  mkdir -p tools-cache
  curl -fL --progress-bar -o tools-cache/cmake.tar.gz \
    https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-macos-universal.tar.gz
  tar -xzf tools-cache/cmake.tar.gz -C tools-cache
  mv tools-cache/cmake-3.30.5-macos-universal tools-cache/cmake
  rm tools-cache/cmake.tar.gz
  xattr -dr com.apple.quarantine tools-cache/cmake 2>/dev/null || true
  CMAKE="$PWD/tools-cache/cmake/CMake.app/Contents/bin/cmake"
fi
"$CMAKE" --version | head -1

# 3. JUCE
say_step "Getting JUCE"
if [ ! -d JUCE ]; then
  git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
else
  echo "Already here"
fi

# 4. Build (universal: runs on Intel + Apple Silicon Macs)
say_step "Building JACKPOT (this is the long part)"
"$CMAKE" -B build -DCMAKE_BUILD_TYPE=Release
"$CMAKE" --build build --config Release --target JACKPOT_VST3 JACKPOT_AU --parallel "$(sysctl -n hw.ncpu)"

# 5. Installer
say_step "Making the installer"
chmod +x installer/mac/make_installer.sh installer/mac/scripts/postinstall
./installer/mac/make_installer.sh

say_step "Done!"
ls -1 dist/*.dmg
open dist
echo ""
echo "The .dmg in the window that just opened is what you give people."
echo "It contains 'Install JACKPOT.pkg' and 'Uninstall JACKPOT'."
read -r -p "Press Return to close." _
