# Sonic Generations for iPhone, iPad and Android

A static recompilation of *Sonic Generations* (Xbox 360, 2011) for iOS and Android, based on
[Sonic Generations Recompiled](https://github.com/Player124413/Sonic-Generations-recomp-android-and-pc-edition)
by Player124413. The game's PowerPC code is translated ahead of time to C++ and compiled for
arm64, and runs on the [ReXGlue](https://github.com/rexglue/rexglue-sdk) runtime (derived from
Xenia), which provides the Xbox 360 kernel, audio, input and graphics (Vulkan on Metal through
MoltenVK on iOS; Vulkan directly on Android). It runs at 60 FPS, with touch controls and a
HedgeModManager-style mod loader. The two builds share the app layer
([`ios/sonicgenerations/src`](ios/sonicgenerations/src)) and the SDK tree; only the platform
glue differs ([`android/`](android)).

**No game content is included. You need your own copy of the Xbox 360 game.**

## Requirements

**iOS:** an iPhone or iPad on iOS 15 or later (developed and tested on an iPhone 16 Pro Max).

**Android:** a phone or tablet on Android 9+ (arm64-v8a) with a Vulkan 1.1 GPU.

Both: *Sonic Generations* for Xbox 360: a disc image (ISO/XISO) or a folder with the disc's files
(`Default.xex`, `disk/`, `movie/`). Developed with the World release (Redump), whose
`default.xex` has SHA-256 `b892fc6cf18062908bde392b9845b41b67ad755dfec88c686e4c4647fcb62d76`.
About 8 GB of free space for the game (a disc image is kept as it is and read in place).

## How to play

### iOS

1. **Download** `Sonic-Generations-iOS.ipa` from the
   [latest release](https://github.com/DevZer0D4Y/SonicGenerations-iOS/releases/latest) (or build it
   yourself, see [Building](#building)).
2. **Install it** with a sideloading tool: AltStore, SideStore, Sideloadly, ESign or TrollStore.
   The IPA is unsigned; the tool signs it. Your game files and saves are kept when you re-sign it.
3. **Give it the game**, either way:
   - open the app and tap **Choose ISO** to pick a disc image (it is copied into the app), or
   - copy the extracted disc into Files → On My iPhone → Sonic Generations → `game/`.
4. Play.

Saves, settings (`sonicgenerations.toml`) and logs are in the app's Documents folder, visible in
the Files app.

### Android

1. **Build** the APK with `bash android/build.sh "<path to disc or default.xex>"`
   (see [Building](#building)), or install a release APK built the same way.
2. **Install** it: `adb install android/out/Sonic-Generations-Android.apk` (or any installer).
3. **Give it the game**: the launcher opens on first start - tap **Choose ISO / default.xex** and
   pick your disc image (it is extracted into the app's storage), or **Choose folder** for an
   already-extracted disc. Anything that is not *Sonic Generations* is refused.
4. Tap **Play**.

Everything is in `Android/data/com.devz.sonicgenerations/files/` (game, saves, settings, mods,
logs) - see the [Android guide](android/README.md) for the details, the graphics dialog and the
performance notes.

## Controls

Without a controller, an on-screen gamepad covers the Xbox 360 pad: a stick with a D-pad ring,
swipe anywhere else to turn the camera, and the face, shoulder, trigger, BACK and START buttons.
**EDIT** opens a layout editor (drag to move; size, hide, opacity, reset); the layout is kept. The
gamepad hides while an MFi, Xbox or PlayStation controller is connected. On Android the same
layout runs over the game window, with its own size/opacity/haptics settings and a **MODS** pill
beside it.

## Mods

Mods made for **HedgeModManager's Sonic Generations** profile go in Files → Sonic Generations →
`mods/<Mod name>/mod.ini` (on Android: `files/mods/`, or **MODS → Import folder** in game). In
game, the **MODS** button (beside EDIT) opens the mod manager: import folders, enable, reorder
(the top mod wins). Mods load when the game starts.

File replacements, additions and append archives (`+Name.ar`) work. Windows code (`DLLFile`,
`CodeFile`) and PC-only data (Direct3D 9 shaders, little-endian Havok files) cannot run on the
Xbox 360 game and are skipped; the rest of such a mod still loads. Details in the
[guide](ios/sonicgenerations/README.md#mods).

## Settings

In `sonicgenerations.toml` (takes effect on the next launch):

| Setting | Values |
|---|---|
| `sg_frame_rate` | `"60"` (default, capped at 60), `"vsync"` (60, locked to the display refresh), `"original"` (30) |
| `present_letterbox` | `false` stretches to the screen, `true` keeps 16:9 with side bars |
| `touch_controls` | `"auto"`, `"on"`, `"off"`; `touch_controls_opacity` 0.1 to 1.0 |

## Building

**iOS** - on an Apple Silicon Mac with Xcode, CMake 3.25+, Ninja and Python 3, with your own disc:

```bash
bash ios/build.sh "path/to/Sonic Generations"   # default.xex, or the extracted disc folder
```

The IPA is written to `ios/sonicgenerations/out/Sonic-Generations-iOS.ipa`. The game's code is
generated from your `default.xex` at build time and never committed. Details, the mod loader's
design and the device scripts: [ios/sonicgenerations/README.md](ios/sonicgenerations/README.md).

**Android** - on a Mac or Linux machine with CMake 3.25+, Ninja, JDK 17 and the Android SDK
(`ANDROID_HOME`), with your own disc:

```bash
bash android/build.sh "path/to/Sonic Generations"   # default.xex, or the extracted disc folder
```

The APK is written to `android/out/Sonic-Generations-Android.apk`. Same story: the game's code is
generated from your `default.xex` at build time. Details, the launcher and the performance notes:
[android/README.md](android/README.md).

## Credits

- [Sonic Generations Recompiled](https://github.com/Player124413/Sonic-Generations-recomp-android-and-pc-edition)
  by Player124413: the recompilation this port builds on (and the Android launcher this app's
  is based on).
- [hells-gate-recomp-android](https://github.com/deivid22srk/hells-gate-recomp-android) by
  deivid22srk: the Android runtime glue and mobile performance work integrated in the SDK tree.
- [ReXGlue](https://github.com/rexglue/rexglue-sdk) and [Xenia](https://github.com/xenia-project/xenia):
  the recompiler and runtime.
- The touch controls follow the Unleashed Recompiled iOS port's layout.
- [libadrenotools](https://github.com/bylaws/libadrenotools): custom Adreno/Turnip driver loading
  on Android.
- [MoltenVK](https://github.com/KhronosGroup/MoltenVK), [SDL](https://github.com/libsdl-org/SDL)
  and the libraries in `ios/rexglue-sdk/thirdparty`.

*Sonic Generations* is a trademark of SEGA. This project is not affiliated with or endorsed by
SEGA, and includes none of the game's content.
