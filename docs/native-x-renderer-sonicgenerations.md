# NativeXRenderer for Sonic Generations (Xbox 360)

The D3D map of Sonic Generations (title id 53450848), for the NativeXRenderer
integration (`native_x_renderer/` submodule). The game's D3D library is
statically linked at `0x82DA0000`–`0x82DC9600` (722 functions; PM4 scan and
per-function summaries in the analysis workspace), with the draws inside the
library, unlike POTCAWE. The device layout is the **Dante's Inferno / GHMetal
family** (register shadow at `+10368`, fetch constants at `+1152`), not
POTCAWE's 5426 form: everything up to the end of the register shadow is
Dante's Inferno's, the block of current objects after it is this game's own.

Method: `tools/ppc_index.py` (function/instruction index of the generated
C++), `tools/pm4_consts.py` (PM4 packet headers), `tools/fn_summary.py`
(per-function device-field access), `tools/rs_setters.py` (render-state
setters named by the shadow registers they write), call-site census against
the engine at `0x82E2xxxx`/`0x82E4xxxx`/`0x8321xxxx`.

## Entry points

Hooked in `src/native_x_renderer_sonicgenerations.cpp`. Evidence in brackets.

| Group | Function | Address | Evidence |
|---|---|---|---|
| Draw | DrawIndexedVertices | `82DA7E60` | 285 ins; 5 args (r4-r7 = prim, baseVertex, startIndex, indexCount); loads all five dirty masks (ld +0/+8/+16/+24/+32) and runs the flush chain before DRAW_INDX; 24 engine call sites (`0x82E44xxx`, `0x8284F658`, `0x824C5A38`) |
| Draw | DrawVertices | `82DA7A48` | 261 ins; 4 args (prim, startVertex, vertexCount); same flush head; engine `0x82B09220` |
| Draw | DrawVerticesUP | `82DA7420` | 18 ins wrapper: BeginVertices(dev, prim, vertexCount, stride) with stride from r7, then copies vertexCount*stride bytes (r6 = data) and commits the inline end; engine `0x824AF198`, `0x83219E70`, `0x8321FA68` |
| Draw | BeginVertices | `82DA6F60` | 303 ins; (dev, prim, vertexCount, stride): writes the fetch stride/4 byte at `+12880`, saves the packet end at `+13828`; the engine calls it itself (`0x8321FB88`) - like GRAW, hook EndVertices too |
| Draw | EndVertices | `82DA6F50` | 3 ins: `lwz r11,13828(r3); stw r11,48(r3); blr` - the saved end into the write pointer. Called from Present and the engine (`0x8321FBD0`) |
| Draw | BeginIndexedVertices | `82DA79A8` | 40-ins entry shuffling 8+ args into the 336-ins body at `82DA7468` (indexFormat in r8, tested with `rlwinm`); engine `0x8321FAB0` |
| Targets | SetRenderTarget | `82DAB320` | 228 ins; writes the RB_COLOR_INFO shadow (+10372) and RB_COLOR_MASK from the surface; reads the target pointers at `+12792`; engine `0x82E251B0` |
| Targets | SetDepthStencilSurface | `82DAB6B0` | 182 ins; writes RB_DEPTH_INFO (+10440), RB_DEPTHCONTROL (+10548), RB_SURFACE_INFO (+10368); engine `0x8321B810`, `0x82E27A00` |
| Targets | Clear | `82DACA48` | 302 ins; (count, rects, flags, D3DCOLOR, Z, stencil) with Z in f1; saves the clear into the `+13532` block; engine `0x82E29630` |
| Targets | ClearF | `82DAC3E0` | 219 ins; (flags, rect, float4*, Z, stencil), Z in f1, rect pointer saved at `+13100`; writes RB_COLOR_CLEAR-family state; engine `0x82E29520` |
| Targets | BeginTiling | `82DAD210` | 104 ins; (flags, count, tileRects): tests r4 and r5, writes the bin masks (SET_BIN_MASK_LO/HI) and the tile state; 20 call sites, always paired with EndTiling |
| Targets | EndTiling | `82DAC270` | 92 ins; (resolveFlags, ...): tests r4 == 0, clears `+13068`, tests the flag byte at `+11068` bits 26-29; same 20 sites |
| Targets | BeginZPass | `82DAC750` | 139 ins; sets `+13072`, tests `+11068` bit 26, runs the query machinery at `+13744`; engine `0x82E25F60` |
| Targets | EndZPass | `82DACF00` | 164 ins; reads the fence at `+11024`, flushes the `+13600`/`+13720` blocks; engine `0x82E26000` |
| Targets | Resolve | `82DC1860` | 19-ins entry over the 1109-ins RB_COPY_* writer (`82DC0708`); 11 engine call sites (`0x82E24DF8`, `0x82E29630`, `0x82E29CB0`, `0x8321B810`, `0x8321F820`) |
| Targets | SetViewport | `82DAB1F0` | 32 ins; reads the D3DVIEWPORT from r4 (int x, y, w, h + float min/max z), converts to the float viewport at `+13000` through `82DAAF88`; engine `0x82E27350`, `0x82E254D8` |
| Targets | SetScissorRect | `82DAA888` | 62 ins; the library-internal apply (GRAW's case): writes the scissor quad at `+13028` and the PA_SC scissor registers (`0x2011`/`0x2012`); the engine calls it from `0x82E254D8`, and the clears/tiling/Present copy internally |
| Shaders | SetVertexShader | `82DB1F20` | 115 ins; writes the shader pointer at `+13048` and the +0/+16/+32 masks; the program flush reads it back (`+13048`) |
| Shaders | SetPixelShader | `82DB1D18` | 111 ins; writes `+13044` and the +8/+16/+32 masks; the program flush reads it back |
| Shaders | SetVertexDeclaration | `82DB2138` | 5 ins; writes `+12216` and the +16 mask |
| Constants | SetVertexShaderConstantF | `82DB1B30` | 54 ins; writes the +0 mask (VS constants dirty); engine `0x83219B48` |
| Constants | SetPixelShaderConstantF | `82DB1C08` | 54 ins; writes the +8 mask; engine `0x8321AB30` |
| Misc | SetShaderGPRAllocation | `82DB11C0` | 53 ins; writes `+24376` and runs the prebuilt one-point draws (`82DC3178`/`82DC3690`, allocator `+24384`/`+24388`) |
| Resources | SetTexture | `82DABA20` | 57 ins; (sampler, texture): copies the texture's fetch constant (object `+28`..`+52`) into the sampler's register, kicks if needed; engine `0x82E27350`, `0x82E254D8` |
| Resources | SetIndices | `82DAAB08` | 36 ins; writes the index buffer pointer at `+12788` and stamps the fence (from the block `+11036` points to) into the unbound buffer's `+8`; 10 engine call sites |
| Resources | Vertex buffer Lock / Unlock | `82DB0828` / `82DAF710` | Lock: 20 ins, waits on the buffer's fence through `82DB04B8` (base/size at object `+24`/`+28`); Unlock: 4 ins tail-call flushing the CPU range (`82DAF300`); 17 call sites each |
| Resources | Index buffer Lock / Unlock | `82DB0878` / `82DAF7D0` | The same pair with the other fence command (12 call sites each); which pair is the vertex one is to confirm in the trace |
| Sync | KickSegment | `82DB47C8` | 65 ins; what every packet writer calls when the write pointer passes the limit (lwz +56 / lwz +48); stamps the segment from `+11024` |
| Sync | NewSegment | `82DB4A80` | 30 ins; writes the 1480-dword segment header, stamps the fence (through `82DB3388`) and spins on `+11152` |
| Frame | Swap | `82DC43F0` | 120 ins; converts the present interval at `+13984` to a vblank count (the SgPresentationInterval 60 FPS hook sits inside it at `0x82DC44E0`), reads `+12260`, drives the swap |
| Frame | Present | `82DC48B0` | 474 ins; runs the copy draw (`82DC3D60`, its pixel constants at `+6016`) and the swap counters; ends the frame (kFrameEnd); engine `0x82E29CB0` |

Not hooked yet: SetStreamSource (the engine calls the fetch-patch pair
`82DA9E60`/`82DAA008` - 3-arg helpers that set the stride/format of the
stream's fetch constant `(stream+48)*24` and dirty its bit, reading the
per-stream byte tables at `+10864`/`+10890`; where the buffer address enters
the fetch is to find, so the hook waits for the trace), TextureUnlock,
FlushCpuRange, QueryIssue and QueryGetData (the query machinery at
`+13600`/`+13720`), and the samplers' render-state table (~90 setters at
`0x82DA85C0`–`0x82DA9E10`, named by rs_setters: RB_BLEND*, RB_DEPTHCONTROL,
RB_COLOR_MASK, PA_SU_*, SQ_WRAPPING_*, ... - the observer reads the shadow
they write, so they need no hooks). `82DB0D38`, a SetPredication candidate,
turned out to be the device's Release (atomic decrement of the refcount at
`+60`). A missing hook shows up in the trace as a call or packets "written
outside any hooked call".

## The device layout

Every offset below was derived from the code that uses it (the draw's flush,
the setters, the tiling/clear/swap paths). The head matches Dante's Inferno
(TU2) exactly; the tail is this game's own spacing.

| Field | Offset | Evidence |
|---|---|---|
| Dirty masks (u64 x 6) | `+0`…`+40` | The draw flushes load +0/+8/+16/+24/+32 (+40 in the clears); VS in +0, PS in +8 (the constant setters), fetch in +24 (the fourth, POTCAWE's order) |
| Fetch dirty bit 0 | bit 31 | The fetch flush (`82DA9E60` family): `addi r4,r4,32` then `srd` from bit 63 - the stream's slot is its fetch constant (`(stream + 48) * 24 = +1152 + stream * 24`) and slot s is bit 31-s |
| Command buffer write / limit / kick | `+48` / `+52` / `+56` | Every packet writer: `lwz +56`, `lwz +48`, `stwu`, `stw +48`. The write pointer holds the **last dword written** (stwu pre-increment) |
| Fetch constants | `+1152` | The Present copy writes a fetch constant at `+1152` (stw); Dante's offset |
| VS float constants | `+1920` | Dante's offset |
| PS float constants | `+6016` | The Present copy draws stfs+6016..+6044 (its c0-c6); Dante's offset |
| Bool/loop constants | `+10112` | Dante's offset (not yet seen in a setter - confirm) |
| Register shadow 0x2000 | `+10368`, 19 regs | rs_setters decodes the writers to the right names |
| Register shadow 0x2100 | `+10444`, 21 regs | |
| Register shadow 0x2180 | `+10528`, 5 regs | SQ_PROGRAM_CNTL (0x2180) + SQ_CONTEXT_MISC written by the program flush (`82DB68C0`) |
| Register shadow 0x2200 | `+10548`, 12 regs | RB_DEPTHCONTROL (0x2200), RB_BLEND* (0x2201/2209/220A/220B) |
| Register shadow 0x2280 | `+10596`, 21 regs | |
| Register shadow 0x2300 | `+10680`, 38 regs | PA_SC_LINE_CNTL (0x2300) |
| Register shadow 0x2380 | `+10832`, 8 regs | PA_SU_POLY_OFFSET_* (0x2380-2383) |
| Fence counter | `+11024` | The kick (`82DB47C8`) and the z-pass paths read it |
| Write-back block | `+11036` | Read as the pair +11036/+11040 in the fence checks ([+0] = last fence the GPU reached) |
| Flags (u32, flag bytes) | `+11068` | +11068 tiling (bits 26-29 tested by the tiling/z-pass pair), +11070 inline draw, +11071 program flush, +11072 predication |
| Vertex declaration | `+12216` | SetVertexDeclaration writes it; the program flush reads it |
| Index buffer | `+12788` | Written next to the targets (POTCAWE's adjacency pattern); SetIndices candidate writes it |
| Render targets RT0-3 | `+12792`…`+12804` | Read interleaved with their companions at `+13080`… by the tiling/resolve/clear paths |
| Depth stencil | `+12808` | Same reads |
| Viewport | `+13000` | The draw flushes read lfs+13000..+13012 (x, y, w, h floats); +13016/+13020 = min/max z; SetViewport writes them through a computed store |
| Scissor | `+13028`…`+13040` | SetScissorRect's apply writes the TL/BR quad (4 dwords); the draws read +13028..+13040 with the shaders |
| Pixel shader | `+13044` | SetPixelShader writes it; the program flush reads it |
| Vertex shader | `+13048` | SetVertexShader writes it; the program flush reads it |
| Predication mask | `+13052` | Next to the shaders, written with the predication flag (confirm with SetPredication) |
| Tile count / tile rects | `+11060` / `+11064` | Written by BeginTiling/EndTiling/ClearF (confirm against the trace) |
| Inline draw end | `+13828` | BeginVertices saves the packet end here; EndVertices copies it into +48 |
| D3DRS_PRESENTINTERVAL | `+13984` | Swap converts it to a vblank count (the 60 FPS hook at `0x82DC44E0`) |
| Swap/vblank state | `+12260`, `+13912`/`+13916`, `+15300`… | Swap/Present counters |
| Prebuilt draws | `+24376`, `+24384`/`+24388`/`+24392` | SetShaderGPRAllocation's one-point draws |
| Per-stream fetch stride/format bytes | `+10864 + stream`, `+10890 + stream` | The fetch-patch pair reads them (`82DA9E60` family) |

Still to place: the vertex-buffer array, the texture object array and the
sampler filter state (the setters write fetch constants computed from the
stream/sampler, so the arrays first show up in the trace's argument dumps).

## The resource objects

| Field | Offset | Evidence |
|---|---|---|
| Texture fetch constant | `+28` | The setters read +28.. of the texture (POTCAWE's offset) |
| Surface RB_SURFACE_INFO / RB_COLOR-DEPTH_INFO | `+24` / `+28` | The GetDesc-style getters read +24/+28 (`82DA6C98` and friends) |
| Surface size (w-1, h-1) | `+36`, bits 18-31 / 3-17 | POTCAWE's packing |
| Vertex/index buffer base, size | `+24` / `+28` | The same getters |
| Resource fence (stamped when unbound) | `+8` | SetIndices stamps it into the old buffer with the fence from the `+11036` block |

## The frame's structure (from the code)

Present (`82DC48B0`) runs the copy draw with its own pixel constants (`+6016`)
into the front buffer, then the swap counters; Swap (`82DC43F0`) waits the
vblanks of the present interval (`+13984`, hooked at `0x82DC44E0` for 60 FPS).
The tiling bracket (BeginTiling/EndTiling) is used from 20 engine sites and
z-pass from two more; the engine resolves eleven times over through Resolve.
The scene resolve for the antialiasing (`scene_resolve_caller`) is to be named
from the trace's "from" lines (see below).

## What the game side has

- `src/native_x_renderer_sonicgenerations.cpp` - the hooks above.
- `src/native_x_renderer_profile.h/.cpp` - `sonicgenerations::NativeXRendererProfile()`
  with the DeviceLayout/ObjectLayout above.
- `SonicGenerationsApp::InitNativeXRenderer()` in `src/sonicgenerations_app.h`,
  called from every platform's `OnPostSetup` before the guest starts.
- `native_x_renderer` defaults to **true** in `SetDefaults()` (the user wants
  the native renderer); `--native_x_renderer=false` forces the Xenos path, and
  the Xenos plugin draws if the native renderer cannot start.
- Build: the `native_x_renderer/` submodule is added after the SDK in both
  `ios/sonicgenerations/CMakeLists.txt` and
  `android/app/src/main/cpp/CMakeLists.txt`; `ios/build.sh`, `android/build.sh`
  and the CI checkout fetch its DiligentCore recursively.

## Verifying (step 3 of integrating-a-game.md)

With the game in hand (`bash android/build.sh <default.xex>` or
`bash ios/build.sh <default.xex>`):

1. `--native_x_renderer_trace=true`, F9 for a frame dump,
   `python native_x_renderer/tools/frame_trace.py <log>`: check the call
   counts per frame, that every entry above fires with sensible arguments, the
   tiling brackets and the resolves; name `scene_resolve_caller` from the
   resolve of the finished scene (after post-processing, before the HUD).
2. `--native_x_renderer_verify=true` (F10): one frame against the SDK's
   command processor, until 100 % of the draws match in several scenes. The
   observer's "written outside any hooked call" names the missing hooks above.
3. `--native_x_renderer=true`: first frames, compared with the Xenos path.

## Todo

- [ ] The remaining hooks: SetStreamSource, TextureUnlock, FlushCpuRange,
      QueryIssue, QueryGetData (and confirm the Lock/Unlock pair assignment).
- [ ] The vertex-buffer array and the sampler filter state; the exact tile
      count/rects and predication-mask offsets (marked confirm above).
- [ ] The XDK version from the XEX (recomp-tools' xdkver.mjs) into the profile.
- [ ] The scene resolve caller for the antialiasing; the aspect-ratio and
      DLAA hooks (step 4 of the checklist) once the trace is clean.
- [ ] Parity play test, then the launcher row; the performance A/B.
