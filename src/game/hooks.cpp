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

namespace juiced {
void SetMenuWindow(uint32_t window);  // src/common/pointer.cpp
}

// the pages' common open (each page's open calls it with its window): the window showing, for the mouse in the menus
X86_EXTERN_FUNC(__imp__sub_00188790);
X86_FUNC(sub_00188790) {
    juiced::SetMenuWindow(kernel::R32(ctx.esp + 4));
    __imp__sub_00188790(ctx, base);
}
