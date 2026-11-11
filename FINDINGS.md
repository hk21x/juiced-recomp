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
