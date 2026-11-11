# Juiced recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 1 - boots: 100 s headless without a trap; the warning screen draws correctly in the reference renderer; the Bink movies play but show black
- XDK: 5849 with D3D8LTCG, 157 of 278 functions, 6 of 13 data (first scan); 20 LTCG argument maps proven (config/juiced.toml)
- Push buffers: none created or replayed; game code writes NV2A commands into the device's push buffer itself
- GitHub: private hk21x/juiced-recomp; SDK on fork branch `juiced` (058f3a3 = fork main 26f372b + SetViewport(NULL); not yet on fork main)

## Log

### 2026-11-11 - The warning screen draws

Three missing names made every frame black, found from the reference renderer's draw lists (CAP, REF_DRAWLOG):
- D3DDevice_SetRenderState_Simple 0x2A9B60 (ecx header, edx value, 414 callers): without it no simple state (colour
  mask, blend, depth function) reached the renderer, so every draw had a colour mask of 0.
- SelectVertexShaderDirect 0x2AF740: the game draws everything with its own vertex programs and direct attribute
  formats; without it the renderer had no vertex declaration.
- The complex render state setters, all named from the library's table at 0x2B86A8 (state * 4; the scan's finds
  agree with it). TextureFactor 0x2A9530 was the one that mattered: the font combines texture with TFACTOR, which was 0,
  so the alpha test dropped every glyph.

SDK: a carry-over of the last vertex's attributes into the registers (xemu does this: pgraph_update_inline_value) was
tried and reverted; nothing here needs it yet (FINDINGS).

Evidence: scripts/harness.sh 16 REF=301: the WARNING screen with the Juice Games and THQ logos and all three lines of
text (captures/1111-2129-ref_00301.png).

Next: the Bink movies (a 640x480 YUY2 texture, format 0x24) draw black; then the title screen after the intro.

### 2026-11-11 - Boots through the logo movies headless; frames black

config/juiced.toml: every [abi] map the scan asked for, each proven from the function body (CDevice_KickOff edx;
DrawVertices stack/ecx/eax; SetStreamSource eax/edi/ebx; SetTexture ecx/eax; SetTransform eax/edx; Swap eax; ...).
Direct3D_CreateDevice gets only flags (eax), parameters (stack) and the out pointer (ecx): LTCG dropped the first three,
which the host never reads, so edx/ebx/esi stand in for them. The scan's D3DResource_IsBusy is BlockOnResource (as in
True Crime). Named by hand: SetTile 0x2AD8D0 (it crashed programming the GPU's tile registers), InitDefaultState 0x2AA7F0,
SelectVertexShaderDirect 0x2AF740 (ecx, eax), BinkWait 0x246290, BinkNextFrame 0x246D70 (from the game's movie step
0x103550; the SDK's Bink pacing called the unnamed BinkWait). Data: DirtyFlags, RenderState, IndexData, Streams,
DeferredTextureState, FvfShader (0x2AF990, called by game code, reads it from the device), and the simple render state
table 0x3108E0, found by True Crime's table contents and confirmed by its two readers.

SDK (branch `juiced`, bb0e1c0): SetViewport(NULL) keeps the viewport and clears the scissors, as the library's 0x2AD512
does; Juiced's render state setter 0x2A9900 passes NULL (the host read guest address 0).

Evidence: scripts/harness.sh 120: 7080 frames, no trap; THQ.bik, juicegames.bik and JuicedCGI_NTSC.bik open in turn.
A movie frame (CAP=1500) has 5 draws: two plain quads, the movie (640x480 YUY2) and a logo into a 640x960 target
0x01460000, then one triangle copies it into 0x03D86000 at 640x480. All use the game's own vertex programs through
SelectVertexShaderDirect.

Next: why the reference renderer shows black (REF_DRAWLOG wrote an empty list at frame 1499): the programs, their
constants (pushed by the game), or which surface is shown.

### 2026-11-11 - Repository set up

Disc: USA/Europe, title 545100EF, default.xbe SHA-1 b38b935794b05e46b70b164acdcca9338f0b692f, built 2005-03-25 as
d:\Src\game\mainGame\Xbox_Release\mainGame.exe. Libraries all 1.0.5849: XAPILIB XBOXKRNL LIBCMT D3D8LTCG D3DX8
XGRAPHCL DSOUND XACTENLT XONLINES XVOICE VOICMAIL. Code sections include BINK*, WMADEC, XACTENG, XONLINE, XNET.

Scan (fork main 26f372b): 157 of 278 functions, 6 of 13 data. The scan placed 17 LTCG D3D functions by constants
whose arguments are in registers; each needs its [abi] proven. Misses: about 70 D3D functions, the D3D data, Bink's
GetSummary/NextFrame/Wait (a 2002 Bink, BinkDoFrame matched only at 0.69), some DSOUND setters; SetFilter and SetEG
ambiguous.

Push buffers (PushBufferScan): no push buffer resource is created (no 0x20001 tag); the 0x20000 tests are the
inlined resource Register. Game code writes commands into the device push buffer through an inlined MakeSpace: push
pointer 0x2BC378, D3D_MakeRequestedSpace 0x2AE740 when short; 0x1575C0 writes SET_BEGIN_END, an INLINE_ARRAY quad,
End. About 50 such stores in .text (0x121240, 0x12A2F0, 0x1575C0-0x157C70, 0x1C8140, ...) and Bink's blitters
(0x244A30, 0x24CBE0-0x24D1C0).

Next: prove the [abi] maps, name the D3D misses, first boot.
