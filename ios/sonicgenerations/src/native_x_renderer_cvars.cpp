// sonicgenerations - ReXGlue Recompiled Project
//
// A forwarding accessor for the SDK graphics cvar NativeXRenderer's in-process
// SDK code reads but its sdk_cvars.cpp does not cover (the same pattern and
// reason as that file, decision D15): gpu_disable_msaa is defined in
// command_processor.cpp, which stays the GPU plugin's, and read in
// util/draw.cpp, which NativeXRenderer compiles again. The value forwards to
// the plugin's registration by name; nothing is registered from the
// executable.

#include "sdk_cvars.h"

bool& FLAGS_gpu_disable_msaa_storage_() {
  thread_local native_x_renderer::sdk::Forwarded<bool> forwarded;
  return forwarded.Get("gpu_disable_msaa", false);
}
