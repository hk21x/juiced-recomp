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

#include "app/app.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace juiced {
extern uint32_t fov_percent;  // src/common/game_info.cpp
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
    __imp__sub_00166A20(ctx, base);
    // Wider than 16:9 (the aspect setting filling an ultrawide screen: app::FrameAspect). In the console's widescreen
    // mode (0x372EE4) the game widens a scene camera to 16:9 itself: its horizontal angle times 1.333, its shape (+0xC0,
    // height over width) 0.5625. Past that the horizontal half angle's tangent grows with the frame; the vertical angle
    // (+0xB4) stays, the sines the game keeps (+0xAC, +0xB0 = sine squared times 0x3134A8) follow, and the projection is
    // built again (vtable +0). The 2D's and the panels' cameras (shape 0.75) and the cube map's (1) are not scenes.
    const float frame = app::FrameAspect();
    auto f32 = [](uint32_t va) {
        float f;
        const uint32_t v = kernel::R32(va);
        std::memcpy(&f, &v, 4);
        return f;
    };
    auto w32 = [](uint32_t va, float f) {
        uint32_t v;
        std::memcpy(&v, &f, 4);
        kernel::W32(va, v);
    };
    if (frame > 16.0f / 9.0f + 0.01f && kernel::R32(0x372EE4) && std::fabs(f32(camera + 0xC0) - 0.5625f) < 0.001f) {
        const float angle = f32(camera + 0xA8);
        if (angle > 0.01f && angle < 3.0f) {
            const float wide = std::min(2.0f * std::atan(std::tan(angle * 0.5f) * frame / (16.0f / 9.0f)), 3.1f);
            const float sine = std::sin(wide);
            w32(camera + 0xA8, wide);
            w32(camera + 0xAC, sine);
            w32(camera + 0xB0, sine * sine * f32(0x3134A8));
            const uint32_t saved = ctx.ecx;
            ctx.ecx = camera;
            kernel::CallGuest(ctx, kernel::R32(kernel::R32(camera)), nullptr, 0);
            ctx.ecx = saved;
        }
    }
}
