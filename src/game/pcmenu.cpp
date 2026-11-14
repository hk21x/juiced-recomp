// PC settings in the game's own menus: Options > PC SETTINGS, from the title's menus and the pause menu.
//
// The game's menus are windows of gadgets laid out by text files in ui.dat, each window with a page object whose
// vtable has its open (+0) and message (+4) functions; the options pages are registered by name with a UI manager
// (0x99210 the front end's, 0x99310 the pause menu's: 0x18B780 registers, 0x18AE70 loads the window).
//
// ui.dat still holds the other platforms' graphics options window, "opts_graphics": seven rows (two with meters, five
// with a value text between arrows), RESET and DONE, and the Options menu's handler still opens it for an item
// named "graph" (0x95736), which the Xbox build removes from the menu when it opens (0x952F7) and has no page for.
// Here that window is registered with the Xbox Live options page's object (0x32E15C; its open 0x98D90 and message
// 0x98E30 are replaced below and pass the Xbox Live window on to the game's own), the Options menu gets an item
// "pc_settings" as the Xbox build adds its "xbox_live" (0x95355), and the window's rows show the settings of the F1
// window, a page of seven at a time: left and right change a row, A steps it on, RESET is NEXT PAGE, DONE and B go
// back. Captions are set as the game sets them (0x195390: UTF-16 into the gadget's caption at +0x10C).
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"
#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace kernel;

namespace juiced {
extern bool modern_buttons, skip_intro, speed_blur, smooth_motion;  // src/common
extern uint32_t fov_percent, effects_percent;
}

namespace {

constexpr uint32_t kRegisterPage = 0x18B780;  // stdcall (manager, window name, page object)
constexpr uint32_t kLoadWindow = 0x18AE70;    // eax the manager, edi the window's name
constexpr uint32_t kSetCaption = 0x195390;    // ecx the caption (gadget + 0x10C), eax UTF-16 text
constexpr uint32_t kAddItem = 0x191C00;       // stdcall (menu, caption, 0, item name, 0, index)
constexpr uint32_t kAddTextured = 0x191E60;   // stdcall (menu, 0, texture name, 0, 0, 0, 0, -1): the row's backdrop
constexpr uint32_t kLivePage = 0x32E15C;      // the Xbox Live options page's object (opts_live)
constexpr uint32_t kWindowName = 0x2FBF80;    // "opts_graphics", as the Options menu's "graph" item names it
constexpr uint32_t kBackdrop = 0x2FBEE4;      // "ui_pan5_ora", the Options rows' backdrop texture
constexpr uint32_t kGame = 0x372B54;          // the game object: +0x3C set in a race (the pause menu's windows)
constexpr uint32_t kGadgets = 0x5C, kGadgetsEnd = 0x60;  // a window's gadget pointers
constexpr uint32_t kGadgetName = 0x38, kGadgetHidden = 0x58, kGadgetTicker = 0x108, kGadgetCaption = 0x10C;
constexpr uint32_t kMsgGadget = 0x7C, kMsgType = 0x88, kMsgManager = 0x40;
constexpr uint32_t kTransition = 0xBC, kTransitionName = 0xC0;  // a manager's next window: 7, then its name
constexpr uint32_t kLeft = 3, kRight = 4, kSelect = 5, kBack = 0xC;

// ---- the settings (as EoN's and the F1 window's)

struct Row {
    const char* label;
    std::string (*text)();
    void (*step)(int);
};
std::string OnOff(bool b) { return b ? "ON" : "OFF"; }
void Toggle(bool& b) {
    b = !b;
    app::Save();
}
template <typename T>
int Cycle(T v, const T* list, int n, int d) {  // the index after v's
    int i = 0;
    for (int k = 0; k < n; k++)
        if (list[k] == v) i = k;
    return (i + d + n) % n;
}
#define FLAG(name, expr)                                            \
    {                                                               \
        name, [] { return OnOff(expr); }, [](int) { Toggle(expr); } \
    }

struct Mode {
    uint32_t w, h;
    bool operator==(const Mode& o) const { return w == o.w && h == o.h; }
};
std::vector<Mode> Modes() {
    std::vector<Mode> list{{0, 0}};  // the desktop's
#if defined(_WIN32)
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); i++) {
        Mode m{dm.dmPelsWidth, dm.dmPelsHeight};
        if (dm.dmBitsPerPel >= 32 && dm.dmPelsHeight >= 480 && std::find(list.begin(), list.end(), m) == list.end())
            list.push_back(m);
    }
    std::sort(list.begin() + 1, list.end(), [](const Mode& a, const Mode& b) { return a.w != b.w ? a.w > b.w : a.h > b.h; });
#endif
    return list;
}

const char* const kModes[] = {"FULLSCREEN", "BORDERLESS", "WINDOW"};
const char* const kAspects[] = {"AUTO", "4:3", "16:9", "FILL"};
const char* const kMsaaNames[] = {"OFF", "2X", "4X", "8X"};
constexpr uint32_t kAnisos[] = {0, 2, 4, 8, 16};
constexpr uint32_t kCaps[] = {30, 60, 120, 144, 240, 0};

constexpr int kRows = 7;
struct Page {
    const char* name;
    Row rows[kRows];
};
const Page kPages[] = {
    {"DISPLAY",
     {{"DISPLAY MODE", [] { return std::string(kModes[uint32_t(app::Get().display_mode) % 3]); },
       [](int d) {
           auto& s = app::Get();
           s.display_mode = app::DisplayMode((int(s.display_mode) + d + 3) % 3);
           app::Save();
           app::ApplyDisplay();
       }},
      {"RESOLUTION",
       [] {
           auto& s = app::Get();
           return s.width && s.height ? std::to_string(s.width) + " X " + std::to_string(s.height) : std::string("DESKTOP");
       },
       [](int d) {
           auto& s = app::Get();
           static const std::vector<Mode> list = Modes();
           Mode m = list[size_t(Cycle(Mode{s.width, s.height}, list.data(), int(list.size()), d))];
           s.width = m.w;
           s.height = m.h;
           app::Save();
           app::ApplyDisplay();
       }},
      {"ASPECT (NEXT START)", [] { return std::string(kAspects[app::Get().aspect % 4]); },
       [](int d) {
           auto& s = app::Get();
           s.aspect = (s.aspect % 4 + uint32_t(d + 4)) % 4;
           app::Save();
       }},
      FLAG("VERTICAL SYNC", app::Get().vsync),
      {"FRAME RATE CAP",
       [] {
           uint32_t c = app::Get().fps_cap;
           return c ? std::to_string(c) : std::string("UNLIMITED");
       },
       [](int d) {
           auto& s = app::Get();
           s.fps_cap = kCaps[Cycle(s.fps_cap, kCaps, 6, d)];
           app::Save();
       }},
      FLAG("SHOW FRAME RATE", app::Get().show_fps),
      FLAG("PAUSE IN BACKGROUND", app::Get().pause_unfocused)}},
    {"GRAPHICS",
     {{"RENDER SCALE", [] { return std::to_string(app::Get().render_percent) + "%"; },
       [](int d) {
           auto& s = app::Get();
           s.render_percent = uint32_t(std::clamp(int(s.render_percent) + d * 25, 25, 200));
           app::Save();
       }},
      {"MSAA", [] { return std::string(kMsaaNames[std::min<uint32_t>(app::Get().msaa, 3)]); },
       [](int d) {
           auto& s = app::Get();
           s.msaa = (std::min<uint32_t>(s.msaa, 3) + uint32_t(d + 4)) % 4;
           app::Save();
       }},
      {"EDGE SMOOTHING",
       [] {
           auto& s = app::Get();
           return std::string(s.smaa ? "SMAA" : s.fxaa ? "FXAA" : "OFF");
       },
       [](int d) {
           auto& s = app::Get();
           int at = (s.smaa ? 2 : s.fxaa ? 1 : 0);
           at = (at + d + 3) % 3;
           s.fxaa = at == 1;
           s.smaa = at == 2;
           app::Save();
       }},
      {"ANISOTROPIC FILTER",
       [] {
           uint32_t a = app::Get().aniso;
           return a ? std::to_string(a) + "X" : std::string("DEFAULT");
       },
       [](int d) {
           auto& s = app::Get();
           s.aniso = kAnisos[Cycle(s.aniso, kAnisos, 5, d)];
           app::Save();
       }},
      FLAG("SHARP TEXT AND HUD", app::Get().sharp_2d),
      FLAG("SHARPER WORLD TEXTURES", app::Get().world_textures),
      {"SHARPEN", [] { return std::to_string(app::Get().sharpen) + "%"; },
       [](int d) {
           auto& s = app::Get();
           s.sharpen = uint32_t(std::clamp(int(s.sharpen) + d * 10, 0, 100));
           app::Save();
       }}}},
    {"GAME",
     {FLAG("MODERN BUTTON PICTURES", juiced::modern_buttons),
      FLAG("SKIP INTRO MOVIES", juiced::skip_intro),
      FLAG("KEYBOARD PLAY", app::Get().keyboard),
      {"FIELD OF VIEW", [] { return std::to_string(juiced::fov_percent) + "%"; },
       [](int d) {
           juiced::fov_percent = uint32_t(std::clamp(int(juiced::fov_percent) + d * 5, 50, 150));
           app::Save();
       }},
      FLAG("SPEED BLUR", juiced::speed_blur),
      {"ENGINE AND EFFECTS LEVEL", [] { return std::to_string(juiced::effects_percent) + "%"; },
       [](int d) {
           juiced::effects_percent = uint32_t(std::clamp(int(juiced::effects_percent) + d * 10, 0, 100));
           app::Save();
       }}}},
    // (rows without a label are hidden)
    {"SYSTEM",
     {{"SCALING FILTER", [] { return std::string(app::Get().scale_filter ? "SHARP" : "BILINEAR"); },
       [](int) {
           auto& s = app::Get();
           s.scale_filter = s.scale_filter ? 0 : 1;
           app::Save();
       }},
      FLAG("LOW LATENCY (NEXT START)", app::Get().low_latency),
      {"RENDERER (NEXT START)", [] { return std::string(app::Get().renderer % 2 ? "DIRECT3D 11" : "DIRECT3D 12"); },
       [](int) {
           auto& s = app::Get();
           s.renderer = (s.renderer + 1) % 2;
           app::Save();
       }},
      FLAG("SMOOTH MOTION ABOVE 60", juiced::smooth_motion)}},
};
constexpr int kPageCount = int(sizeof(kPages) / sizeof(kPages[0]));

// ---- the window

// the rows top to bottom: the button, and its value text where the window has one (the first two have meters:
// their value goes into the button's caption)
struct Slot {
    const char* button;
    const char* value;
};
const Slot kSlots[kRows] = {{"BRIGHT_BUT", nullptr},  {"DETAIL_BUT", nullptr}, {"SHADOWS_BUT", "SHADOWS"}, {"REFLECTION_BUT", "REFLECTION"},
                            {"MOBLUR_BUT", "MOBLUR"}, {"GLOW_BUT", "GLOW"},    {"DOF_BUT", "DOF"}};

bool g_ours = false;   // the Xbox Live page's object is showing our window
uint32_t g_window = 0;
int g_page = 0;

bool Named(uint32_t gadget, const char* name) {
    const char* n = static_cast<const char*>(HostPtr(gadget + kGadgetName));
    for (; *n && *name; n++, name++)
        if ((*n | 0x20) != (*name | 0x20)) return false;
    return !*n && !*name;
}

template <typename F>
void EachGadget(uint32_t window, F f) {
    for (uint32_t p = R32(window + kGadgets), end = R32(window + kGadgetsEnd); p && p < end; p += 4) f(R32(p));
}

uint32_t Find(uint32_t window, const char* name) {
    uint32_t found = 0;
    EachGadget(window, [&](uint32_t g) {
        if (!found && Named(g, name)) found = g;
    });
    return found;
}

// UTF-16 text in guest memory, for the calls that copy it
uint32_t Wide(const std::string& text) {
    static uint32_t buffer = 0;
    constexpr uint32_t kChars = 96;
    if (!buffer) buffer = mem::AllocSystem(kChars * 2, 16);
    uint32_t n = std::min<uint32_t>(uint32_t(text.size()), kChars - 1);
    for (uint32_t i = 0; i < n; i++) W16(buffer + i * 2, uint8_t(text[i]));
    W16(buffer + n * 2, 0);
    return buffer;
}

// as the game's pages do after 0x195390: the caption's ticker starts again
void SetCaption(X86Context& ctx, uint32_t gadget, const std::string& text) {
    if (!gadget) return;
    X86Context saved = ctx;
    ctx.ecx = gadget + kGadgetCaption;
    ctx.eax = Wide(text);
    CallGuest(ctx, kSetCaption, {});
    ctx = saved;
    if (uint32_t t = R32(gadget + kGadgetTicker)) {
        W32(t + 8, R32(t + 4));
        W32(t + 0x20, 0);
        W32(t + 0x1C, 0);
        W32(t + 0x18, 1);
        W32(t + 0x24, 0x3FC00000);  // 1.5
    }
}

// a gadget's top in the window's 480 lines (its rectangle at +0x8C: x, y, width, height as fractions of the screen)
float TopOf(uint32_t gadget) {
    float y;
    std::memcpy(&y, HostPtr(gadget + 0x8C + 4), 4);
    return y * 480;
}

// a page with fewer rows than the window has: everything in the unused rows' bands (the button, its value and
// arrows) is hidden, and shown again when the page turns
void HideUnusedRows(const Page& page) {
    static std::vector<uint32_t> hidden;
    for (uint32_t g : hidden) W32(g + kGadgetHidden, 0);
    hidden.clear();
    const uint32_t next_page = Find(g_window, "RESET_BUT");
    for (int i = 0; i < kRows; i++) {
        if (page.rows[i].label) continue;
        const uint32_t button = Find(g_window, kSlots[i].button);
        const uint32_t below = i + 1 < kRows ? Find(g_window, kSlots[i + 1].button) : next_page;
        if (!button || !below) continue;
        const float top = TopOf(button) - 2, bottom = TopOf(below) - 2;
        EachGadget(g_window, [&](uint32_t g) {
            if (R32(g + kGadgetHidden) || TopOf(g) < top || TopOf(g) >= bottom) return;
            W32(g + kGadgetHidden, 1);
            hidden.push_back(g);
        });
    }
}

void Show(X86Context& ctx) {
    const Page& page = kPages[g_page];
    HideUnusedRows(page);
    for (int i = 0; i < kRows; i++) {
        const Row& row = page.rows[i];
        if (!row.label) continue;
        if (kSlots[i].value) {
            SetCaption(ctx, Find(g_window, kSlots[i].button), row.label);
            SetCaption(ctx, Find(g_window, kSlots[i].value), row.text());
        } else {
            SetCaption(ctx, Find(g_window, kSlots[i].button), std::string(row.label) + ":  " + row.text());
        }
    }
    SetCaption(ctx, Find(g_window, "TITLE"),
               std::string("PC SETTINGS - ") + page.name + " (" + std::to_string(g_page + 1) + "/" + std::to_string(kPageCount) + ")");
    SetCaption(ctx, Find(g_window, "RESET_BUT"), "NEXT PAGE");
    SetCaption(ctx, Find(g_window, "DONE_BUT"), "DONE");
}

// back to the Options menu, as the game's pages go (0x98F38): the front end's or the pause menu's
void Leave(uint32_t manager) {
    if (R32(manager + kTransition)) return;
    const char* to = R32(R32(kGame) + 0x3C) ? "opts_ingame_menu" : "opts_menu";
    W32(manager + kTransition, 7);
    std::memcpy(HostPtr(manager + kTransitionName), to, std::strlen(to) + 1);
}

int RowOf(uint32_t gadget) {
    for (int i = 0; i < kRows; i++)
        if (gadget && Named(gadget, kSlots[i].button)) return i;
    return -1;
}

}  // namespace

// at the end of the two functions registering the options pages (0x99301 the front end's, 0x9939D the pause
// menu's; esi the UI manager): the graphics window of the disc's ui.dat, with the Xbox Live page's object
void RegisterPcPage(X86Context& ctx, uint8_t* /*base*/) {
    X86Context saved = ctx;
    const uint32_t mgr = ctx.esi;
    CallGuest(ctx, kRegisterPage, {mgr, kWindowName, kLivePage});
    ctx.eax = mgr;
    ctx.edi = kWindowName;
    uint32_t win = CallGuest(ctx, kLoadWindow, {});
    LOGI("juiced: PC settings window registered with manager %08X: window %08X", mgr, win);
    ctx = saved;
}

// at 0x9539B, the end of the Options menu's open (ebp the page: +0x10 its menu, +0x14 the rows' backdrops), after
// the Xbox build has added its "xbox_live" item: ours, above DONE, the same way
void AddPcItem(X86Context& ctx, uint8_t* /*base*/) {
    const uint32_t menu = R32(ctx.ebp + 0x10), back = R32(ctx.ebp + 0x14);
    if (!menu) return;
    static uint32_t caption = 0, name = 0;
    if (!caption) {
        // a caption as the menu takes it: begin, end, and the end of its room
        static const char16_t text[] = u"PC SETTINGS";
        constexpr uint32_t kBytes = sizeof(text);
        uint32_t chars = mem::AllocSystem(kBytes, 16);
        std::memcpy(HostPtr(chars), text, kBytes);
        caption = mem::AllocSystem(16, 16);
        W32(caption, chars);
        W32(caption + 4, chars + kBytes - 2);
        W32(caption + 8, 0);
        W32(caption + 12, chars + kBytes);
        name = mem::AllocSystem(16, 16);
        std::memcpy(HostPtr(name), "pc_settings", 12);
    }
    X86Context saved = ctx;
    const uint32_t count = (R32(menu + 0x120) - R32(menu + 0x11C)) / 4;
    CallGuest(ctx, kAddItem, {menu, caption, 0, name, 0, count ? count - 1 : 0});
    if (back) {
        uint32_t item = CallGuest(ctx, kAddTextured, {back, 0, kBackdrop, 0, 0, 0, 0, 0xFFFFFFFFu});
        if (item) W32(item + 0x54, 1);
    }
    ctx = saved;
}

// at 0x95736 in the Options menu's message function, where it compares the chosen item's name (esi) with "graph"
// (ebp the message): ours opens the PC settings window as that item would have; true goes on to 0x95A1D as the
// other items do
bool ChoosePcItem(X86Context& ctx, uint8_t* /*base*/) {
    const char* name = static_cast<const char*>(HostPtr(ctx.esi));
    if (std::strcmp(name, "pc_settings") != 0) return false;
    const uint32_t mgr = R32(ctx.ebp + kMsgManager);
    if (!R32(mgr + kTransition)) {
        W32(mgr + kTransition, 7);
        std::memcpy(HostPtr(mgr + kTransitionName), HostPtr(kWindowName), 14);
    }
    return true;
}

// the Xbox Live options page's open (thiscall page, window; ret 4): our window's rows, or the game's own
X86_EXTERN_FUNC(__imp__sub_00098D90);
X86_FUNC(sub_00098D90) {
    const uint32_t window = R32(ctx.esp + 4);
    g_ours = std::strcmp(static_cast<const char*>(HostPtr(window)), static_cast<const char*>(HostPtr(kWindowName))) == 0;
    __imp__sub_00098D90(ctx, base);
    if (!g_ours) return;
    g_window = window;
    g_page = 0;
    // the meters and their signs are not used: the first two rows' values are in their captions
    EachGadget(window, [](uint32_t g) {
        if (Named(g, "BRIGHT") || Named(g, "DETAIL") || Named(g, "MINUSSIGN1") || Named(g, "PLUSSIGN1")) W32(g + kGadgetHidden, 1);
    });
    Show(ctx);
    LOGI("juiced: the PC settings page opened");
}

// its message function (thiscall page, message; ret 4): left, right and A change the highlighted row, RESET turns
// the page, DONE and B leave
X86_EXTERN_FUNC(__imp__sub_00098E30);
X86_FUNC(sub_00098E30) {
    if (!g_ours) {
        __imp__sub_00098E30(ctx, base);
        return;
    }
    const uint32_t msg = R32(ctx.esp + 4), type = R32(msg + kMsgType), gadget = R32(msg + kMsgGadget);
    const uint32_t mgr = R32(msg + kMsgManager);
    const int row = RowOf(gadget);
    if (type == kBack) {
        Leave(mgr);
    } else if (type == kSelect && gadget && Named(gadget, "DONE_BUT")) {
        Leave(mgr);
    } else if (type == kSelect && gadget && Named(gadget, "RESET_BUT")) {
        g_page = (g_page + 1) % kPageCount;
        Show(ctx);
    } else if ((type == kLeft || type == kRight || type == kSelect) && row >= 0 && kPages[g_page].rows[row].label) {
        kPages[g_page].rows[row].step(type == kLeft ? -1 : 1);
        Show(ctx);
    }
    ctx.eax = 1;
    ctx.esp += 8;
}
