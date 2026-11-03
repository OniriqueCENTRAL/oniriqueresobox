#!/usr/bin/env bash
# Builds ResoBox on a Mac the way the CI does, for when GitHub has no Mac free
# to run it on: universal VST3 and Audio Unit, the tests, Apple's validator,
# and the two zips the CI would have made. They are then left on the
# ci-build-macOS branch, where the PC and the server pick up the CI's builds.
#
#   bash tools/build-mac.sh        from anywhere in the repository, on main
#
# Needs Xcode (or its command line tools), CMake and push access to the repository.
set -euo pipefail
cd "$(dirname "$0")/.."

stop() { echo "$1" >&2; exit 1; }

[ "$(uname)" = Darwin ] || stop "À lancer sur un Mac."
[ "$(git branch --show-current)" = main ] || stop "À lancer sur la branche main."
[ -z "$(git status --porcelain --untracked-files=no)" ] || stop "Des fichiers sont modifiés : seul ce qui est sur GitHub se compile ici."
git pull -q --ff-only
sha=$(git rev-parse HEAD)

# JUCE 9.0.3, as on the CI, and never another copy found beside the repository.
if [ ! -d JUCE ]; then
    git clone -q --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git JUCE
fi
[ "$(git -C JUCE describe --tags 2>/dev/null)" = 9.0.3 ] || stop "JUCE/ n'est pas JUCE 9.0.3 : supprimez le dossier JUCE et relancez."

work=build/mac
cmake -B "$work" -G Xcode -DRESOBOX_JUCE_DIR="$PWD/JUCE"
cmake --build "$work" --config Release --parallel
ctest --test-dir "$work" -C Release --output-on-failure

out=$work/OniriqueResoBox_artefacts/Release
lipo -info "$out/VST3/ResoBox.vst3/Contents/MacOS/ResoBox"
lipo -info "$out/AU/ResoBox.component/Contents/MacOS/ResoBox"

# Signed ad hoc as a whole, both slices and the bundle, as the CI's Xcode does by
# itself: a local Xcode may leave the Audio Unit with only the linker's signature
# on its arm64 slice. The installed copies are replaced by the signed ones, so
# auval checks what is shipped.
for bundle in "$out/VST3/ResoBox.vst3" "$out/AU/ResoBox.component"; do
    xattr -cr "$bundle"
    codesign --force --sign - "$bundle"
    codesign --verify --strict "$bundle"
done
ditto "$out/AU/ResoBox.component" "$HOME/Library/Audio/Plug-Ins/Components/ResoBox.component"
ditto "$out/VST3/ResoBox.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/ResoBox.vst3"

# Apple's validator, on the copy just installed for this user.
# As on the CI, a failure is reported, not fatal.
killall -9 AudioComponentRegistrar 2>/dev/null || true
if auval -v aufx Rsbx Onrq > "$work/auval.log" 2>&1; then
    tail -n 3 "$work/auval.log"
else
    echo "auval a échoué (voir $work/auval.log) : à regarder avant de publier." >&2
fi

# Zipped with ditto, as on the CI, so the bundles keep their symlinks and signature,
# and without this Mac's extended attributes, which ditto would add as ._ files.
dist=$work/dist
rm -rf "$dist"
mkdir -p "$dist"
ditto -c -k --norsrc --noextattr --noqtn --keepParent "$out/VST3/ResoBox.vst3" "$dist/ResoBox-macOS-VST3.zip"
ditto -c -k --norsrc --noextattr --noqtn --keepParent "$out/AU/ResoBox.component" "$dist/ResoBox-macOS-AU.zip"
echo "$sha - $(date -u +%FT%TZ) - built on $(hostname -s)" > "$dist/BUILD.txt"

# Where the CI leaves its builds: a branch holding nothing else, replaced each time.
remote=$(git remote get-url origin)
(
    cd "$dist"
    git init -q
    git checkout -q -b ci-build-macOS
    git add -A
    git -c user.name=build -c user.email=build@users.noreply.github.com commit -q -m "Build of $sha"
    git push -q --force "$remote" HEAD:ci-build-macOS
) || stop "La compilation est faite mais l'envoi sur GitHub a échoué : les deux zips sont dans $dist."

echo "Fait : ResoBox $sha compilé pour Mac et déposé sur la branche ci-build-macOS."
