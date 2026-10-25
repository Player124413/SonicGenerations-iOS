// sonicgenerations - ReXGlue Recompiled Project
//
// NativeXRenderer profile for Sonic Generations (Xbox 360): where the game's
// XDK Direct3D device keeps what the native renderer reads. Mapped from the
// recompiled game's D3D library (0x82DA0000-0x82DC9600) with the PM4 scan,
// the draws' flush and the render-state setters; the evidence is in
// docs/native-x-renderer-sonicgenerations.md. The device layout is the
// Dante's Inferno / GHMetal family (the register shadow at +10368, fetch
// constants at +1152), not POTCAWE's 5426 form.

#pragma once

#include <native_x_renderer/native_x_renderer.h>

namespace sonicgenerations {

// The profile for one run. The app passes it to native_x_renderer::Initialize()
// from its OnPostSetup, before the guest starts (SonicGenerationsApp::InitNativeXRenderer).
const native_x_renderer::Profile& NativeXRendererProfile();

}  // namespace sonicgenerations
