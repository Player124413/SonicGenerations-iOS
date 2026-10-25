// sonicgenerations - ReXGlue Recompiled Project
//
// NativeXRenderer hooks over Sonic Generations' XDK Direct3D entry points
// (the library at 0x82DA0000-0x82DC9600). Each address is a function the
// game calls; the evidence is in docs/native-x-renderer-sonicgenerations.md.
// Hooks run the original function either way: when no NativeXRenderer feature
// is on, a hook is one branch plus the call.
//
// Still to hook (mapped or not yet found): SetStreamSource (the setters write
// fetch constants computed from the stream), TextureUnlock, FlushCpuRange,
// SetPredication (candidate 82DB0D38 turned out to be the device Release),
// QueryIssue and QueryGetData (the query machinery at +13600/+13720).
// Missing hooks show up in the trace as calls or draw packets "written
// outside any hooked call" - never add an address that is not a function
// entry.

#include <native_x_renderer/hooks.h>

// Draws. DrawIndexedVertices (285 ins) and DrawVertices (261 ins) load all
// five dirty masks and run the flush chain (sub_82DB5400, sub_82DB5790,
// sub_82DB68C0's program flush) before their DRAW_INDX; the engine calls them
// from its submission functions (0x82E44xxx, 0x8284F658, 0x82B09220).
NATIVE_X_RENDERER_HOOK(82DA7E60, DrawIndexedVertices)
NATIVE_X_RENDERER_HOOK(82DA7A48, DrawVertices)
// DrawVerticesUP: BeginVertices, then a copy of vertexCount*stride bytes into
// the ring and the inline end commit. The engine calls it from 0x824AF198 and
// 0x83219E70.
NATIVE_X_RENDERER_HOOK(82DA7420, DrawVerticesUP)
// The inline draw bracket: BeginVertices (dev, prim, vertexCount, stride -
// it writes the fetch stride/4 byte at +12880 and saves the packet end at
// +13828), EndVertices (three instructions: the saved end into +48). The
// engine calls BeginVertices itself (0x8321FB88), like GRAW.
NATIVE_X_RENDERER_HOOK(82DA6F60, BeginVertices)
NATIVE_X_RENDERER_HOOK(82DA6F50, EndVertices)
// BeginIndexedVertices: the 40-ins entry at 0x82DA79A8 shuffles its 8+
// arguments into the 336-ins body at 0x82DA7468. The engine calls the entry
// from 0x8321FAB0.
NATIVE_X_RENDERER_HOOK(82DA79A8, BeginIndexedVertices)

// Targets. SetRenderTarget copies the surface header into RB_COLOR_INFO
// (+10372 of the shadow); SetDepthStencilSurface into RB_DEPTH_INFO and
// RB_DEPTHCONTROL. Both read the render target pointers at +12792.
NATIVE_X_RENDERER_HOOK(82DAB320, SetRenderTarget)
NATIVE_X_RENDERER_HOOK(82DAB6B0, SetDepthStencilSurface)
// SetViewport reads the D3DVIEWPORT (int x, y, w, h + float min/max z from
// r4) and stores the device's float viewport at +13000 through 82DAAF88;
// SetScissorRect is the library-internal apply (GRAW's case) that writes the
// scissor quad at +13028 and the PA_SC scissor registers, called from the
// engine's setter (0x82E254D8) as well as from the clears and tiling.
NATIVE_X_RENDERER_HOOK(82DAB1F0, SetViewport)
NATIVE_X_RENDERER_HOOK(82DAA888, SetScissorRect)
// Clear (count, rects, flags, D3DCOLOR, Z, stencil) and ClearF (flags, rect,
// float4*, Z, stencil - f1 carries Z): both save the clear into the +13532
// block and drive the library's own clear draws (sub_82DAF7E0's IM_LOAD_IMM +
// DRAW_INDX_2). One engine caller each (0x82E29630, 0x82E29520).
NATIVE_X_RENDERER_HOOK(82DACA48, Clear)
NATIVE_X_RENDERER_HOOK(82DAC3E0, ClearF)
// The predicated tiling bracket: BeginTiling (flags, count, tileRects, ...)
// saves the targets and writes the bin masks (SET_BIN_MASK_LO/HI); EndTiling
// (resolveFlags, ...) ends it. Twenty engine call sites each, always paired.
NATIVE_X_RENDERER_HOOK(82DAD210, BeginTiling)
NATIVE_X_RENDERER_HOOK(82DAC270, EndTiling)
// The z-pass bracket that goes with tiling (they test the flag byte at
// +11068 bit 26 and run through the query machinery at +13600/+13720).
NATIVE_X_RENDERER_HOOK(82DAC750, BeginZPass)
NATIVE_X_RENDERER_HOOK(82DACF00, EndZPass)
// Resolve: the engine's post-processing calls it eleven times over (0x82E24DF8,
// 0x82E29630, 0x82E29CB0, 0x8321B810, 0x8321F820); it drives the RB_COPY_*
// writer (sub_82DC0708) and the clear-of-color/depth helpers.
NATIVE_X_RENDERER_HOOK(82DC1860, Resolve)

// Shaders and constants. The setters write the object pointer next to the
// shaders (+13044 pixel, +13048 vertex) or the declaration (+12216) and mark
// their dirty masks; the constant setters (called from the engine's
// submission at 0x83219B48 and 0x8321AB30) write the +0 and +8 masks.
NATIVE_X_RENDERER_HOOK(82DB1F20, SetVertexShader)
NATIVE_X_RENDERER_HOOK(82DB1D18, SetPixelShader)
NATIVE_X_RENDERER_HOOK(82DB2138, SetVertexDeclaration)
NATIVE_X_RENDERER_HOOK(82DB1B30, SetVertexShaderConstantF)
NATIVE_X_RENDERER_HOOK(82DB1C08, SetPixelShaderConstantF)
// SetShaderGPRAllocation (flags, vertexGprs, pixelGprs): writes +24376 and
// runs the prebuilt one-point draws (sub_82DC3178/82DC3690's allocator at
// +24384/+24388).
NATIVE_X_RENDERER_HOOK(82DB11C0, SetShaderGPRAllocation)

// Resources. SetTexture copies the texture's fetch constant (object +28) into
// the sampler's register; SetIndices writes +12788 and stamps the fence
// (from +11036) into the unbound buffer's +8, the resource-fence pattern of
// the table. The Lock/Unlock pairs wait on the buffer's fence (object +24/+28
// its base and size) and flush its CPU range: the pairs are certain by their
// matching call-site sets (17 and 12); which pair is the vertex one is to
// confirm in the trace.
NATIVE_X_RENDERER_HOOK(82DABA20, SetTexture)
NATIVE_X_RENDERER_HOOK(82DAAB08, SetIndices)
NATIVE_X_RENDERER_HOOK(82DB0828, VertexBufferLock)
NATIVE_X_RENDERER_HOOK(82DAF710, VertexBufferUnlock)
NATIVE_X_RENDERER_HOOK(82DB0878, IndexBufferLock)
NATIVE_X_RENDERER_HOOK(82DAF7D0, IndexBufferUnlock)

// Command buffer segments. Kick (sub_82DB47C8, 65 ins) is what every packet
// writer calls when the write pointer passes the limit: the observer follows
// a call's packets into the new segment through it. NewSegment writes the
// 1480-dword segment header, stamps the fence and spins on +11152.
NATIVE_X_RENDERER_HOOK(82DB47C8, KickSegment)
NATIVE_X_RENDERER_HOOK(82DB4A80, NewSegment)

// Frame. Swap ends the vblank wait (our SgPresentationInterval frame-rate
// hook lives inside it, at 0x82DC44E0) and Present runs the copy draw
// (sub_82DC3D60 writes its pixel constants to +6016) and the swap counters -
// Present ends the frame.
NATIVE_X_RENDERER_HOOK(82DC43F0, Swap)
NATIVE_X_RENDERER_HOOK(82DC48B0, Present)
