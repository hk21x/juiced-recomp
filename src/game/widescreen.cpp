// Widescreen: what the SDK's 2D squeeze needs from the game's side.
//
// The game's 3D is 16:9 in the console's widescreen mode (and wider here: src/game/fov.cpp); its 2D is laid out for
// 4:3 in both modes and shown a third wider. GameInfo::hud_aspect 4:3 and squeeze_flat_programs have the renderers
// squeeze each 2D draw into the frame's centred 4:3 part (corners on the frame's edges stay: backdrops, fades and the
// frame's own copy fill it). Two things they cannot tell:
// - A pass with a viewport of its own (0x12DA30 sets each pass's at 0x12DB14: the race's map, 112 wide at 56,324; a
//   car in a menu's panel) is a small 3D scene placed in the 2D. Its viewport is moved and narrowed as the 2D around it
//   is, and what is drawn in it is left as drawn (gpu::State::no_squeeze): the scene keeps its shape.
// - The speed blur's passes (src/game/blur.cpp) are full screen effects with corners inside the frame.
// - The movies are 4:3 pictures: see PassViewport.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

#include "app/app.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/d3d8.h"
#include "xdk/hle.h"

namespace juiced {
// what 2D is narrowed by, about the frame's centre: 1 when the frame is 4:3
float Squeeze() {
    const float frame = app::FrameAspect(), hud = app::HudAspect();
    return frame > hud + 1e-3f ? hud / frame : 1.0f;
}
}  // namespace juiced

namespace juiced {
extern std::atomic<bool> intro_movie;  // the last movie opened is one of the boot's (src/common/game_info.cpp)
}

// at 0x12DB14, the pass's D3DDevice_SetViewport: [esp] its D3DVIEWPORT8 (x, y, width, height, min z, max z)
void PassViewport(X86Context& ctx, uint8_t* /*base*/) {
    const uint32_t vp = kernel::R32(ctx.esp);
    const uint32_t x = kernel::R32(vp), w = kernel::R32(vp + 8);
    const float k = juiced::Squeeze();
    const bool inset = k < 1 && w && w < 600 && x + w <= 640;
    d3d::State().no_squeeze = inset;
    // A movie drawn across the frame (Bink's YUY2 texture, 640x480 for 4:3): the boot's logos and intro keep their
    // shape with bars at the sides; the menus' backdrops fill the frame's width at their shape, cropped above and
    // below, as PGR2's do (gpu::State::pillar_aspect, pillar_fill). A movie in a panel does not span the frame.
    d3d::State().pillar_aspect = 4.0f / 3.0f;
    d3d::State().pillar_format = 0x24;  // D3DFMT_YUY2
    d3d::State().pillar_fill = !juiced::intro_movie.load();
    if (!inset) return;
    static const uint32_t scratch = mem::AllocSystem(24);
    std::memcpy(kernel::HostPtr(scratch), kernel::HostPtr(vp), 24);
    const float left = 320 + (float(x) - 320) * k, right = 320 + (float(x + w) - 320) * k;
    kernel::W32(scratch, uint32_t(left + 0.5f));
    kernel::W32(scratch + 8, uint32_t(right + 0.5f) - uint32_t(left + 0.5f));
    kernel::W32(ctx.esp, scratch);
}

// at the 2D quads' D3DDevice_DrawVerticesUP calls (config/juiced.toml): the vertices' x, y are in 640x480.
// The menus' header and footer bands are two tiles, 0 to 320 and 320 to 640: squeezed with the rest of the 2D they
// end at the 4:3 area's sides with the backdrop beyond. A textured quad from one edge of the frame to its middle is
// left unsqueezed instead, so the band runs the frame's whole width (its picture, streaks of colour, wider).
namespace {
bool g_band_was = false, g_band = false;
}
void BandQuad(X86Context& ctx, uint8_t* /*base*/) {
    g_band = false;
    if (juiced::Squeeze() >= 1 || d3d::State().no_squeeze) return;
    // (primitive, vertices, their address, the bytes of one)
    const uint32_t v = kernel::R32(ctx.esp + 8), stride = kernel::R32(ctx.esp + 12);
    if (kernel::R32(ctx.esp) != 6 || kernel::R32(ctx.esp + 4) != 4 || stride < 8 || stride > 64) return;
    float lo = 1e9f, hi = -1e9f;
    for (uint32_t i = 0; i < 4; i++) {
        float x;
        std::memcpy(&x, kernel::HostPtr(v + i * stride), 4);
        lo = std::min(lo, x), hi = std::max(hi, x);
    }
    // (the tiles overlap a little and move: 0 to 323.2, 320 to 643.2 in the main menu)
    const bool left = lo <= 0.5f && lo >= -6 && std::fabs(hi - 320) <= 6;
    const bool right = hi >= 639.5f && hi <= 646 && std::fabs(lo - 320) <= 6;
    if (!left && !right) return;
    g_band = true;
    g_band_was = d3d::State().no_squeeze;
    d3d::State().no_squeeze = true;
}

// after that draw
void BandQuadDone(X86Context& /*ctx*/, uint8_t* /*base*/) {
    if (g_band) d3d::State().no_squeeze = g_band_was;
    g_band = false;
}
