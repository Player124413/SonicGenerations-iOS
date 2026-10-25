# Sonic Generations for Android: detailed guide

The Android build of the same recompilation the iOS app runs:
*Sonic Generations* (Xbox 360, 2011) statically recompiled (PowerPC → C++ →
arm64) on the [ReXGlue](../ios/rexglue-sdk) runtime (derived from Xenia) -
Xbox 360 kernel, audio, input and graphics (Vulkan, straight onto the device's
own Vulkan driver: no translation layer on this side). The app layer is the
shared one: the game code, the [HedgeModManager mod loader](../ios/sonicgenerations/src/mods),
the 60 FPS presentation hook and the spin-wait fixes are the **same sources**
the iOS build compiles ([`ios/sonicgenerations/src`](../ios/sonicgenerations/src));
only the platform glue (this folder) differs.

**No game content is included. You need your own copy of the Xbox 360 game.**

## Requirements

- A phone or tablet with Android 9+ (arm64-v8a) and a Vulkan 1.1 GPU
  (every mainstream SoC since ~2018). Developed against a Snapdragon-class
  device; Mali and Xclipse work through the same Vulkan path.
- *Sonic Generations* for Xbox 360: a disc image (ISO/XISO), a bare
  `default.xex`, or a folder with the disc's files. The World release
  (Redump) is what the iOS port was developed with.
- About 8 GB of free space for the game.

## How to play

1. **Build** the APK (`bash android/build.sh "<path to disc or default.xex>"`,
    see [Building](#building)) or install a release APK someone built for you.
2. **Install** it (`adb install out/Sonic-Generations-Android.apk`, or any
   installer). The build is unsigned unless `android/ci-release.jks` exists.
3. **Give it the game**: the launcher opens on first start - tap
   **Choose ISO / default.xex** and pick your disc image (it is extracted into
   the app's own storage), or **Choose folder** for an already-extracted disc.
   Anything that is not *Sonic Generations* (Title ID `53450848`) is refused.
4. Tap **Play**.

The launcher also holds the graphics/performance dialog, the GPU driver
manager (Adreno), logs and the crash report, and can remove the game files.

## What lives where

Everything is in the app's external files dir, visible to file managers and
`adb` at `Android/data/com.devz.sonicgenerations/files/`:

| Path | What |
|---|---|
| `game/` | the extracted disc (`Default.xex`, `disk/`, `movie/`) |
| `game_root.txt` | the staged game root, written by the launcher |
| `settings.txt` | graphics/performance options from the launcher (cvars) |
| `mods/` | HedgeModManager mod folders (each with `mod.ini`) |
| `ios-mods.ini` | which mods are enabled, and their order (the name is kept from the iOS port) |
| `data/` | saves and the console profile |
| `cache/` | shader cache, merged mod archives |
| `sonicgenerations.toml` | settings, editable with any file manager |
| `logs/` | the game log and `crash.txt` (shown by the launcher after a crash) |

## Controls

The on-screen gamepad (Unleashed Recompiled iOS port's layout, in Java over
the SDL surface): a stick with a D-pad ring, swipe anywhere else to turn the
camera, and the face, shoulder, trigger, BACK and START buttons. The pill
**MODS** button, top left, opens the mod manager. The pad's own settings
(size, opacity, haptics, hide) come from its settings dialog; it hides while
an MFi/Xbox/PlayStation (or any SDL gamepad) controller is connected. Set it
up in the launcher's graphics dialog or from the pad itself.

## Mods

Mods made for **HedgeModManager's Sonic Generations** profile go in
`files/mods/<Mod name>/mod.ini` (copy the folder in with a file manager, or
tap **MODS → Import folder** in game). In game, **MODS** pauses the game and
opens the manager: import, enable, reorder (the top mod wins). Mods load when
the game starts; a changed selection offers **Quit Game** (applies on the next
launch) or **Keep Playing**.

File replacements, additions and append archives (`+Name.ar`) work. Windows
code (`DLLFile`, `CodeFile`) and PC-only data (Direct3D 9 shaders, little-endian
Havok files) cannot run on the Xbox 360 game and are skipped; the rest of such
a mod still loads. Details in the
[iOS guide](../ios/sonicgenerations/README.md#mods) - the loader is the same
code.

## Settings

In `sonicgenerations.toml` (takes effect on the next launch):

| Setting | Values |
|---|---|
| `sg_frame_rate` | `"60"` (default, capped at 60), `"vsync"` (60, locked to the display refresh), `"original"` (30) |
| `present_letterbox` | `false` stretches to the screen, `true` keeps 16:9 with side bars |
| `touch_controls` | `"auto"`, `"on"`, `"off"`; `touch_controls_opacity` 0.1 to 1.0 |

The launcher's **Graphics & performance** dialog writes `settings.txt`, which
overrides the toml on the next launch: resolution scale, vsync, video mode,
letterbox, tolerant mode, orientation, a performance preset
(`performance`/`balanced`/`accuracy`) and a frame-rate cap.

## Performance: what this port does on Android

The runtime defaults (`SetDefaults()` in the shared app class) are the iOS
port's, which came out of the Simpsons, NFSMW, MC360 and Superstars Tennis
ports of this SDK - tuned for tile-based mobile GPUs:

- **60 FPS instead of 30.** The console presents every second vblank; a hook
  on that instruction (`0x82DC44E0`) sets the presentation interval so frames
  present as soon as they are ready, capped at 60 (`sg_frame_rate`).
- **Busy waits yield.** Havok's job wait and D3D's GPU poll spin on the
  console; hooks (`src/spin_waits.cpp`) make them yield/sleep - on big.LITTLE
  SoCs the spinning otherwise steals cores from the very threads they wait for.
- **Real memory fences.** The Xbox 360's `sync`/`lwsync`/`eieio` became nothing
  in recompilations for x86; on ARM they are real barriers now (without them
  Sonic fell through the ground or out of the 2D path).
- **Tile-friendly rendering.** Tight render areas and batched dirty-page
  uploads (`vulkan_tight_render_area`, `vulkan_batch_dirty_uploads`), direct
  uploads into the GPU's buffer (`vulkan_shared_memory_direct_upload`,
  `vulkan_defer_cpu_uploads`), one queue submission per frame, host render
  targets (`render_target_path_vulkan=fbo`), and MSAA views that share one
  host target (`vulkan_msaa_as_single_sample`).
- **Upload traffic cut 6x.** A CPU write invalidates only its own 16 KB page
  (`shared_memory_invalidation_widen_kb=16`), not the surrounding 1 MB - the
  difference between re-uploading ~550 MB/s and ~85 MB/s (51 → 60 FPS on an
  M1 in the iOS measurements).
- **No per-frame guest-memory re-upload** (`clear_memory_page_state=false`).
- **Runtime compiled for the SoC**: thin LTO, ARMv8.2-A with fp16/dotprod,
  `-fno-plt`/hidden visibility for cheaper dispatcher calls (the recompiled
  code is millions of small functions; per-call overhead is the CPU
  bottleneck). Set `REX_ANDROID_BASELINE_ARM=1` to build plain armv8-a.
- **Thermal-aware frame cap.** The OS thermal status (Android 10+) drops
  `framerate_limit` live (45 FPS warm, 30 FPS hot) instead of letting the
  phone throttle into a slideshow, and lifts the cap when it cools down.
- **Game Mode + sustained performance.** The app asks for the OS performance
  game mode (Android 13+) and the sustained clock profile, keeps the screen
  on, and runs the guest threads at `nice -10`.
- **Custom GPU drivers (Adreno).** The launcher imports Turnip/Adreno driver
  zips (the Yuzu/Skyline/Dolphin/Winlator format) and the app loads one
  through libadrenotools before the runtime starts - often the single biggest
  quality/perf difference on Snapdragon.
- **Quiet, batched logging** (FUSE-backed storage makes per-line flushes
  visible in frame times), a logcat sink, and a native crash report
  (`logs/crash.txt`, `sub_XXXXXXXX` = guest function) the launcher shows after
  a crash.

The Android-specific SDK fixes (join-once thread handles for bionic, the
`/proc/self/maps` cache on the write-watch fault path, interrupt dispatch that
does not hold the global lock, signal chaining to the crash reporter) live in
[`../ios/rexglue-sdk`](../ios/rexglue-sdk) and benefit every platform.

## Building

On a Mac or Linux machine with CMake 3.25+, Ninja, a host C++ compiler,
JDK 17 and the Android SDK (`ANDROID_HOME`; the first Gradle run installs the
NDK and CMake packages listed in `app/build.gradle.kts`):

```bash
bash android/build.sh "path/to/Sonic Generations"   # default.xex, or the extracted disc folder
```

That runs the ReXGlue code generator on the build machine (the game's C++ is
generated from *your* `default.xex` and never committed), stages SDL's Java
glue, clones libadrenotools (pinned), and builds
`android/out/Sonic-Generations-Android.apk` with Gradle.

To build without the game (e.g. for a CI smoke test of the app layer), point
`android/sonicgenerations/generated/default` at an existing codegen output.

## Device tools

- `adb logcat -s rexglue` - the game's log (also written to `files/logs/`).
- `files/cleanup.txt` - game data paths to delete on the next launch, one per
  line, relative to `files/game` (the same trick as the iOS port).
- The launcher's **Logs & crash report** dialog shows the log tail and any
  `crash.txt`, with Share/Copy.

## Credits

- [Sonic Generations Recompiled](https://github.com/Player124413/Sonic-Generations-recomp-android-and-pc-edition)
  by Player124413: the recompilation this port builds on, and whose Android
  launcher (setup flow, virtual gamepad, GPU driver manager, crash reporter)
  this app reuses and extends with the mod manager and the shared app layer.
- [hells-gate-recomp-android](https://github.com/deivid22srk/hells-gate-recomp-android)
  (deivid22srk): the Android runtime glue (`android_runtime.cpp`, the jthread
  shim, the logcat sink) and the mobile performance work this SDK tree
  integrates.
- [ReXGlue](https://github.com/rexglue/rexglue-sdk) and
  [Xenia](https://github.com/xenia-project/xenia): the recompiler and runtime.
- [libadrenotools](https://github.com/bylaws/libadrenotools) (bylaws): custom
  Adreno/Turnip driver loading.
- [MoltenVK](https://github.com/KhronosGroup/MoltenVK), [SDL](https://github.com/libsdl-org/SDL)
  and the libraries in `ios/rexglue-sdk/thirdparty`.

*Sonic Generations* is a trademark of SEGA. This project is not affiliated with
or endorsed by SEGA, and includes none of the game's content.
