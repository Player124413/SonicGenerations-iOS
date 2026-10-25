// sonicgenerations - ReXGlue Recompiled Project
//
// NativeXRenderer profile for Sonic Generations (Xbox 360). Every offset below
// carries the evidence it was derived from; the full map is in
// docs/native-x-renderer-sonicgenerations.md. The head of the device (the
// dirty masks, the command buffer, the constants and the register shadow) is
// the Dante's Inferno / GHMetal layout; the block of current objects after it
// is this game's own, found field by field.

#include "native_x_renderer_profile.h"

namespace sonicgenerations {

using native_x_renderer::DeviceLayout;
using native_x_renderer::ObjectLayout;
using native_x_renderer::Profile;

namespace {

Profile BuildProfile() {
  Profile p;
  p.game = "sonicgenerations";
  // The XDK the game was linked against. The device layout is the family of
  // Dante's Inferno (TU2) and Guitar Hero: Metallica (2.0.7978): everything up
  // to the end of the register shadow is Dante's, the tail is this game's own.
  // Confirm the exact number with recomp-tools' xdkver.mjs on default.xex.
  p.xdk_version = "2.0.7978 family (Dante/GHMetal layout)";
  // The game renders 1280x720 (its front buffer).
  p.output_width = 1280;
  p.output_height = 720;

  // --- Dirty masks: six u64 from +0 (VS, PS, register groups, fetch, misc,
  // tiling). SetVertexShaderConstantF writes the mask at +0 and
  // SetPixelShaderConstantF the one at +8; the shader setters mark +16 (the
  // groups) and SetStreamSource's fetch dirty bits live in the fourth mask.
  p.device.vs_constants_dirty = 0;
  p.device.ps_constants_dirty = 8;
  p.device.fetch_constants_dirty = 24;
  // Fetch slot s is bit 31 - s of the low half (the flush shifts it up and
  // walks it with cntlzd), the POTCAWE order.
  p.device.fetch_dirty_slot0_bit = 31;

  // The command buffer: the write pointer holds the last dword written
  // (stwu pre-increment), the limit next to it, then the kick threshold.
  // Every packet writer in the library does lwz+56 / lwz+48 / stw+48.
  p.device.command_buffer_write = 48;

  // Constants. The Present copy draw writes its pixel constants to +6016
  // (stfs+6016...) and a fetch constant to +1152; the float constants follow
  // Dante's offsets (+1920, +6016, +10112).
  p.device.fetch_constants = 1152;
  p.device.vs_float_constants = 1920;
  p.device.ps_float_constants = 6016;
  p.device.bool_loop_constants = 10112;

  // The register shadow, in contiguous groups. Confirmed by rs_setters.py:
  // the setters decode to the right register names (RB_DEPTHCONTROL at
  // +10548 = 0x2200, RB_BLEND* at 0x2201..., SQ_PROGRAM_CNTL at +10528 =
  // 0x2180). Same offsets as Dante's Inferno.
  p.device.register_groups = {{{10368, 0x2000, 19},
                               {10444, 0x2100, 21},
                               {10528, 0x2180, 5},
                               {10548, 0x2200, 12},
                               {10596, 0x2280, 21},
                               {10680, 0x2300, 38},
                               {10832, 0x2380, 8}}};

  // The kick (sub_82DB47C8) stamps the segment from +11024 and reads the
  // fence pair at +11036/+11040 (the last fence the GPU reached is [+0] of the
  // block +11036 points to).
  p.device.fence_counter = 11024;
  p.device.write_back_block = 11036;

  // The flag bytes (tiling and predication) share one u32 at +11068:
  // +11068 tiling, +11070 inline draw, +11071 program flush, +11072
  // predication. The tiling pair tests bits 26-29 of +11068.
  p.device.flags = 11068;

  // The block of current objects, this game's own spacing:
  // SetVertexDeclaration writes +12216; SetIndices writes +12788, right
  // before the five target pointers RT0-3 at +12792 and depth at +12808
  // (the tiling and resolve paths read them interleaved with their
  // companions at +13080); the shader setters write +13044 (pixel) and
  // +13048 (vertex); the draw flush reads the viewport floats at +13000.
  p.device.vertex_declaration = 12216;
  p.device.index_buffer = 12788;
  p.device.render_targets = 12792;
  p.device.depth_stencil = 12808;
  p.device.viewport = 13000;
  p.device.pixel_shader = 13044;
  p.device.vertex_shader = 13048;
  // Next to the shaders, written around the predication flag byte.
  p.device.predication_mask = 13052;
  // BeginTiling stores the bracket's count and rects pointer here (the
  // tiling and clear paths write both); confirm against the trace before
  // trusting the tiling rules.
  p.device.tile_count = 11060;
  p.device.tile_rects = 11064;
  // EndVertices (three instructions: lwz+13828, stw+48) copies this into the
  // write pointer: where BeginVertices saved the end of its inline packets.
  p.device.inline_draw_end = 13828;
  // Vertex buffers and the texture object array: still being mapped (the
  // setters write fetch constants computed from the stream/sampler, so the
  // arrays only show up in the trace's argument dumps).

  // --- Resource objects. The GetDesc-style getters read +24 and +28 of
  // surfaces and buffers (POTCAWE's layout).
  p.objects.texture_fetch = 28;
  p.objects.surface_info = 24;
  p.objects.surface_color_depth_info = 28;
  p.objects.surface_size = 36;
  p.objects.surface_width_minus_one = {18, 14};   // bits 18-31
  p.objects.surface_height_minus_one = {3, 15};  // bits 3-17
  p.objects.buffer_address = 24;
  p.objects.buffer_size = 28;
  return p;
}

}  // namespace

const Profile& NativeXRendererProfile() {
  static const Profile profile = BuildProfile();
  return profile;
}

}  // namespace sonicgenerations
