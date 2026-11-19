// Hide the HUD: the race's HUD window ("hud_race"; Showoff's, a sprint's and a solo event's, IsRaceHud: the laps, times, map, position, the speed dial and the rivals'
// markers) is left undrawn. 0x18C820 draws one window, EDI, each of its gadgets by its vtable's +4, then the window's
// fade; the UI manager's draw (0x1893B0) calls it for each window of its stack, so the pause menu, its message boxes
// and the race's other windows still draw. Nothing of the game's state changes: the gadgets keep their hidden flags.
// Setting hide_hud (F9 in a race, PC SETTINGS > GAME, F1).
#include <atomic>
#include <cstring>

#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

namespace juiced {
extern bool hide_hud;                 // src/common/game_info.cpp
extern std::atomic<bool> showoff;     // src/common/game_info.cpp: Showoff's HUD drawn in this race
}

namespace juiced {
// a single player's HUD window: a circuit race's, Showoff's, a sprint's, a solo event's (names in default.xbe; split
// screen's "hud_split" is laid out per view and left as it is). Anchored to the frame's sides (src/game/widescreen.cpp)
// and hidden by hide_hud
bool IsRaceHud(const char* n) {
    for (const char* h : {"hud_race", "hud_cruise", "hud_sprint", "hud_solo"})
        if (!std::strcmp(n, h)) return true;
    return false;
}
}  // namespace juiced

X86_EXTERN_FUNC(__imp__sub_0018C820);
X86_FUNC(sub_0018C820) {
    const uint32_t window = ctx.edi;
    const char* name = window >= 0x10000 && mem::IsCommitted(window) ? static_cast<const char*>(kernel::HostPtr(window)) : "";
    const bool cruise = !std::strncmp(name, "hud_cruise", 11);  // Showoff's HUD (score, move points, combo)
    if (cruise && !juiced::showoff.load()) juiced::showoff = true;
    if (juiced::hide_hud && juiced::IsRaceHud(name)) {
        ctx.esp += 4;  // its RET: no arguments
        return;
    }
    __imp__sub_0018C820(ctx, base);
}
