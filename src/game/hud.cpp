// Hide the HUD: the race's HUD window ("hud_race": the laps, times, map, position, the speed dial and the rivals'
// markers) is left undrawn. 0x18C820 draws one window, EDI, each of its gadgets by its vtable's +4, then the window's
// fade; the UI manager's draw (0x1893B0) calls it for each window of its stack, so the pause menu, its message boxes
// and the race's other windows still draw. Nothing of the game's state changes: the gadgets keep their hidden flags.
// Setting hide_hud (F9 in a race, PC SETTINGS > GAME, F1).
#include <cstring>

#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

namespace juiced {
extern bool hide_hud;  // src/common/game_info.cpp
}

X86_EXTERN_FUNC(__imp__sub_0018C820);
X86_FUNC(sub_0018C820) {
    const uint32_t window = ctx.edi;
    if (juiced::hide_hud && window >= 0x10000 && mem::IsCommitted(window) &&
        !std::strncmp(static_cast<const char*>(kernel::HostPtr(window)), "hud_race", 9)) {
        ctx.esp += 4;  // its RET: no arguments
        return;
    }
    __imp__sub_0018C820(ctx, base);
}
