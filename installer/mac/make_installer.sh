#!/bin/bash
# Builds dist/ODDKIN-Installer-Mac.pkg: one file, double-click to install.
#   Installs the VST3 + AU (user can untick either) and
#   /Applications/Oddkin/Uninstall ODDKIN.app
#
# Run on a Mac after building the plug-in (./build_mac.sh, or the CI build).
#
# Unsigned by default (plug-ins are ad-hoc signed). To ship without Gatekeeper
# warnings, set these (needs a paid Apple Developer account):
#   ODDKIN_SIGN_APP_ID="Developer ID Application: Your Name (TEAMID)"
#   ODDKIN_SIGN_INSTALLER_ID="Developer ID Installer: Your Name (TEAMID)"
# and for notarization either
#   ODDKIN_NOTARY_PROFILE=<keychain profile made with `xcrun notarytool store-credentials`>
# or
#   ODDKIN_NOTARY_APPLE_ID, ODDKIN_NOTARY_TEAM_ID, ODDKIN_NOTARY_PASSWORD (app-specific password)
set -euo pipefail
cd "$(dirname "$0")/../.."

VERSION="${1:-$(sed -n 's/^project(ODDKIN VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)}"
ART=build/ODDKIN_artefacts/Release
VST3="$ART/VST3/ODDKIN.vst3"
AU="$ART/AU/ODDKIN.component"
HERE=installer/mac
WORK=build/installer-mac
OUT="dist/ODDKIN-Installer-Mac.pkg"

[ -d "$VST3" ] || { echo "No VST3 found at $VST3 — build the plug-in first (./build_mac.sh)."; exit 1; }
HAS_AU=0; [ -d "$AU" ] && HAS_AU=1

APP_ID="${ODDKIN_SIGN_APP_ID:-}"
INST_ID="${ODDKIN_SIGN_INSTALLER_ID:-}"

sign_code() {
  if [ -n "$APP_ID" ]; then
    codesign --force --deep --options runtime --timestamp -s "$APP_ID" "$1"
  else
    codesign --force --deep -s - "$1"
  fi
}

echo "== ODDKIN $VERSION mac installer =="
rm -rf "$WORK"
mkdir -p "$WORK/root_vst3/Library/Audio/Plug-Ins/VST3" "$WORK/root_au/Library/Audio/Plug-Ins/Components" "$WORK/root_uninst/Applications/Oddkin" dist

cp -R "$VST3" "$WORK/root_vst3/Library/Audio/Plug-Ins/VST3/"
sign_code "$WORK/root_vst3/Library/Audio/Plug-Ins/VST3/ODDKIN.vst3"
if [ $HAS_AU = 1 ]; then
  cp -R "$AU" "$WORK/root_au/Library/Audio/Plug-Ins/Components/"
  sign_code "$WORK/root_au/Library/Audio/Plug-Ins/Components/ODDKIN.component"
fi

osacompile -o "$WORK/root_uninst/Applications/Oddkin/Uninstall ODDKIN.app" "$HERE/Uninstall ODDKIN.applescript"
sign_code "$WORK/root_uninst/Applications/Oddkin/Uninstall ODDKIN.app"

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
           --scripts "$HERE/scripts" "$WORK/oddkin_$kind.pkg"
}

build_component vst3 com.oddkin.oddkin.vst3
[ $HAS_AU = 1 ] && build_component au com.oddkin.oddkin.au
build_component uninst com.oddkin.oddkin.uninstaller

AU_LINE=""; AU_CHOICE=""; AU_REF=""
if [ $HAS_AU = 1 ]; then
  AU_LINE='<line choice="au"/>'
  AU_CHOICE='<choice id="au" title="ODDKIN Audio Unit" description="For Logic Pro, GarageBand and other AU hosts. Installs to /Library/Audio/Plug-Ins/Components."><pkg-ref id="com.oddkin.oddkin.au"/></choice>'
  AU_REF="<pkg-ref id=\"com.oddkin.oddkin.au\" version=\"$VERSION\" onConclusion=\"none\">oddkin_au.pkg</pkg-ref>"
fi

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>ODDKIN $VERSION</title>
  <organization>com.oddkin</organization>
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
  <choice id="uninst" visible="false" title="Uninstaller"><pkg-ref id="com.oddkin.oddkin.uninstaller"/></choice>
  <pkg-ref id="com.oddkin.oddkin.uninstaller" version="$VERSION" onConclusion="none">oddkin_uninst.pkg</pkg-ref>
  <choice id="vst3" title="ODDKIN VST3" description="For Ableton Live and most other hosts. Installs to /Library/Audio/Plug-Ins/VST3."><pkg-ref id="com.oddkin.oddkin.vst3"/></choice>
  $AU_CHOICE
  <pkg-ref id="com.oddkin.oddkin.vst3" version="$VERSION" onConclusion="none">oddkin_vst3.pkg</pkg-ref>
  $AU_REF
</installer-gui-script>
EOF

rm -f "$OUT"
if [ -n "$INST_ID" ]; then
  productbuild --distribution "$WORK/distribution.xml" --resources "$HERE/resources" --package-path "$WORK" --sign "$INST_ID" "$OUT"
else
  productbuild --distribution "$WORK/distribution.xml" --resources "$HERE/resources" --package-path "$WORK" "$OUT"
fi

if [ -n "${ODDKIN_NOTARY_PROFILE:-}" ]; then
  xcrun notarytool submit "$OUT" --keychain-profile "$ODDKIN_NOTARY_PROFILE" --wait
  xcrun stapler staple "$OUT"
elif [ -n "${ODDKIN_NOTARY_APPLE_ID:-}" ]; then
  xcrun notarytool submit "$OUT" --apple-id "$ODDKIN_NOTARY_APPLE_ID" --team-id "$ODDKIN_NOTARY_TEAM_ID" --password "$ODDKIN_NOTARY_PASSWORD" --wait
  xcrun stapler staple "$OUT"
fi

echo "Done: $OUT"
