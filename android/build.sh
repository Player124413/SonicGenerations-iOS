#!/usr/bin/env bash
# Builds Sonic Generations for Android from your own default.xex: the ReXGlue
# code generator (for the build machine) and runtime (for Android, from the
# shared ios/rexglue-sdk tree), the game's C++ from your executable, the app,
# and an APK.
#
#   bash android/build.sh <default.xex, or the extracted disc folder holding it>
#
#   JOBS   build parallelism (default 2)
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

source_xex="${1:?usage: bash android/build.sh <default.xex or extracted disc folder>}"
# The disc names it Default.xex.
[ -d "$source_xex" ] && source_xex="$(find "$source_xex" -maxdepth 1 -iname default.xex | head -n 1)"
[ -f "$source_xex" ] || { echo "default.xex not found: $source_xex" >&2; exit 1; }

for tool in cmake ninja java git; do
  command -v "$tool" >/dev/null || { echo "Required tool missing: $tool" >&2; exit 1; }
done
[ -n "${ANDROID_HOME:-}${ANDROID_SDK_ROOT:-}" ] || {
  echo 'Set ANDROID_HOME (or ANDROID_SDK_ROOT) to your Android SDK.' >&2; exit 1; }

# The executable is read from android/game_xex/ (ignored by git, never committed).
mkdir -p "$HERE/game_xex"
cp "$source_xex" "$HERE/game_xex/default.xex"

# --- host code generator ----------------------------------------------------
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

# --- the APK ----------------------------------------------------------------
(
  cd "$HERE"
  ./gradlew --no-daemon assembleRelease
)
apk="$HERE/app/build/outputs/apk/release/app-release-unsigned.apk"
[ -f "$apk" ] || apk="$(find "$HERE/app/build/outputs/apk" -name '*.apk' | head -n 1)"
[ -n "$apk" ] && [ -f "$apk" ] || { echo "gradle produced no APK" >&2; exit 1; }
mkdir -p "$HERE/out"
cp "$apk" "$HERE/out/Sonic-Generations-Android.apk"
echo "APK ready: $HERE/out/Sonic-Generations-Android.apk"
echo "Install it with adb install (it is unsigned unless android/ci-release.jks exists;"
echo "for other tools sign it like any release APK)."
