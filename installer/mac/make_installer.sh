#!/bin/bash
# Builds dist/JACKPOT-Installer-Mac.pkg: one file, double-click to install.
#   Installs the VST3 + AU (user can untick either) and
#   /Applications/Jackpot Audio/Uninstall JACKPOT.app
#
# Run on a Mac after building the plug-in (./build_mac.sh, or the CI build).
#
# Unsigned by default (plug-ins are ad-hoc signed). To ship without Gatekeeper
# warnings, set these (needs a paid Apple Developer account):
#   JACKPOT_SIGN_APP_ID="Developer ID Application: Your Name (TEAMID)"
#   JACKPOT_SIGN_INSTALLER_ID="Developer ID Installer: Your Name (TEAMID)"
# and for notarization either
#   JACKPOT_NOTARY_PROFILE=<keychain profile made with `xcrun notarytool store-credentials`>
# or
#   JACKPOT_NOTARY_APPLE_ID, JACKPOT_NOTARY_TEAM_ID, JACKPOT_NOTARY_PASSWORD (app-specific password)
set -euo pipefail
cd "$(dirname "$0")/../.."

VERSION="${1:-$(sed -n 's/^project(JACKPOT VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)}"
ART=build/JACKPOT_artefacts/Release
VST3="$ART/VST3/JACKPOT.vst3"
AU="$ART/AU/JACKPOT.component"
HERE=installer/mac
WORK=build/installer-mac
OUT="dist/JACKPOT-Installer-Mac.pkg"

[ -d "$VST3" ] || { echo "No VST3 found at $VST3 — build the plug-in first (./build_mac.sh)."; exit 1; }
HAS_AU=0; [ -d "$AU" ] && HAS_AU=1

APP_ID="${JACKPOT_SIGN_APP_ID:-}"
INST_ID="${JACKPOT_SIGN_INSTALLER_ID:-}"

sign_code() {
  if [ -n "$APP_ID" ]; then
    codesign --force --deep --options runtime --timestamp -s "$APP_ID" "$1"
  else
    codesign --force --deep -s - "$1"
  fi
}

echo "== JACKPOT $VERSION mac installer =="
rm -rf "$WORK"
mkdir -p "$WORK/root_vst3/Library/Audio/Plug-Ins/VST3" "$WORK/root_au/Library/Audio/Plug-Ins/Components" "$WORK/root_uninst/Applications/Jackpot Audio" dist

cp -R "$VST3" "$WORK/root_vst3/Library/Audio/Plug-Ins/VST3/"
sign_code "$WORK/root_vst3/Library/Audio/Plug-Ins/VST3/JACKPOT.vst3"
if [ $HAS_AU = 1 ]; then
  cp -R "$AU" "$WORK/root_au/Library/Audio/Plug-Ins/Components/"
  sign_code "$WORK/root_au/Library/Audio/Plug-Ins/Components/JACKPOT.component"
fi

osacompile -o "$WORK/root_uninst/Applications/Jackpot Audio/Uninstall JACKPOT.app" "$HERE/Uninstall JACKPOT.applescript"
sign_code "$WORK/root_uninst/Applications/Jackpot Audio/Uninstall JACKPOT.app"

chmod +x "$HERE/scripts/postinstall"

build_component() { # kind identifier
  local kind=$1 ident=$2
  # bundles must not be "relocatable", or the installer may update an old copy somewhere else instead
  pkgbuild --analyze --root "$WORK/root_$kind" "$WORK/$kind.plist" >/dev/null
  local i=0
  while plutil -extract "$i" xml1 -o /dev/null "$WORK/$kind.plist" >/dev/null 2>&1; do
    plutil -replace "$i.BundleIsRelocatable" -bool NO "$WORK/$kind.plist"
    i=$((i + 1))
  done
  pkgbuild --root "$WORK/root_$kind" --component-plist "$WORK/$kind.plist" \
           --identifier "$ident" --version "$VERSION" --install-location / \
           --scripts "$HERE/scripts" "$WORK/jackpot_$kind.pkg"
}

build_component vst3 com.jackpotaudio.jackpot.vst3
[ $HAS_AU = 1 ] && build_component au com.jackpotaudio.jackpot.au
build_component uninst com.jackpotaudio.jackpot.uninstaller

AU_LINE=""; AU_CHOICE=""; AU_REF=""
if [ $HAS_AU = 1 ]; then
  AU_LINE='<line choice="au"/>'
  AU_CHOICE='<choice id="au" title="JACKPOT Audio Unit" description="For Logic Pro, GarageBand and other AU hosts. Installs to /Library/Audio/Plug-Ins/Components."><pkg-ref id="com.jackpotaudio.jackpot.au"/></choice>'
  AU_REF="<pkg-ref id=\"com.jackpotaudio.jackpot.au\" version=\"$VERSION\" onConclusion=\"none\">jackpot_au.pkg</pkg-ref>"
fi

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>JACKPOT $VERSION</title>
  <organization>com.jackpotaudio</organization>
  <welcome file="welcome.html" mime-type="text/html"/>
  <conclusion file="conclusion.html" mime-type="text/html"/>
  <background file="background.png" alignment="bottomleft" scaling="none"/>
  <background-darkAqua file="background_dark.png" alignment="bottomleft" scaling="none"/>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true" enable_currentUserHome="false" enable_anywhere="false"/>
  <volume-check><allowed-os-versions><os-version min="10.15"/></allowed-os-versions></volume-check>
  <choices-outline>
    <line choice="vst3"/>
    $AU_LINE
    <line choice="uninst"/>
  </choices-outline>
  <choice id="uninst" visible="false" title="Uninstaller"><pkg-ref id="com.jackpotaudio.jackpot.uninstaller"/></choice>
  <pkg-ref id="com.jackpotaudio.jackpot.uninstaller" version="$VERSION" onConclusion="none">jackpot_uninst.pkg</pkg-ref>
  <choice id="vst3" title="JACKPOT VST3" description="For Ableton Live and most other hosts. Installs to /Library/Audio/Plug-Ins/VST3."><pkg-ref id="com.jackpotaudio.jackpot.vst3"/></choice>
  $AU_CHOICE
  <pkg-ref id="com.jackpotaudio.jackpot.vst3" version="$VERSION" onConclusion="none">jackpot_vst3.pkg</pkg-ref>
  $AU_REF
</installer-gui-script>
EOF

rm -f "$OUT"
if [ -n "$INST_ID" ]; then
  productbuild --distribution "$WORK/distribution.xml" --resources "$HERE/resources" --package-path "$WORK" --sign "$INST_ID" "$OUT"
else
  productbuild --distribution "$WORK/distribution.xml" --resources "$HERE/resources" --package-path "$WORK" "$OUT"
fi

if [ -n "${JACKPOT_NOTARY_PROFILE:-}" ]; then
  xcrun notarytool submit "$OUT" --keychain-profile "$JACKPOT_NOTARY_PROFILE" --wait
  xcrun stapler staple "$OUT"
elif [ -n "${JACKPOT_NOTARY_APPLE_ID:-}" ]; then
  xcrun notarytool submit "$OUT" --apple-id "$JACKPOT_NOTARY_APPLE_ID" --team-id "$JACKPOT_NOTARY_TEAM_ID" --password "$JACKPOT_NOTARY_PASSWORD" --wait
  xcrun stapler staple "$OUT"
fi

echo "Done: $OUT"
