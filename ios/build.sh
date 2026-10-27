#!/usr/bin/env bash
# Builds Sonic Generations for iOS: the ReXGlue SDK and runtime (for iOS), the
# recompiled game C++ and the app, into an unsigned IPA.
#
#   bash ios/build.sh [default.xex, or the extracted disc folder holding it]
#
# With a default.xex, the game's C++ is regenerated from your own executable
# first. Without one, the build uses the recompiled game code this repository
# ships in sonicgenerations/generated/default (as the original project does):
# a clone builds with no disc at all.
#
#   JOBS   build parallelism (default 2)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK="$HERE/rexglue-sdk"
APP="$HERE/sonicgenerations"
JOBS="${JOBS:-2}"
case "$JOBS" in ''|*[!0-9]*|0) echo 'JOBS must be a positive integer.' >&2; exit 1;; esac

source_xex=""
if [ -n "${1:-}" ]; then
  source_xex="$1"
  # The disc names it Default.xex.
  [ -d "$source_xex" ] && source_xex="$(find "$source_xex" -maxdepth 1 -iname default.xex | head -n 1)"
  [ -f "$source_xex" ] || { echo "default.xex not found: $source_xex" >&2; exit 1; }
fi
if [ -z "$source_xex" ] && [ ! -f "$APP/generated/default/sources.cmake" ]; then
  echo 'No default.xex and no sonicgenerations/generated/default: nothing to build the game from.' >&2
  exit 1
fi

[ "$(uname -s)" = Darwin ] || { echo 'Build on macOS with Xcode installed.' >&2; exit 1; }
for tool in cmake ninja python3 xcrun codesign zip; do
  command -v "$tool" >/dev/null || { echo "Required tool missing: $tool" >&2; exit 1; }
done
xcrun --sdk iphoneos --show-sdk-path >/dev/null

# NativeXRenderer submodules (native_x_renderer and its DiligentCore).
if [ ! -f "$HERE/../native_x_renderer/thirdparty/diligent-core/CMakeLists.txt" ]; then
  git -C "$HERE/.." submodule update --init --recursive
  echo "[ios] NativeXRenderer submodules fetched"
fi
# NativeXRenderer's device.cpp passes an empty window outside Windows; the
# Android branch of the patch matters here only when the same tree is reused
# by the Android build, but keeping both trees identically patched avoids
# surprises. The submodule itself stays pristine.
NXR_DIR="$HERE/../native_x_renderer"
NXR_PATCH="$HERE/../android/patches/native_x_renderer-android-window.patch"
if ! grep -q "NativeWindow window{native_window}" "$NXR_DIR/src/device.cpp" 2>/dev/null; then
  patch -d "$NXR_DIR" -p1 --forward --batch < "$NXR_PATCH" \
    || { echo "[ios] native_x_renderer patch failed" >&2; exit 1; }
  echo "[ios] native_x_renderer: window patch applied"
fi
NXR_GUARD_PATCH="$HERE/../android/patches/native_x_renderer-null-device-guard.patch"
if ! grep -q "mislabeled hook, skipped" "$NXR_DIR/src/state.cpp" 2>/dev/null; then
  patch -d "$NXR_DIR" -p1 --forward --batch < "$NXR_GUARD_PATCH" \
    || { echo "[ios] native_x_renderer guard patch failed" >&2; exit 1; }
  echo "[ios] native_x_renderer: null-device guard patched in"
fi

# The executable is read from ios/game_xex/ (ignored by git, never committed).
if [ -n "$source_xex" ]; then
  mkdir -p "$HERE/game_xex"
  cp "$source_xex" "$HERE/game_xex/default.xex"
fi

(
  cd "$SDK"
  if [ -n "$source_xex" ]; then
    # The code generator, a Mac program, with the runtime library it links.
    cmake --preset mac-arm64 -DREXGLUE_REGISTER_PACKAGE=OFF -DREXGLUE_ENABLE_TRACY=OFF
    cmake --build out/build/mac-arm64 --config Release --target rexglue --parallel "$JOBS"
  fi
  # The runtime for iOS.
  cmake --preset ios-arm64 -DREXGLUE_REGISTER_PACKAGE=OFF
  cmake --build out/build/ios-arm64 --config Release --parallel "$JOBS"
  cmake --install out/build/ios-arm64 --config Release
)
export REXGLUE_IOS_SDK="$SDK/out/install/ios-arm64"

(
  cd "$APP"
  if [ -n "$source_xex" ]; then
    export REXGLUE_HOST_TOOL="$SDK/out/mac-arm64/Release/rexglue"
    "$REXGLUE_HOST_TOOL" codegen sonicgenerations_manifest.toml
  fi
  cmake --preset ios-arm64-release
  cmake --build --preset ios-arm64-release --parallel "$JOBS"
  bash ios/make-ipa.sh
)
echo "IPA ready: $APP/out/Sonic-Generations-iOS.ipa"
