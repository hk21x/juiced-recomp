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
// - The race's HUD (the window "hud_race", Showoff's "hud_cruise": lap and position at the top corners, the times, the map and the dials at
//   the bottom corners) belongs at the frame's sides, not the 4:3 part's: see HudAnchor.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
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

// The race's HUD at the frame's sides. Each of the window's gadgets (pointers at +0x5C to +0x60; a gadget's name at
// +0x38, hidden at +0x58, its rectangle x, y, width, height at +0x8C as fractions of 640x480) draws a quad of its
// rectangle through the 2D quad calls (BandQuad) before its text, needle or meter. A quad matching a gadget in the
// frame's left or right part sets that side as the anchor for the draws that follow, until the next quad that is
// not inside that gadget: they are squeezed as the rest of the 2D is and moved so the 640 wide layout's edge lies on
// the frame's edge (gpu::State::squeeze_beyond, squeeze_shift). Gadgets about the middle (messages, the countdown,
// the split times) stay in the centred part.
namespace juiced {
uint32_t MenuManager();  // src/common/pointer.cpp
bool HudAnchored();      // src/common/pointer.cpp: not switched off (JUICED_HUD_ANCHOR=0, tests)
namespace {
float g_anchor = 0;                  // -1 the left edge, 1 the right, between them in proportion, 0 none
float g_anchor_box[4] = {0, 0, 0, 0};  // the gadget's rectangle: left, top, right, bottom

bool Mapped(uint32_t va, uint32_t bytes) { return va >= 0x10000 && mem::IsCommitted(va) && mem::IsCommitted(va + bytes - 1); }

// which side a quad of x0, y0 to x1, y1 belongs to when it is one of the window's gadgets' rectangles: 2 when none
// matches (-1 the left edge, 1 the right, 0 neither). The race's HUD (hud): by the gadget's middle. The race's other windows (the pause menu and what it opens:
// a dark strip from the left edge, the menu's panel and rows, a bar with the A and B prompts at the bottom right):
// the bar and what lies on it go to the right edge, where the bar ends on a 4:3 frame; the rest stays (the menu in
// the centred part, where the mouse takes it to be).
float GadgetSide(uint32_t window, bool hud, float x0, float y0, float x1, float y1, float box[4]) {
    const uint32_t first = kernel::R32(window + 0x5C), end = kernel::R32(window + 0x60);
    for (uint32_t at = first; first && at < end && at - first < 0x400; at += 4) {
        const uint32_t g = kernel::R32(at);
        if (!Mapped(g, 0x9C) || kernel::R32(g + 0x58)) continue;
        float r[4];
        std::memcpy(r, kernel::HostPtr(g + 0x8C), 16);
        const float gx = r[0] * 640, gy = r[1] * 480, gw = r[2] * 640, gh = r[3] * 480;
        if (std::fabs(gx - x0) > 1.5f || std::fabs(gy - y0) > 1.5f || std::fabs(gx + gw - x1) > 1.5f || std::fabs(gy + gh - y1) > 1.5f)
            continue;
        box[0] = gx, box[1] = gy, box[2] = gx + gw, box[3] = gy + gh;
        const float mid = gx + gw * 0.5f;
        // the rivals' markers (RIVAL0 to RIVAL4) slide along the bottom: across the frame's width as across the
        // 4:3 screen's, not in steps
        if (hud && !std::memcmp(kernel::HostPtr(g + 0x38), "RIVAL", 5)) return std::clamp((mid - 320) / 320, -1.0f, 1.0f);
        if (hud) return mid < 240 ? -1 : mid > 400 ? 1 : 0;
        const uint32_t type = kernel::R32(g + 0xC);  // 2 a picture, 3 a text: nothing the mouse points at
        return (type == 2 || type == 3) && gy >= 400 && gy + gh <= 445 && gx >= 150 && gw < 500 ? 1 : 0;
    }
    return 2;
}

void SetAnchor(float side) {
    // what the gadget before pushed itself (its text) is drawn as it was anchored
    if (side != g_anchor) d3d::Pending();
    g_anchor = side;
    const float k = Squeeze();
    d3d::State().squeeze_beyond = side != 0 && k < 1;
    d3d::State().squeeze_shift = side != 0 && k < 1 ? side * 320.0f * (1 - k) / k : 0.0f;
}
}  // namespace

// the anchor's shift in the 640 wide layout, 0 with none
float AnchorShift() { return d3d::State().squeeze_beyond ? d3d::State().squeeze_shift : 0.0f; }

// a 2D quad from x0, y0 to x1, y1: true when it is one of the HUD's (the anchor set for it and what follows it)

// a 2D quad from x0, y0 to x1, y1: true when it is anchored, or one of the HUD's (the anchor set for it and for what
// follows it)
bool HudAnchor(float x0, float y0, float x1, float y1) {
    if (!HudAnchored() || Squeeze() >= 1) return false;
    // about the gadget last matched (a dial's needle, a meter's bar): the same anchor. By the quad's middle: a needle
    // turns about the dial's and its quad's corners pass the dial's rectangle (it showed beside the dial then)
    const float mx = (x0 + x1) * 0.5f, my = (y0 + y1) * 0.5f;
    if (g_anchor && mx >= g_anchor_box[0] && my >= g_anchor_box[1] && mx <= g_anchor_box[2] && my <= g_anchor_box[3] &&
        x1 - x0 <= g_anchor_box[2] - g_anchor_box[0] + 8 && y1 - y0 <= g_anchor_box[3] - g_anchor_box[1] + 8)
        return true;
    // the HUD, then the windows over it from the one showing down
    const uint32_t m = MenuManager(), gm = kernel::R32(0x372B54);
    const bool race = Mapped(gm, 0x40) && kernel::R32(gm + 0x3C) != 0 && Mapped(m, 0x48);
    const uint32_t first = race ? kernel::R32(m) : 0, end = race ? kernel::R32(m + 4) : 0;
    if (first && end > first && end - first <= 0x40) {
        for (uint32_t at = end; at > first; at -= 4) {
            const uint32_t window = kernel::R32(at - 4);
            if (!Mapped(window, 0x64)) continue;
            const char* name = static_cast<const char*>(kernel::HostPtr(window));
            const bool hud = !std::strcmp(name, "hud_race") || !std::strcmp(name, "hud_cruise");  // a race's, Showoff's
            const float side = GadgetSide(window, hud, x0, y0, x1, y1, g_anchor_box);
            if (side == 2) continue;
            SetAnchor(side);
            return side != 0 || hud;
        }
    }
    if (g_anchor) SetAnchor(0);
    return false;
}

// a frame ends: nothing of the next one is anchored until a HUD quad says so
void EndAnchor() {
    if (g_anchor || d3d::State().squeeze_beyond) SetAnchor(0);
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
    if (inset != d3d::State().no_squeeze) d3d::Pending();
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
    // (a HUD gadget's pass, the map: at the side its gadget is anchored to)
    const float shift = juiced::AnchorShift();
    const float left = 320 + (float(x) + shift - 320) * k, right = 320 + (float(x + w) + shift - 320) * k;
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
    float lo = 1e9f, hi = -1e9f, top = 1e9f, bottom = -1e9f;
    for (uint32_t i = 0; i < 4; i++) {
        float xy[2];
        std::memcpy(xy, kernel::HostPtr(v + i * stride), 8);
        lo = std::min(lo, xy[0]), hi = std::max(hi, xy[0]);
        top = std::min(top, xy[1]), bottom = std::max(bottom, xy[1]);
    }
    if (juiced::HudAnchor(lo, top, hi, bottom)) return;
    // (the tiles overlap a little and move: 0 to 323.2, 320 to 643.2 in the main menu)
    const bool left = lo <= 0.5f && lo >= -6 && std::fabs(hi - 320) <= 6;
    const bool right = hi >= 639.5f && hi <= 646 && std::fabs(lo - 320) <= 6;
    if (!left && !right) return;
    d3d::Pending();
    g_band = true;
    g_band_was = d3d::State().no_squeeze;
    d3d::State().no_squeeze = true;
}

// after that draw
void BandQuadDone(X86Context& /*ctx*/, uint8_t* /*base*/) {
    if (g_band) d3d::State().no_squeeze = g_band_was;
    g_band = false;
}

// the frame's present (0x12B9F0: the two swaps): the HUD's anchor ends with the frame
X86_EXTERN_FUNC(__imp__sub_0012B9F0);
X86_FUNC(sub_0012B9F0) {
    juiced::EndAnchor();
    __imp__sub_0012B9F0(ctx, base);
}
