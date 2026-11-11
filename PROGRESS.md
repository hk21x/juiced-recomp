# Juiced recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 0 - set up
- XDK: 5849 with D3D8LTCG, 157 of 278 functions, 6 of 13 data (first scan); 17 LTCG argument maps to prove
- Push buffers: none created or replayed; game code writes NV2A commands into the device's push buffer itself
- GitHub: private hk21x/juiced-recomp; SDK at fork main 26f372b

## Log

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
