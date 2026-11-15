// Hooks for default.xbe. A strong definition replaces the recompiled function at that address, for example:
//
// #include "xdk/hle.h"
//
// X86_EXTERN_FUNC(__imp__sub_00012345);
// X86_FUNC(sub_00012345) {
//     __imp__sub_00012345(ctx, base);  // the original
// }

#include "kernel/kernel.h"
#include "xdk/hle.h"

#include <cstring>

namespace juiced {
extern bool skip_intro;  // setting skip_intro (src/common/game_info.cpp)
void SetMenuWindow(uint32_t window);  // src/common/pointer.cpp
void SetMenuManager(uint32_t manager);  // src/common/pointer.cpp
}

// the pages' common open (each page's open calls it with its window): the window showing, for the mouse in the menus
X86_EXTERN_FUNC(__imp__sub_00188790);
X86_FUNC(sub_00188790) {
    juiced::SetMenuWindow(kernel::R32(ctx.esp + 4));
    __imp__sub_00188790(ctx, base);
}

// a page registered with a UI manager (stdcall: manager, window name, page object): the manager in use, which in a
// race is another one (the pause menu's pages, 0x99310)
X86_EXTERN_FUNC(__imp__sub_0018B780);
X86_FUNC(sub_0018B780) {
    juiced::SetMenuManager(kernel::R32(ctx.esp + 4));
    __imp__sub_0018B780(ctx, base);
}

// The boot's two pages before the movies, each a countdown at page + 0xC (ecx the page) taken down by the frame's time
// every update: the driving WARNING page ("disclaimer", 0x7A830) then goes to the legal page ("legals", 0x7AAE0), which
// goes to the movies at 0 (or on a press once most of its time is gone). START does not cut the first, and the second
// only late: with skip_intro their time is up at once, about 10 s less before the title (Harry, 2026-11-15)
namespace {
void EndBootPage(X86Context& ctx) {
    if (!juiced::skip_intro) return;
    const float over = -0.001f;  // below 0 and not 0, as the pages test it
    uint32_t bits;
    std::memcpy(&bits, &over, 4);
    kernel::W32(ctx.ecx + 0xC, bits);
}
}  // namespace

X86_EXTERN_FUNC(__imp__sub_0007A830);
X86_FUNC(sub_0007A830) {
    EndBootPage(ctx);
    __imp__sub_0007A830(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_0007AAE0);
X86_FUNC(sub_0007AAE0) {
    EndBootPage(ctx);
    __imp__sub_0007AAE0(ctx, base);
}
