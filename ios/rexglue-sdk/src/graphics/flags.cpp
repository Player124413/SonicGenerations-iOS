/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <rex/graphics/flags.h>
#include <rex/logging.h>
#include <rex/ui/renderdoc_api.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

REXCVAR_DEFINE_BOOL(gpu_allow_invalid_fetch_constants, false, "GPU",
                    "Allow invalid fetch constants");
REXCVAR_DEFINE_BOOL(gpu_allow_null_optional_streams, true, "GPU",
                    "Draw meshes whose only invalid vertex fetch constants are all-zero "
                    "optional streams (no vertex binding with a stride reads them; their "
                    "fetches return zeros). The Simpsons Game binds absent blend-shape "
                    "streams this way, and dropping those draws left characters invisible");
REXCVAR_DEFINE_BOOL(native_2x_msaa, true, "GPU", "Enable native 2x MSAA");
REXCVAR_DEFINE_BOOL(depth_float24_round, false, "GPU", "Round float24 depth values");
REXCVAR_DEFINE_BOOL(depth_float24_convert_in_pixel_shader, false, "GPU",
                    "Convert float24 depth in pixel shader");
REXCVAR_DEFINE_BOOL(depth_transfer_not_equal_test, true, "GPU",
                    "Use not-equal test for depth transfer");
REXCVAR_DEFINE_BOOL(gamma_render_target_as_unorm16, true, "GPU",
                    "Use R16G16B16A16_UNORM for gamma render targets (more accurate than sRGB)")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);
REXCVAR_DEFINE_STRING(dump_shaders, "", "GPU", "Path to dump shaders to");
REXCVAR_DEFINE_BOOL(use_fuzzy_alpha_epsilon, false, "GPU",
                    "Use approximate compare for alpha test values to prevent "
                    "flickering on NVIDIA graphics cards");
REXCVAR_DEFINE_BOOL(gpu_debug_markers, false, "GPU",
                    "Insert debug markers into GPU command streams for tools "
                    "like PIX and RenderDoc. Automatically enabled when "
                    "RenderDoc is detected.");

// Shared-memory invalidation (read in shared_memory.cpp) and the two shader
// settings (read in the SPIR-V translator and its utilities). Defined here,
// the GPU plugin's one registration point: NativeXRenderer recompiles those
// sources into the executable (D15), where forwarding accessors read these
// values back by name - a second registration would be ignored and leave the
// plugin with its compiled-in defaults (the game's native_x_renderer_cvars.cpp
// forwards these names for the executable's copy).
REXCVAR_DEFINE_INT32(shared_memory_invalidation_widen_kb, 256, "GPU",
                     "How far, in KB, an invalidation from a CPU write to GPU-used memory may "
                     "widen to save later write faults (aligned; up to 64 host pages). 256 KB "
                     "matches 4 KB-page hosts; with 16 KB pages the old fixed 64 pages was 1 MB")
    .range(0, 4096);
REXCVAR_DEFINE_BOOL(shared_memory_widen_invalidation, true, "GPU",
                    "On a CPU write to GPU-watched memory, invalidate up to the surrounding 256 KB "
                    "instead of only the written pages: fewer access faults, but textures that "
                    "share the block with per-frame CPU data are reloaded every frame.")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

// Metal has no fragment-input coverage mask; MoltenVK synthesises
// gl_SampleMaskIn and never reports sample 0 on iOS GPUs, so default this on
// there. macOS Metal reports it correctly, so leave it off to avoid paying for
// per-sample invocations.
#if defined(__APPLE__) && TARGET_OS_IPHONE
#define REX_FSI_SAMPLE_ID_COVERAGE_DEFAULT true
#else
#define REX_FSI_SAMPLE_ID_COVERAGE_DEFAULT false
#endif

// NoContraction is expanded by SPIRV-Cross into a call to an
// [[clang::optnone]] fma() helper for every decorated operation. On Metal that
// both defeats optimization in the hottest shaders and miscompiles: in
// one title's terrain vertex shader the world-space Y component came out as a
// constant while X and Z stayed exact, flattening every chunk into a sliver.
// The reproducibility it buys is only needed for multipass rendering that
// relies on bit-exact results between shaders, which is not worth a miscompile.
#if defined(__APPLE__)
#define REX_SHADER_NO_CONTRACTION_DEFAULT false
#else
#define REX_SHADER_NO_CONTRACTION_DEFAULT true
#endif

REXCVAR_DEFINE_BOOL(shader_no_contraction, REX_SHADER_NO_CONTRACTION_DEFAULT, "GPU/Shader",
                    "Decorate arithmetic with NoContraction for bit-exact reproducibility across "
                    "shaders. Must be off on Metal: SPIRV-Cross expands every decorated operation "
                    "into an [[clang::optnone]] fma() helper, and the Metal compiler miscompiles "
                    "the resulting shader (a vertex position component comes out constant), as "
                    "well as losing all optimization in the hottest shaders.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_BOOL(
    fsi_sample_mask_from_sample_id, REX_FSI_SAMPLE_ID_COVERAGE_DEFAULT, "GPU/Shader",
    "Fragment shader interlock: derive the MSAA coverage mask from gl_SampleID "
    "with sample-rate shading instead of reading gl_SampleMaskIn. Needed on "
    "drivers that do not report a fragment-input coverage mask correctly "
    "(Metal/MoltenVK has no native equivalent, and drops sample 0), at the cost "
    "of running the pixel shader once per covered sample. Defaults on for iOS.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

bool IsGpuDebugMarkersEnabled() {
  static bool cached = false;
  static bool result = false;
  if (!cached) {
    cached = true;
    if (REXCVAR_GET(gpu_debug_markers)) {
      result = true;
      REXLOG_INFO("GPU debug markers enabled via CVar");
    } else {
      auto renderdoc_api = rex::ui::RenderDocAPI::CreateIfConnected();
      if (renderdoc_api) {
        result = true;
        REXLOG_INFO("GPU debug markers auto-enabled (RenderDoc detected)");
      }
    }
  }
  return result;
}
