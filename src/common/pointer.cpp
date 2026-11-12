// The mouse in the menus: the row under the pointer becomes the current one, a left click chooses it (A), a right
// click goes back (B). Off the rows, and on a screen with none, a left click is A.
//
// A menu screen is a window of gadgets (src/game/pcmenu.cpp has the layout: a window's gadget pointers at +0x5C to
// +0x60, its focused gadget at +0x7C; a gadget's type at +0xC (4 a button, 6 a menu), hidden at +0x58, its rectangle
// at +0x8C as fractions of the 640x480 screen; a menu's items at +0x11C to +0x120, its current item at +0x13C, its
// item height at +0x140). The window showing is the last of the UI manager's stack of them (manager +0 to +4), or
// the message box over it (manager +0x44; the
// manager is each window's +0x40, taken from the pages' common open 0x188790, src/game/hooks.cpp). The pointer moves
// the selection as the d-pad would, by presses of up and down, so the game's own rules, highlights and sounds hold; it
// only does so after the mouse has moved or clicked, so a resting mouse leaves the keyboard and the pad alone. Only
// in the front end (the game object's +0x3C clear): a press in a race would reach the car.
//
// JUICED_POINTER=poll:x:y:buttons,... (headless): the pointer from each pad poll on, in 640x480; buttons 1 left, 2 right.
// JUICED_POINTER_LOG=1: each window's buttons and menus in the log as the pointer meets it.
// JUICED_POINTER=@file: the pointer as the file's "x y buttons" says, read again as the run goes.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace juiced {
namespace {
using kernel::R32;

constexpr uint32_t kGame = 0x372B54;
constexpr uintptr_t kCursorTimerId = 0x5243;  // kCursorTimer
constexpr uint32_t kGadgets = 0x5C, kGadgetsEnd = 0x60, kFocus = 0x7C, kManager = 0x40, kModal = 0x44, kTransition = 0xBC;
constexpr uint32_t kType = 0xC, kHidden = 0x58, kRect = 0x8C;
constexpr uint32_t kButton = 4, kMenu = 6;
constexpr uint32_t kItems = 0x11C, kItemsEnd = 0x120, kCurrent = 0x13C, kItemHeight = 0x140, kItemName = 0x28;
// a menu's rows as drawn: the Options menu's (item height 0.04) are 22.3 lines of 480 apart, the first 2 below the
// menu's top (captures)
constexpr float kPitch = 22.3f / (0.04f * 480.0f), kFirst = 2.0f;

std::atomic<uint32_t> g_manager{0};

float F(uint32_t va) {
    float f;
    uint32_t v = R32(va);
    std::memcpy(&f, &v, 4);
    return f;
}

struct Pointer {
    float x = 0, y = 0;
    uint32_t buttons = 0;
};

bool Forced(Pointer& p) {
    static const char* e = std::getenv("JUICED_POINTER");
    static int poll = 0;
    static Pointer at;
    static bool on = false;
    if (e && *e == '@') {  // @file: "x y buttons" read from the file as it changes (tests by hand)
        static int every = 0;
        if (every++ % 6 == 0) {
            if (std::FILE* f = std::fopen(e + 1, "r")) {
                float x, y;
                unsigned b;
                if (std::fscanf(f, "%f %f %u", &x, &y, &b) == 3) at = {x, y, b}, on = true;
                std::fclose(f);
            }
        }
        if (on) p = at;
        return on;
    }
    if (!e || !*e) return on ? (p = at, true) : false;
    poll++;
    int when, used;
    float x, y;
    unsigned b;
    while (std::sscanf(e, "%d:%f:%f:%u%n", &when, &x, &y, &b, &used) == 4 && when <= poll) {
        at = {x, y, b};
        on = true;
        e += used;
        if (*e == ',') e++;
    }
    if (on) p = at;
    return on;
}

bool PointerAt(Pointer& p) {
    if (Forced(p)) return true;
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(app::Window());
    POINT c;
    RECT r;
    if (!hwnd || GetForegroundWindow() != hwnd || app::OverlayOpen() || !GetCursorPos(&c) || !ScreenToClient(hwnd, &c) ||
        !GetClientRect(hwnd, &r) || r.right <= 0 || r.bottom <= 0)
        return false;
    // the frame in the window (bars where its shape differs)
    const float window = float(r.right) / float(r.bottom), frame = app::FrameAspect() > 0 ? app::FrameAspect() : window;
    const float fw = frame < window ? frame / window : 1.0f, fh = frame > window ? window / frame : 1.0f;
    p.x = 640 * (0.5f + (float(c.x) / r.right - 0.5f) / fw);
    p.y = 480 * (0.5f + (float(c.y) / r.bottom - 0.5f) / fh);
    // the 2D is squeezed into the frame's centred 4:3 part on a wider frame (GameInfo::hud_aspect)
    if (frame > app::HudAspect() + 1e-3f) p.x = 320 + (p.x - 320) * frame / app::HudAspect();
    p.buttons = (GetAsyncKeyState(VK_LBUTTON) & 0x8000 ? 1u : 0u) | (GetAsyncKeyState(VK_RBUTTON) & 0x8000 ? 2u : 0u);
    return true;
#else
    return false;
#endif
}

#ifdef _WIN32
// the arrow while a menu the pointer works in shows: the runtime hides the cursor over the game (WM_SETCURSOR, and
// its half second timer kCursorTimer, sdk/runtime/app/main.cpp)
WNDPROC g_proc = nullptr;
std::atomic<bool> g_arrow{false};
LRESULT CALLBACK PointerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_arrow.load() && !app::OverlayOpen()) {
        if (msg == WM_SETCURSOR && LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW)));
            return TRUE;
        }
        if (msg == WM_TIMER && wp == kCursorTimerId) return 0;
    }
    return CallWindowProcW(g_proc, hwnd, msg, wp, lp);
}
#endif
void ShowArrow(bool show) {
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(app::Window());
    if (!hwnd) return;
    if (!g_proc) {
        if (!show) return;
        g_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PointerProc)));
        LOGI("juiced: the menu pointer is ready");
    }
    if (g_arrow.exchange(show) != show)
        PostMessageW(hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
#else
    (void)show;
#endif
}

void Press(input::Pad& p, int c) {
    if (c < input::kA) p.buttons |= uint16_t(1u << c);
    else p.analog[c - input::kA] = 255;
}

// what is under the pointer and what is current, as places in one list: a button (its gadget, placed by its top) or
// a menu's item
struct Place {
    uint32_t gadget = 0;
    int index = -1;  // a menu's item, -1 a button
    float x = 0, y = 0;
    bool operator==(const Place& o) const { return gadget == o.gadget && index == o.index; }
};

bool Readable(uint32_t va, uint32_t bytes) { return va >= 0x10000 && mem::IsCommitted(va) && mem::IsCommitted(va + bytes - 1); }

// false when the window has nothing to point at
bool Places(uint32_t window, const Pointer& p, Place& under, Place& current) {
    bool any = false;
    const uint32_t focus = R32(window + kFocus);
    for (uint32_t at = R32(window + kGadgets), end = R32(window + kGadgetsEnd); at && at < end && end - at < 0x1000; at += 4) {
        const uint32_t g = R32(at);
        if (!Readable(g, 0x160) || R32(g + kHidden)) continue;
        const float x0 = F(g + kRect) * 640, y0 = F(g + kRect + 4) * 480, w = F(g + kRect + 8) * 640, h = F(g + kRect + 12) * 480;
        const uint32_t type = R32(g + kType);
        if (type == kButton) {
            any = true;
            if (g == focus) current = {g, -1, x0, y0};
            // a row is its text and the value under it, to the next row: the lowest row that starts above the pointer
            if (p.x >= x0 && p.x < x0 + w && p.y >= y0 - 2 && p.y < y0 + std::max(h, 38.0f) && (!under.gadget || y0 >= under.y))
                under = {g, -1, x0, y0};
        } else if (type == kMenu) {
            const uint32_t items = R32(g + kItems), count = (R32(g + kItemsEnd) - items) / 4;
            if (!items || !count || count > 64) continue;
            const uint32_t first = R32(items);
            if (!Readable(first, 0x58) || !kernel::G<uint8_t>(first + kItemName)) continue;  // a menu of backdrops
            const float pitch = F(g + kItemHeight) * 480 * kPitch, top = y0 + kFirst;
            if (pitch * float(count) > h + pitch) continue;  // a row of choices side by side (Arcade's series)
            any = true;
            const uint32_t cur = R32(g + kCurrent);
            if (!focus && cur < count) current = {g, int(cur), x0, top + pitch * float(cur)};
            if (pitch > 4 && p.x >= x0 && p.x < x0 + w && p.y >= top && p.y < top + pitch * float(count)) {
                const int i = int((p.y - top) / pitch);
                under = {g, i, x0, top + pitch * float(i)};
            }
        }
    }
    return any;
}
}  // namespace

// the pages' common open (src/game/hooks.cpp), for the windows' manager
void SetMenuWindow(uint32_t window) { g_manager = kernel::R32(window + kManager); }

// GameInfo::pad, controller 1
void MenuPointer(input::Pad& pad) {
    static Pointer last;
    static int armed = 0;        // polls the pointer still steers the selection for, after it last moved
    static int pulse = 0, held = -1;  // the press being played: polls left of it and its gap
    static bool click = false;   // a left click waits for the selection to reach the pointer
    static Place tried, from, stuck;  // the last move's target and start; a target a press did not move towards
    Pointer p;
    // the window showing: the last of the manager's stack of them (+0 to +4; "opts_bg", "opts_menu" in Options)
    const uint32_t manager = g_manager.load();
    uint32_t window = 0;
    if (Readable(manager, 0x100) && R32(manager + 4) > R32(manager) && R32(manager + 4) - R32(manager) <= 0x40)
        window = R32(R32(manager + 4) - 4);
    if (Readable(manager, 0x100) && R32(manager + kModal)) window = R32(manager + kModal);  // a message box over it
    if (!PointerAt(p) || !Readable(window, 0x100) || !Readable(kGame, 4) || !Readable(R32(kGame), 0x40) ||
        R32(R32(kGame) + 0x3C)) {
        armed = 0, click = false;
        ShowArrow(false);
        return;
    }
    const uint32_t pressed = p.buttons & ~last.buttons;
    if (std::fabs(p.x - last.x) + std::fabs(p.y - last.y) > 0.5f || pressed) armed = 90;
    last = p;
    // the press under way: 3 polls down, then 5 up before the next
    if (pulse > 0) {
        if (pulse > 5 && held >= 0) Press(pad, held);
        pulse--;
        return;
    }
    if (R32(manager + kTransition)) return;  // between two windows
    if (pressed & 2) {
        held = input::kB, pulse = 8;
        Press(pad, held);
        return;
    }
    static const bool trace = std::getenv("JUICED_POINTER_LOG") != nullptr;  // each window as the pointer meets it
    static uint32_t traced = 0;
    if (trace && traced != window) {
        traced = window;
        LOGI("pointer: window '%.32s' focus %08X", static_cast<const char*>(kernel::HostPtr(window)), R32(window + kFocus));
        for (uint32_t at = R32(window + kGadgets), end = R32(window + kGadgetsEnd); at && at < end && end - at < 0x1000; at += 4) {
            const uint32_t g = R32(at), type = Readable(g, 0x160) ? R32(g + kType) : 0;
            if (type != kButton && type != kMenu) continue;
            LOGI("pointer:   %08X '%.24s' type %u hidden %u at %.0f %.0f size %.0f %.0f items %u current %u height %.3f", g,
                 static_cast<const char*>(kernel::HostPtr(g + 0x38)), type, R32(g + kHidden), F(g + kRect) * 640,
                 F(g + kRect + 4) * 480, F(g + kRect + 8) * 640, F(g + kRect + 12) * 480,
                 type == kMenu ? (R32(g + kItemsEnd) - R32(g + kItems)) / 4 : 0, R32(g + kCurrent), F(g + kItemHeight));
        }
    }
    Place under, current;
    const bool any = Places(window, p, under, current);
    ShowArrow(any);
    if ((pressed & 1) && !under.gadget) {  // off the rows, or a screen with none (a series, a car to choose): A
        held = input::kA, pulse = 8;
        Press(pad, held);
        return;
    }
    if (!any || !under.gadget) {
        click = false;
        return;
    }
    // a press that changed nothing (a screen whose rows the d-pad does not walk): leave that row be
    if (tried.gadget && tried == under && from == current) stuck = under;
    tried = {};
    if (!(stuck == under)) stuck = {};
    if (pressed & 1) click = true;
    if (!armed && !click) return;
    if (armed) armed--;
    if (under == current) {
        if (click) {
            click = false;
            held = input::kA, pulse = 8;
            Press(pad, held);
        }
        return;
    }
    if (!current.gadget || stuck.gadget) {
        click = false;
        return;
    }
    tried = under, from = current;
    if (std::fabs(under.y - current.y) > 1) held = under.y > current.y ? input::kDown : input::kUp;
    else held = under.x > current.x ? input::kRight : input::kLeft;
    pulse = 8;
    Press(pad, held);
}

}  // namespace juiced
