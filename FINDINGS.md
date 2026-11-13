# Juiced recomp - findings

What the work has established, by topic. PROGRESS.md has the dated log with the evidence for each item. Each item
is marked **[console]** (how the Xbox, its XDK or the runtime behaves: the fix is in the SDK) or **[game]** (this
game's own code and data, with its addresses).

## The image

## Rendering
- **[game]** Every draw uses the game's own vertex programs, selected through SelectVertexShaderDirect (0x2AF740) with
  direct attribute formats; the scene goes to a 640x960 target (0x01460000) that one triangle copies into the 640x480
  back buffer.
- **[game]** Fonts are P8 textures with alpha ramp palettes, combined MODULATE(texture, TFACTOR).
- **[console]** After a draw from vertex arrays the NV2A's attribute registers keep the last vertex's values; a later
  draw whose format leaves a register without a stream reads them (xemu: pgraph_gl_bind_vertex_attributes calls
  pgraph_update_inline_value with the provoking element: draw arrays the highest vertex, indexed the last index, inline
  array the last vertex). The runtime does not do this; tried on SDK branch `juiced` (d0ae136) and reverted as unneeded
  so far. Some of Juiced's programs read v1 without a stream (vattr[1]), so revisit if colours come out wrong.
- **[game]** Widescreen is native: with the console's widescreen flag (0x372EE4, from XGetVideoFlags at 0x12AA8C) the
  camera's field of view setter 0x166A20 multiplies the horizontal angle by 1.333 (0x313634) and the derived vertical
  one by 1.15 (0x3317B4), and the device gets D3DPRESENTFLAG_WIDESCREEN. The 2D (HUD, menus, movies) is laid out for
  4:3 and is the same in both modes: at 16:9 it is a third wider, as on the console with a widescreen TV.
- **[game]** The 2D is drawn by vertex programs taking a scale and offset (c22 = 2 / viewport width, -2 / viewport
  height; c23 = -1, 1) pushed raw for each viewport: not pretransformed, not a pass-through or matrix program, so the
  SDK's 2D squeeze (gpu::ScreenDraw) does not see it.
- **[console]** Car bodies and windows reflect a 128x128 cube map (0x83301F80, six swizzled X8R8G8B8 faces) that the
  game draws a few faces a frame into memory it never clears. A reference frame drawn on its own (REF=N) shows the
  faces not drawn that frame as coloured noise; eight frames in a row (SNAP_FRAMES=8) show the reflection. Not a bug.

## Sound
- **[game]** Engines and effects are the game's own DirectSound buffers (Xbox ADPCM), 3D head relative with rolloff 0
  (the listener's and each buffer's: the game sets levels itself); music and speech go through XACT (WMA music).
- **[console]** In a race the mixed output is about 15 dB above the menus' (-10 dB RMS against -25) and clips a little
  (about 200 samples a second): three or four voices each near full level (gain 0.47) plus the music. XACT sets every
  mix bin's headroom to 0 (0x29C744). Not known whether the console sounds the same: to be judged by ear on Windows.
  The race's voices are 3D (headroom 0, about twenty at once); the menus' music is a 2D ADPCM voice and the race's
  music a PCM stream, both with the library's 6 dB of headroom. Setting effects_percent (default 50) scales the 3D
  voices only (GameInfo::voice_3d_level).

## Menus
- First boot: warning, movies, title, a profile is made and saved ("Save Successful"), main menu. Later boots:
  Profile Manager (the saved driver; X deletes), "Load Successful", main menu.
- Main menu: Career, Arcade (series, race), Custom Race (race type, track, time of day, weather, cars: a live fly-over),
  Multiplay (Splitscreen, System Link Play), Xbox Live (sign-in: New Account offers the dashboard), Options (Audio, Game,
  Controls, Xbox Live), Extras (Showoff tutorial videos, Save, Hi-scores, Credits, Cheats).
- Pause: Continue, Restart (asks, NO first), Options, Abandon Race.
