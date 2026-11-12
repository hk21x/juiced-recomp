// The speed blur. 0x130910 draws the frame buffer over itself six times a frame (D3DDevice_DrawVerticesUP at
// 0x130EAC: twelve vertices of 20 bytes, a triangle list: x, y, colour, u, v), each pass a little larger about the screen's
// centre and blended by its alpha, more with speed.
//
// Each vertex's u, v is its own x, y before the enlargement, in texels. The console samples a pixel at its corner, so
// u = x reads the frame half a texel up and left of the pixel: at the console's 640x480 that only softens (each pass
// averages four texels), but drawn at a PC's resolution it moves the whole frame half a console pixel a pass, six
// times, a smear several pixels wide over everything: the race looked very blurred on Windows (Harry, 2026-11-12).
// Here u, v get the half texel, so a pass with no enlargement reads each pixel from itself and only the enlargement
// blurs, as the effect means. Setting speed_blur off leaves the passes out.
#include <cstdlib>
#include <cstring>

#include "kernel/kernel.h"
#include "xdk/d3d8.h"
#include "xdk/hle.h"

namespace juiced {
extern bool speed_blur;  // setting speed_blur (src/common/game_info.cpp)
}

// at 0x130EAC ([esp] primitive, +4 count, +8 vertices, +12 stride): true leaves the draw out
bool SpeedBlurPass(X86Context& ctx, uint8_t* /*base*/) {
    static const char* mode = std::getenv("JUICED_BLUR");  // tests: 0 none, 1 as the disc draws it
    if (!juiced::speed_blur || (mode && *mode == '0')) {
        ctx.esp += 16;
        return true;
    }
    d3d::State().no_squeeze = true;  // a full screen effect: not the 2D the renderers squeeze (src/game/widescreen.cpp)
    if (mode && *mode == '1') return false;
    const uint32_t count = kernel::R32(ctx.esp + 4), verts = kernel::R32(ctx.esp + 8), stride = kernel::R32(ctx.esp + 12);
    if (count != 12 || stride != 20) return false;
    for (uint32_t i = 0; i < count; i++)
        for (uint32_t o : {12u, 16u}) {
            float f;
            uint32_t v = kernel::R32(verts + i * stride + o);
            std::memcpy(&f, &v, 4);
            f += 0.5f;
            std::memcpy(&v, &f, 4);
            kernel::W32(verts + i * stride + o, v);
        }
    return false;
}

// at 0x130EB1, after the pass's draw
void SpeedBlurPassDone(X86Context& /*ctx*/, uint8_t* /*base*/) { d3d::State().no_squeeze = false; }
