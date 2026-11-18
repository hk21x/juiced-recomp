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
// only does so after the mouse has moved or clicked, so a resting mouse leaves the keyboard and the pad alone. In a
// race (the game object's +0x3C set) only over a window with rows, the pause menu's: a press would reach the car.
//
// JUICED_POINTER=poll:x:y:buttons,... (headless): the pointer from each pad poll on, in 640x480; buttons 1 left, 2 right.
// JUICED_POINTER_LOG=1: each window's buttons and menus in the log as the pointer meets it.
// JUICED_POINTER=@file: the pointer as the file's "x y buttons" says, read again as the run goes.
#include <algorithm>
#include <array>
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

// The pointer's own picture, in the menus' colours (the logo's yellow running to the bands' orange, a dark edge and a
// soft shadow), drawn here: an arrow of `size` pixels, its tip the hot spot. Made for the window's height (32 at 1080
// lines) and again when that changes; Windows' arrow when it cannot be made.
HCURSOR MenuCursor(HWND hwnd) {
    static HCURSOR cursor = nullptr;
    static int made = 0;
    RECT r;
    int size = 32;
    if (GetClientRect(hwnd, &r) && r.bottom > 0) size = std::clamp(int(r.bottom * 32 / 1080), 24, 96);
    if (cursor && made == size) return cursor;
    // the arrow's outline in a unit square: tip, down the left edge, the notch, the tail, back up the right edge
    static const float kShape[7][2] = {{0.06f, 0.04f}, {0.06f, 0.80f}, {0.25f, 0.63f}, {0.39f, 0.93f},
                                       {0.52f, 0.87f}, {0.38f, 0.58f}, {0.64f, 0.58f}};
    const auto inside = [](float x, float y) {
        bool in = false;
        for (int i = 0, j = 6; i < 7; j = i++) {
            const float xi = kShape[i][0], yi = kShape[i][1], xj = kShape[j][0], yj = kShape[j][1];
            if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) in = !in;
        }
        return in;
    };
    const auto distance = [](float x, float y) {  // to the outline
        float best = 1e9f;
        for (int i = 0, j = 6; i < 7; j = i++) {
            const float ax = kShape[j][0], ay = kShape[j][1], bx = kShape[i][0] - ax, by = kShape[i][1] - ay;
            const float t = std::clamp(((x - ax) * bx + (y - ay) * by) / (bx * bx + by * by), 0.0f, 1.0f);
            best = std::min(best, std::hypot(x - ax - bx * t, y - ay - by * t));
        }
        return best;
    };
    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof bi;
    bi.bV5Width = size, bi.bV5Height = -size, bi.bV5Planes = 1, bi.bV5BitCount = 32, bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000, bi.bV5GreenMask = 0x0000FF00, bi.bV5BlueMask = 0x000000FF, bi.bV5AlphaMask = 0xFF000000;
    void* bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    HCURSOR next = nullptr;
    if (color && mask && bits) {
        auto* px = static_cast<uint32_t*>(bits);
        const float edge = 0.045f, shadow = 0.05f;
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                float fill = 0, line = 0, shade = 0;  // 4 by 4 samples
                for (int sy = 0; sy < 4; sy++)
                    for (int sx = 0; sx < 4; sx++) {
                        const float u = (x + (sx + 0.5f) / 4) / size, v = (y + (sy + 0.5f) / 4) / size;
                        const bool in = inside(u, v);
                        const float d = distance(u, v);
                        if (in && d > edge * 0.5f) fill += 1;
                        else if (d <= edge * (in ? 0.5f : 0.6f)) line += 1;
                        if (inside(u - shadow, v - shadow) || distance(u - shadow, v - shadow) < edge) shade += 1;
                    }
                fill /= 16, line /= 16, shade /= 16;
                const float t = std::clamp(float(y) / size / 0.9f, 0.0f, 1.0f);  // yellow at the tip, orange at the tail
                const float r = 255, g = 222 - 110 * t, b = 20 - 20 * t;
                const float a = std::min(1.0f, fill + line + 0.45f * shade * (1 - fill - line));
                const float cr = a > 0 ? (fill * r + line * 24) / a : 0, cg = a > 0 ? (fill * g + line * 16) / a : 0,
                            cb = a > 0 ? (fill * b + line * 40) / a : 0;
                px[y * size + x] = uint32_t(a * 255 + 0.5f) << 24 | uint32_t(cr + 0.5f) << 16 | uint32_t(cg + 0.5f) << 8 |
                                   uint32_t(cb + 0.5f);
            }
        ICONINFO info{};
        info.fIcon = FALSE;
        info.xHotspot = DWORD(0.06f * size), info.yHotspot = DWORD(0.04f * size);
        info.hbmMask = mask, info.hbmColor = color;
        next = reinterpret_cast<HCURSOR>(CreateIconIndirect(&info));
    }
    if (color) DeleteObject(color);
    if (mask) DeleteObject(mask);
    if (!next) return cursor ? cursor : LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    if (cursor) DestroyCursor(cursor);
    cursor = next, made = size;
    LOGI("juiced: the menu pointer's picture made, %d pixels", size);
    return cursor;
}

LRESULT CALLBACK PointerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_arrow.load() && !app::OverlayOpen()) {
        if (msg == WM_SETCURSOR && LOWORD(lp) == HTCLIENT) {
            SetCursor(MenuCursor(hwnd));
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

// A press played to the game: kHold polls down, then kGap up before the next. The menus take a press held for one
// poll and want one poll up between two; one more of each for a poll the game skips. (4 down and 5 up before: a row
// every 150 ms, which dragged behind the pointer; Harry, 2026-11-18.) JUICED_POINTER_PULSE=hold:gap (tests).
int g_hold = 2, g_gap = 2;
int Pulse() {
    static const bool read = [] {
        if (const char* e = std::getenv("JUICED_POINTER_PULSE")) std::sscanf(e, "%d:%d", &g_hold, &g_gap);
        g_hold = std::clamp(g_hold, 1, 30), g_gap = std::clamp(g_gap, 1, 30);
        return true;
    }();
    (void)read;
    return g_hold - 1 + g_gap;  // polls left after the first one down
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

// A carousel: a menu of choices side by side shown one at a time, its name centred with arrows either side (Arcade's
// series: "NOVICE SERIES" and an arrow, the strip at 386 to 410 of 480). Pointed at, a click on its left steps left,
// on its right steps right, on the name chooses it (Harry, 2026-11-18: the arrows took no click).
constexpr float kCarouselName = 70;  // half the name's width about the strip's centre

// Menus the d-pad does not walk, in the window they were found in: a list beside the one the presses move (Custom
// Race's CARS IN GROUP beside its groups), told when a press towards one of its rows moved another menu instead
uint32_t g_inert_window = 0;
std::array<uint32_t, 4> g_inert{};
bool Inert(uint32_t window, uint32_t g) {
    return window == g_inert_window && std::find(g_inert.begin(), g_inert.end(), g) != g_inert.end();
}
void MakeInert(uint32_t window, uint32_t g) {
    if (window != g_inert_window) g_inert = {}, g_inert_window = window;
    for (uint32_t& slot : g_inert)
        if (!slot) {
            slot = g;
            return;
        }
}
// the current items of the window's menus but one, mixed into one number: a press that moved another menu changes it
uint32_t OthersCurrent(uint32_t window, uint32_t except) {
    uint32_t sum = 0;
    for (uint32_t at = R32(window + kGadgets), end = R32(window + kGadgetsEnd); at && at < end && end - at < 0x1000; at += 4) {
        const uint32_t g = R32(at);
        if (g != except && Readable(g, 0x160) && R32(g + kType) == kMenu) sum = sum * 31 + g + R32(g + kCurrent) * 7919;
    }
    return sum;
}

// false when the window has nothing to point at; carousel: the carousel under the pointer, else 0. With no focused
// gadget, current is the current item of the menu under the pointer (each of a window's menus keeps one), else the
// last menu's
bool Places(uint32_t window, const Pointer& p, Place& under, Place& current, uint32_t* carousel = nullptr) {
    bool any = false;
    const uint32_t focus = R32(window + kFocus);
    Place mine;  // the current item of the menu under the pointer
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
            if (Inert(window, g)) continue;
            const float pitch = F(g + kItemHeight) * 480 * kPitch, top = y0 + kFirst;
            if (pitch * float(count) > h + pitch) {  // a row of choices side by side (Arcade's series): a carousel
                if (carousel && p.x >= x0 && p.x < x0 + w && p.y >= y0 - 6 && p.y < y0 + h + 6) *carousel = g;
                continue;
            }
            any = true;
            const uint32_t cur = R32(g + kCurrent);
            const Place now = {g, cur < count ? int(cur) : -1, x0, top + pitch * float(cur)};
            if (!focus && cur < count) current = now;
            if (pitch > 4 && p.x >= x0 && p.x < x0 + w && p.y >= top && p.y < top + pitch * float(count)) {
                const int i = int((p.y - top) / pitch);
                under = {g, i, x0, top + pitch * float(i)};
                mine = cur < count ? now : Place{};
            }
        }
    }
    if (!focus && under.index >= 0 && under.gadget == mine.gadget) current = mine;
    return any;
}
}  // namespace

// the pages' common open (src/game/hooks.cpp), for the windows' manager
void SetMenuWindow(uint32_t window) { g_manager = kernel::R32(window + kManager); }
// a page registered with a manager (src/game/hooks.cpp): the race's own manager, which the pause menu's windows are in
void SetMenuManager(uint32_t manager) {
    if (g_manager.exchange(manager) != manager) LOGI("juiced: UI manager %08X", manager);
}

uint32_t MenuManager() { return g_manager.load(); }
// the race's HUD and pause menu at the frame's sides on a wide frame (src/game/widescreen.cpp); the HUD position
// setting off, or JUICED_HUD_ANCHOR=0 (tests), leaves them in the centred 4:3 part
extern bool hud_sides;  // src/common/game_info.cpp
bool HudAnchored() {
    static const bool off = [] {
        const char* e = std::getenv("JUICED_HUD_ANCHOR");
        return e && *e == '0';
    }();
    return !off && hud_sides;
}

// GameInfo::pad, controller 1
void MenuPointer(input::Pad& pad) {
    static Pointer last;
    static int armed = 0;        // polls the pointer still steers the selection for, after it last moved
    static int pulse = 0, held = -1;  // the press being played: polls left of it and its gap
    static bool click = false;   // a left click waits for the selection to reach the pointer
    static Place tried, from, stuck;  // the last move's target and start; a target a press did not move towards
    static uint32_t others = 0;       // OthersCurrent as the last move started
    static int polls = 0;
    polls++;
    Pointer p;
    // the window showing: the last of the manager's stack of them (+0 to +4; "opts_bg", "opts_menu" in Options)
    const uint32_t manager = g_manager.load();
    uint32_t window = 0;
    if (Readable(manager, 0x100) && R32(manager + 4) > R32(manager) && R32(manager + 4) - R32(manager) <= 0x40)
        window = R32(R32(manager + 4) - 4);
    if (Readable(manager, 0x100) && R32(manager + kModal)) window = R32(manager + kModal);  // a message box over it
    if (!PointerAt(p) || !Readable(window, 0x100) || !Readable(kGame, 4) || !Readable(R32(kGame), 0x40)) {
        armed = 0, click = false;
        ShowArrow(false);
        return;
    }
    // in a race only a window with rows counts (the pause menu and what it opens): the race's own windows have
    // none, and a press there would reach the car
    const bool race = R32(R32(kGame) + 0x3C) != 0;
    if (race) {
        Place u, c;
        if (R32(manager + kTransition) || !Places(window, p, u, c)) {
            armed = 0, click = false, pulse = 0;
            ShowArrow(false);
            return;
        }
    }
    const uint32_t pressed = p.buttons & ~last.buttons;
    if (std::fabs(p.x - last.x) + std::fabs(p.y - last.y) > 0.5f || pressed) armed = 90;
    last = p;
    // the press under way (Pulse)
    if (pulse > 0) {
        if (pulse > g_gap && held >= 0) Press(pad, held);
        pulse--;
        return;
    }
    if (R32(manager + kTransition)) return;  // between two windows
    if (pressed & 2) {
        held = input::kB, pulse = Pulse();
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
    uint32_t carousel = 0;
    const bool any = Places(window, p, under, current, &carousel);
    // the arrow on every screen the mouse does something on: in the menus a click is A on a screen without rows too
    // (Arcade's series and its races had no arrow; Harry, 2026-11-18)
    ShowArrow(any || !race);
    if ((pressed & 1) && carousel && !under.gadget) {  // a carousel: its left, its right, its name
        const float mid = (F(carousel + kRect) + F(carousel + kRect + 8) / 2) * 640;
        held = std::fabs(p.x - mid) < kCarouselName ? input::kA : p.x < mid ? input::kLeft : input::kRight;
        if (trace) LOGI("pointer: carousel %08X current %u, at %.0f: %s", carousel, R32(carousel + kCurrent), p.x, held == input::kA ? "A" : held == input::kLeft ? "left" : "right");
        pulse = Pulse();
        Press(pad, held);
        return;
    }
    if ((pressed & 1) && !under.gadget && !race) {  // off the rows, or a screen with none (a series, a car to choose): A
        held = input::kA, pulse = Pulse();
        Press(pad, held);
        return;
    }
    if (!any || !under.gadget) {
        click = false;
        return;
    }
    // a press that changed nothing (a screen whose rows the d-pad does not walk): leave that row be
    // a press towards a menu's row that did not step that menu one row its way but moved another menu: a list the
    // d-pad does not walk (Custom Race's cars, centred on the group's middle car, jump two rows as the group changes)
    if (tried.gadget && tried.index >= 0 && from.index >= 0 && OthersCurrent(window, tried.gadget) != others) {
        const int step = tried.y > from.y ? 1 : -1;
        const int now = current.gadget == tried.gadget ? current.index : int(R32(tried.gadget + kCurrent));
        if (now != from.index + step) {
            if (trace) LOGI("pointer: menu %08X is not walked by the d-pad", tried.gadget);
            MakeInert(window, tried.gadget);
            tried = {};
            return;
        }
    }
    if (tried.gadget && tried == under && from == current) stuck = under;
    tried = {};
    if (!(stuck == under)) stuck = {};
    if (pressed & 1) click = true;
    if (!armed && !click) return;
    if (armed) armed--;
    static int since = 0;  // the poll the pointer's row was first not the current one (JUICED_POINTER_LOG)
    if (!(under == current)) since = since ? since : polls;
    else if (since) {
        if (trace) LOGI("pointer: reached the row after %d polls", polls - since);
        since = 0;
    }
    if (under == current) {
        if (click) {
            click = false;
            held = input::kA, pulse = Pulse();
            Press(pad, held);
        }
        return;
    }
    if (!current.gadget || stuck.gadget) {
        click = false;
        return;
    }
    tried = under, from = current, others = OthersCurrent(window, under.gadget);
    if (std::fabs(under.y - current.y) > 1) held = under.y > current.y ? input::kDown : input::kUp;
    else held = under.x > current.x ? input::kRight : input::kLeft;
    pulse = Pulse();
    Press(pad, held);
}

}  // namespace juiced
