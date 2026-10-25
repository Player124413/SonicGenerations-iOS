// sonicgenerations - ReXGlue Recompiled Project
//
// Forwarding accessors for the SDK graphics cvars NativeXRenderer's in-process
// SDK code reads but its sdk_cvars.cpp does not cover (the same pattern and
// reason as that file, decision D15): these are defined in sources the GPU
// plugin keeps (command_processor.cpp, flags.cpp) and read in sources
// NativeXRenderer compiles again (util/draw.cpp, shared_memory.cpp, the SPIR-V
// translator and its utilities). The values forward to the plugin's
// registration by name; nothing is registered from the executable - a second
// registration would be ignored and leave the plugin with its compiled-in
// defaults.

#include "sdk_cvars.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

bool& FLAGS_gpu_disable_msaa_storage_() {
  thread_local native_x_renderer::sdk::Forwarded<bool> forwarded;
  return forwarded.Get("gpu_disable_msaa", false);
}

int32_t& FLAGS_shared_memory_invalidation_widen_kb_storage_() {
  thread_local native_x_renderer::sdk::Forwarded<int32_t> forwarded;
  return forwarded.Get("shared_memory_invalidation_widen_kb", int32_t(256));
}

bool& FLAGS_shared_memory_widen_invalidation_storage_() {
  thread_local native_x_renderer::sdk::Forwarded<bool> forwarded;
  return forwarded.Get("shared_memory_widen_invalidation", true);
}

// The two shader settings mirror flags.cpp's platform-dependent defaults.
bool& FLAGS_shader_no_contraction_storage_() {
  thread_local native_x_renderer::sdk::Forwarded<bool> forwarded;
#if defined(__APPLE__)
  constexpr bool kDefault = false;
#else
  constexpr bool kDefault = true;
#endif
  return forwarded.Get("shader_no_contraction", kDefault);
}

bool& FLAGS_fsi_sample_mask_from_sample_id_storage_() {
  thread_local native_x_renderer::sdk::Forwarded<bool> forwarded;
#if defined(__APPLE__) && TARGET_OS_IPHONE
  constexpr bool kDefault = true;
#else
  constexpr bool kDefault = false;
#endif
  return forwarded.Get("fsi_sample_mask_from_sample_id", kDefault);
}
