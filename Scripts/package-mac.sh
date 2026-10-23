#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ "$(uname -s)" == Darwin ]] || { echo 'Packaging requires macOS.' >&2; exit 1; }
version=0.3.0
out="$PWD/dist"
work="$(mktemp -d "${TMPDIR:-/tmp}/irchoir-package.XXXXXX")"
trap 'rm -rf "$work"' EXIT
mkdir -p "$out" "$work/root/Library/Audio/Plug-Ins/Components" "$work/root/Library/Audio/Plug-Ins/VST3" "$work/image"
au='build/IRChoir_artefacts/Release/AU/IRChoir.component'
vst='build/IRChoir_artefacts/Release/VST3/IRChoir.vst3'
[[ -d "$au" && -d "$vst" ]] || { echo 'Both compiled plugin bundles are required; nothing packaged.' >&2; exit 1; }
for binary in "$au/Contents/MacOS/IRChoir" "$vst/Contents/MacOS/IRChoir"; do
  xcrun lipo -verify_arch arm64 x86_64 "$binary"
done
ditto "$au" "$work/root/Library/Audio/Plug-Ins/Components/IRChoir.component"
ditto "$vst" "$work/root/Library/Audio/Plug-Ins/VST3/IRChoir.vst3"
label=unsigned
app_identity="${IRCHOIR_APP_IDENTITY:-}"
pkg_identity="${IRCHOIR_INSTALLER_IDENTITY:-}"
if [[ -n "$app_identity" && -z "$pkg_identity" ]] || [[ -z "$app_identity" && -n "$pkg_identity" ]]; then
  echo 'Set both signing identities, or neither.' >&2; exit 1
fi
if [[ -n "$app_identity" ]]; then
  label=signed
  for bundle in "$work/root/Library/Audio/Plug-Ins/Components/IRChoir.component" "$work/root/Library/Audio/Plug-Ins/VST3/IRChoir.vst3"; do
    codesign --force --timestamp --options runtime --sign "$app_identity" "$bundle"
    codesign --verify --strict "$bundle"
  done
else
  for bundle in "$work/root/Library/Audio/Plug-Ins/Components/IRChoir.component" "$work/root/Library/Audio/Plug-Ins/VST3/IRChoir.vst3"; do
    codesign --force --sign - "$bundle"
    codesign --verify --strict "$bundle"
  done
fi
pkg="$work/image/I.R.Choir-$version-$label.pkg"
pkgbuild --root "$work/root" --identifier audio.ir.choir.installer --version "$version" --install-location / "$work/payload.pkg"
pkgutil --payload-files "$work/payload.pkg" > "$out/package-payload.txt"
grep -F 'IRChoir.component/Contents/MacOS/IRChoir' "$out/package-payload.txt" >/dev/null
grep -F 'IRChoir.vst3/Contents/MacOS/IRChoir' "$out/package-payload.txt" >/dev/null
if [[ "$label" == signed ]]; then
  productbuild --distribution Scripts/Distribution.xml --resources InstallerResources --package-path "$work" --sign "$pkg_identity" "$pkg"
else
  productbuild --distribution Scripts/Distribution.xml --resources InstallerResources --package-path "$work" "$pkg"
fi
mkdir -p "$work/image/Documentation"
cp Documentation/*.pdf "$work/image/Documentation/"
cp Documentation/VOCAL-IMPORT.txt "$work/image/Documentation/"
cp INSTALL_MAC.md "$work/image/READ-ME-FIRST.txt"
cp AUDIT.md "$work/image/BUILD-AUDIT.txt"
cp LICENSE.txt "$work/image/LICENSE.txt"
# Include original and framework source alongside the default AGPL build.
tar --exclude=build --exclude=dist --exclude=.git -czf "$work/image/I.R.Choir-source.tar.gz" .
juce_source="${IRCHOIR_JUCE_SOURCE:-build/_deps/juce-src}"
[[ -f "$juce_source/LICENSE.md" ]] || { echo 'Cannot locate corresponding JUCE source; set IRCHOIR_JUCE_SOURCE.' >&2; exit 1; }
cp "$juce_source/LICENSE.md" "$work/image/JUCE-LICENSE.md"
tar --exclude=.git -czf "$work/image/JUCE-8.0.6-source.tar.gz" -C "$juce_source" .
dmg="$out/I.R.Choir-$version-$label.dmg"
hdiutil create -ov -volname 'I.R.Choir Installer' -srcfolder "$work/image" -format UDZO "$dmg"
hdiutil verify "$dmg"
if [[ "$label" == signed ]]; then
  codesign --force --timestamp --sign "$app_identity" "$dmg"
  # Optional credential profile stored in the macOS keychain by the user.
  if [[ -n "${IRCHOIR_NOTARY_PROFILE:-}" ]]; then
    xcrun notarytool submit "$dmg" --keychain-profile "$IRCHOIR_NOTARY_PROFILE" --wait
    xcrun stapler staple "$dmg"
    xcrun stapler validate "$dmg"
  fi
fi
shasum -a 256 "$dmg" > "$dmg.sha256"
echo "Created $dmg"
