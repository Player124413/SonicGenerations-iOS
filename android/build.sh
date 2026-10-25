#!/usr/bin/env bash
# Builds Sonic Generations for Android: the ReXGlue code generator (for the
# build machine) and runtime (from the shared ios/rexglue-sdk tree), the
# recompiled game C++, the app, and an APK.
#
#   bash android/build.sh        # the game code this repository ships
#   bash android/build.sh <default.xex, or the extracted disc folder>
#   bash android/build.sh --smoke        # CI: build the app layer WITHOUT the game
#
#   JOBS   build parallelism (default 2)
#
# Without arguments the build uses the recompiled game code this repository
# ships in ios/sonicgenerations/generated/default (as the original project
# does): a clone builds with no disc at all. With a default.xex, codegen
# regenerates it from your own executable first. --smoke skips the game
# entirely (the generated game code is replaced by a compile stub, see
# stub_codegen.sh): the APK launches and runs the launcher, it just cannot
# play. Everything else is the same build path.
#
# Needs: macOS or Linux, CMake 3.25+, Ninja, a C++ host compiler, JDK 17 and
# the Android SDK (ANDROID_HOME; gradle installs the NDK and CMake packages it
# lists in app/build.gradle.kts on first run).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SDK="$ROOT/ios/rexglue-sdk"
APP="$HERE/sonicgenerations"
JOBS="${JOBS:-2}"
case "$JOBS" in ''|*[!0-9]*|0) echo 'JOBS must be a positive integer.' >&2; exit 1;; esac

SMOKE=0
if [ "${1:-}" = "--smoke" ]; then SMOKE=1; shift; fi

CODEGEN=0
if [ "$SMOKE" = 1 ]; then
  # CI smoke build: no game content anywhere.
  bash "$HERE/stub_codegen.sh"
elif [ -n "${1:-}" ]; then
  CODEGEN=1
  source_xex="$1"
  # The disc names it Default.xex.
  [ -d "$source_xex" ] && source_xex="$(find "$source_xex" -maxdepth 1 -iname default.xex | head -n 1)"
  [ -f "$source_xex" ] || { echo "default.xex not found: $source_xex" >&2; exit 1; }

  # The executable is read from android/game_xex/ (ignored by git, never committed).
  mkdir -p "$HERE/game_xex"
  cp "$source_xex" "$HERE/game_xex/default.xex"
else
  # No disc: build the recompiled game code this repository ships - the same
  # copy the iOS build uses (the original project commits it).
  GEN="$APP/generated/default"
  if [ -f "$GEN/sonicgenerations_nativex_stubs.cpp" ] || [ ! -e "$GEN" ]; then
    rm -rf "$GEN"   # a smoke stub or nothing; never a real checkout (rm would take the link alone)
    mkdir -p "$APP/generated"
    ln -s ../../../ios/sonicgenerations/generated/default "$GEN"
  fi
  [ -f "$GEN/sources.cmake" ] || {
    echo "no game code under ios/sonicgenerations/generated/default" >&2; exit 1; }
  echo "[android] game code -> ios/sonicgenerations/generated/default (shipped)"
fi

for tool in cmake ninja java git; do
  command -v "$tool" >/dev/null || { echo "Required tool missing: $tool" >&2; exit 1; }
done
[ -n "${ANDROID_HOME:-}${ANDROID_SDK_ROOT:-}" ] || {
  echo 'Set ANDROID_HOME (or ANDROID_SDK_ROOT) to your Android SDK.' >&2; exit 1; }

if [ "$CODEGEN" = 1 ]; then
  # --- host code generator --------------------------------------------------
  # The rexglue CLI runs on the build machine and emits the game's C++ into
  # android/sonicgenerations/generated/ (ignored by git; generated per machine).
  host_preset=""
  case "$(uname -s)/$(uname -m)" in
    Darwin/arm64)  host_preset="mac-arm64" ;;
    Darwin/x86_64) host_preset="mac-amd64" ;;
    Linux/x86_64)  host_preset="linux-amd64" ;;
    Linux/aarch64) host_preset="linux-arm64" ;;
    *) echo "Unsupported build machine: $(uname -s)/$(uname -m)" >&2; exit 1 ;;
  esac
  (
    cd "$SDK"
    cmake --preset "$host_preset" -DREXGLUE_REGISTER_PACKAGE=OFF -DREXGLUE_ENABLE_TRACY=OFF
    cmake --build "out/build/$host_preset" --config Release --target rexglue --parallel "$JOBS"
  )
  # The SDK colocates binaries under out/<platform>[-<arch>][/<config>].
  REXGLUE_HOST_TOOL="$(find "$SDK/out/$host_preset" -maxdepth 2 -type f -name rexglue 2>/dev/null | head -n 1)"
  [ -n "$REXGLUE_HOST_TOOL" ] && [ -x "$REXGLUE_HOST_TOOL" ] || {
    echo "host rexglue tool not found under $SDK/out/$host_preset" >&2; exit 1; }

  ( cd "$APP" && "$REXGLUE_HOST_TOOL" codegen sonicgenerations_manifest.toml )
fi

# --- SDL3 Java glue ---------------------------------------------------------
# Staged from the SDK's own SDL checkout (kept out of git; rebuilds are cheap).
SDL_JAVA_SRC="$SDK/thirdparty/sdl3/android-project/app/src/main/java/org/libsdl/app"
SDL_JAVA_DST="$HERE/app/src/main/java/org/libsdl/app"
[ -d "$SDL_JAVA_SRC" ] || { echo "SDL3 Java glue missing under $SDL_JAVA_SRC" >&2; exit 1; }
mkdir -p "$SDL_JAVA_DST"
cp "$SDL_JAVA_SRC"/*.java "$SDL_JAVA_DST/"
echo "[android] SDL3 Java glue -> $SDL_JAVA_DST"

# --- libadrenotools (BSD-2): custom Turnip/Adreno Vulkan driver loader -------
# Pinned commits; built as part of the app's CMake project. Optional: without
# it the build works and only the "import GPU driver" option is unavailable.
AT_DIR="$HERE/sdk/libadrenotools"
AT_REV="8fae8ce254dfc1344527e05301e43f37dea2df80"
LNB_REV="aa3975893d83ef1bc84c321ec60c65fbf1287887"
if [ ! -f "$AT_DIR/CMakeLists.txt" ]; then
  mkdir -p "$HERE/sdk"
  git clone https://github.com/bylaws/libadrenotools.git "$AT_DIR"
  git -C "$AT_DIR" checkout -q "$AT_REV"
  git clone https://github.com/bylaws/liblinkernsbypass.git "$AT_DIR/lib/linkernsbypass"
  git -C "$AT_DIR/lib/linkernsbypass" checkout -q "$LNB_REV"
  echo "[android] libadrenotools -> $AT_DIR"
fi

# --- NativeXRenderer submodules ----------------------------------------------
# native_x_renderer and its DiligentCore (diligent_patches are applied at
# configure time). One shot when the tree is not there yet.
NXR_DIR="$ROOT/native_x_renderer"
if [ ! -f "$NXR_DIR/thirdparty/diligent-core/CMakeLists.txt" ]; then
  git -C "$ROOT" submodule update --init --recursive
  echo "[android] NativeXRenderer submodules fetched"
fi
# NativeXRenderer's device.cpp passes an empty window outside Windows, so the
# renderer could never start on Android; patch the working tree (the submodule
# itself stays pristine), like its own diligent_patches at configure time.
NXR_PATCH="$ROOT/android/patches/native_x_renderer-android-window.patch"
if ! grep -q "NativeWindow window{native_window}" "$NXR_DIR/src/device.cpp" 2>/dev/null; then
  patch -d "$NXR_DIR" -p1 --forward --batch < "$NXR_PATCH" \
    || { echo "[android] native_x_renderer patch failed" >&2; exit 1; }
  echo "[android] native_x_renderer: Android swap chain patched in"
fi

# --- the APK ----------------------------------------------------------------
if [ ! -f "$HERE/ci-release.jks" ]; then
  # A deterministic keystore (the gradle build signs with it when present):
  # the APK installs and updates over itself. Smoke builds guard nothing;
  # for a release of your own, sign it with your own key instead.
  keytool -genkeypair -v -keystore "$HERE/ci-release.jks" -alias rexauto \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -storepass rexauto -keypass rexauto -dname "CN=rexauto" >/dev/null
  echo "[android] generated ci-release.jks"
fi
(
  cd "$HERE"
  ./gradlew --no-daemon assembleRelease
)
apk="$HERE/app/build/outputs/apk/release/app-release.apk"
[ -f "$apk" ] || apk="$HERE/app/build/outputs/apk/release/app-release-unsigned.apk"
[ -f "$apk" ] || apk="$(find "$HERE/app/build/outputs/apk" -name '*.apk' | head -n 1)"
[ -n "$apk" ] && [ -f "$apk" ] || { echo "gradle produced no APK" >&2; exit 1; }
mkdir -p "$HERE/out"
cp "$apk" "$HERE/out/Sonic-Generations-Android.apk"
echo "APK ready: $HERE/out/Sonic-Generations-Android.apk"
if [ "$SMOKE" = 1 ]; then
  echo "This is a smoke APK (signed with ci-release.jks): the launcher runs, the"
  echo "game does not - build with your own default.xex to play."
else
  echo "Install it with adb install (signed with android/ci-release.jks unless you"
  echo "sign it with your own key like any release APK)."
fi
