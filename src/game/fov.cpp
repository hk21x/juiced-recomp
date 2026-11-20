// Field of view (setting fov_percent, 100 the game's): the race camera's.
//
// A camera's field of view is set through its vtable (+0x14: 0x166A20, thiscall: horizontal angle in radians, vertical
// angle or 0 to derive it; ret 8), which stores the angles and rebuilds the projection (vtable +0: 0x15CB10, 1 / tan of
// each half angle). The race's camera is set from four places in its own code (the calls returning to 0x1272B, 0x175B5,
// 0x177DE and 0x17DF0; found by scaling every camera and logging the callers: the angle follows the speed), every
// frame, so a change applies at once. Here the tangent of that half angle is scaled, which widens the picture by the
// setting's percentage; the vertical angle follows from it as the game derives it. The menus' scenes (0x197E57,
// 0x1966C5), the cube map's cameras (0x1AD62B) and the 2D ones (0x15BAC8) stay as they are.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>

#include "app/app.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace juiced {
extern uint32_t fov_percent;  // src/common/game_info.cpp
}

namespace {
// the scene cameras narrowed to 4:3 (below): their angle is the game's 4:3 one already
std::unordered_set<uint32_t> g_narrowed;

float F32(uint32_t va) {
    float f;
    const uint32_t v = kernel::R32(va);
    std::memcpy(&f, &v, 4);
    return f;
}
void W32F(uint32_t va, float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    kernel::W32(va, v);
}

// a scene camera: the game's widescreen mode on (0x372EE4) and the 16:9 shape it gives a scene (+0xC0, height over width)
bool SceneCamera(uint32_t camera) { return kernel::R32(0x372EE4) && std::fabs(F32(camera + 0xC0) - 0.5625f) < 0.001f; }

// Narrower than 16:9 (the aspect setting at 4:3, applied at once: GameInfo::aspect_live keeps the game in its
// widescreen mode whatever the choice). The scene camera gets the angles the game's 4:3 mode gives it: the horizontal
// one without the widescreen factor (0x313634, 1.333), the vertical one derived for a 4:3 shape without the widescreen
// 1.15 (0x3317B4); one asked for outright stays, and a camera that takes it in proportion (+0x178) takes 0.75 of the
// horizontal angle. Up to 16:9 the horizontal tangent grows with the shape from there. angle: the game's own (+0xA8 as
// it set it); the projection is built again (vtable +0)
void Narrow(X86Context& ctx, uint32_t camera, float angle, uint32_t asked_vertical, float shape) {
    const float factor = F32(0x313634);
    if (!(factor > 1.0f && angle > 0.01f && angle < 3.1f)) return;
    const float base_angle = angle / factor;
    const float wide = std::min(2.0f * std::atan(std::tan(base_angle * 0.5f) * shape / (4.0f / 3.0f)), 3.1f);
    const float sine = std::sin(wide);
    W32F(camera + 0xA8, wide);
    W32F(camera + 0xAC, sine);
    W32F(camera + 0xB0, sine * sine * F32(0x3134A8));
    if (asked_vertical == kernel::R32(0x3134D0))
        W32F(camera + 0xB4, kernel::R32(camera + 0x178) ? base_angle * 0.75f : 2.0f * std::atan(std::tan(base_angle * 0.5f) * 0.75f));
    g_narrowed.insert(camera);
    const uint32_t saved = ctx.ecx;
    ctx.ecx = camera;
    kernel::CallGuest(ctx, kernel::R32(kernel::R32(camera)), nullptr, 0);
    ctx.ecx = saved;
}

// A view drawn in a box of its own: a pass at 0x12DA30 with a viewport inside the 640 wide frame (the race's map, 112x112
// at 56,324, its camera at the pass's +0x24; a menu's car panel). widescreen.cpp keeps that box's shape at any aspect,
// so its camera takes the game's 4:3 angles at any aspect, as at 4:3 (wider, the map was zoomed out and squeezed
// across). The box is only known at the pass's SetViewport (PassViewport, which calls InsetCamera), after the frame's
// projection is taken: such a camera is narrowed from then on as the game sets it (the race's map, every frame)
std::unordered_set<uint32_t> g_inset;
}  // namespace

namespace juiced {
void InsetCamera(uint32_t camera, bool inset) {
    if (!inset) g_inset.erase(camera);
    else if (g_inset.size() < 64) g_inset.insert(camera);
}
}  // namespace juiced

// a camera's horizontal angle as the game's 4:3 mode has it: in widescreen mode the stored angle times 0.75. A camera
// narrowed to 4:3 holds that angle itself
X86_EXTERN_FUNC(__imp__sub_00166B30);
X86_FUNC(sub_00166B30) {
    const bool narrowed = g_narrowed.count(ctx.ecx) != 0;
    const uint32_t flag = kernel::R32(0x372EE4);
    if (narrowed) kernel::W32(0x372EE4, 0);
    __imp__sub_00166B30(ctx, base);
    if (narrowed) kernel::W32(0x372EE4, flag);
}

X86_EXTERN_FUNC(__imp__sub_00166A20);
X86_FUNC(sub_00166A20) {
    const uint32_t percent = std::clamp<uint32_t>(juiced::fov_percent, 50, 150);
    const uint32_t ret = kernel::R32(ctx.esp);
    if (percent != 100 && (ret == 0x1272B || ret == 0x175B5 || ret == 0x177DE || ret == 0x17DF0)) {
        float fov;
        uint32_t bits = kernel::R32(ctx.esp + 4);
        std::memcpy(&fov, &bits, 4);
        if (fov > 0.01f && fov < 3.0f) {
            fov = std::min(2.0f * std::atan(std::tan(fov * 0.5f) * float(percent) / 100.0f), 3.0f);
            std::memcpy(&bits, &fov, 4);
            kernel::W32(ctx.esp + 4, bits);
        }
    }
    const uint32_t camera = ctx.ecx;
    const uint32_t asked_vertical = kernel::R32(ctx.esp + 8);  // the vertical angle, or the game's "derive it" (0x3134D0)
    __imp__sub_00166A20(ctx, base);
    // Wider than 16:9 (the aspect setting filling an ultrawide screen: app::FrameAspect). In the console's widescreen
    // mode (0x372EE4) the game widens a scene camera to 16:9 itself: its horizontal angle times 1.333, its shape (+0xC0,
    // height over width) 0.5625. Past that the horizontal half angle's tangent grows with the frame; the vertical angle
    // (+0xB4) stays, the sines the game keeps (+0xAC, +0xB0 = sine squared times 0x3134A8) follow, and the projection is
    // built again (vtable +0). The 2D's and the panels' cameras (shape 0.75) and the cube map's (1) are not scenes.
    const float frame = app::FrameAspect();
    const bool scene = SceneCamera(camera);
    const bool inset = scene && g_inset.count(camera);
    if (scene && (inset || frame < 16.0f / 9.0f - 0.01f)) {
        Narrow(ctx, camera, F32(camera + 0xA8), asked_vertical, inset ? 4.0f / 3.0f : frame);
            return;
    }
    g_narrowed.erase(camera);
    if (frame > 16.0f / 9.0f + 0.01f && scene) {
        const float angle = F32(camera + 0xA8);
        if (angle > 0.01f && angle < 3.0f) {
            const float wide = std::min(2.0f * std::atan(std::tan(angle * 0.5f) * frame / (16.0f / 9.0f)), 3.1f);
            const float sine = std::sin(wide);
            W32F(camera + 0xA8, wide);
            W32F(camera + 0xAC, sine);
            W32F(camera + 0xB0, sine * sine * F32(0x3134A8));
            const uint32_t saved = ctx.ecx;
            ctx.ecx = camera;
            kernel::CallGuest(ctx, kernel::R32(kernel::R32(camera)), nullptr, 0);
            ctx.ecx = saved;
        }
    }
}
