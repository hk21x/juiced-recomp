#include <cmath>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace juiced {
extern bool modern_buttons;  // src/common/buttons.cpp
void MenuPointer(input::Pad& pad);  // src/common/pointer.cpp
bool skip_intro = false;      // no logo and intro movies at the start (Pad presses START through them)
bool speed_blur = true;       // the race's speed blur (src/game/blur.cpp)
// the race's HUD at the sides of a frame wider than 4:3; off, in the centred 4:3 part (src/game/widescreen.cpp)
bool hud_sides = true;
// the race's HUD left undrawn (src/game/hud.cpp); F9 in a race switches it
bool hide_hud = false;
// LT at a standstill reverses (as modern racing games): the game's own reverse is holding X (Options > Controls,
// option 1: "gear down / reverse"), so while LT is held from below kReverseSpeed the pad gives X instead (Pad)
bool brake_reverse = true;
// In a race with the frame cap over 60: frames between the game's steps (sdk gpu/interp.cpp). The game times its
// simulation by the time between frames, but the car drives differently at another rate (at 120 frames a second it
// accelerated about 15% harder), so the game stays at 60 and the frames between are drawn from the last two steps.
// Off by default and labelled experimental: frames between steps judder or misdraw in some events (Showoff's trail;
// Harry, 2026-11-19), so a player opts in
bool smooth_motion = false;
std::atomic<bool> menus_narrowed{false};  // the frame is 4:3 for the menus on a wider setting (GameInfo::frame_aspect)
// the game object's +0x3C is set in a race (src/common/pointer.cpp)
bool InRace() {
    const uint32_t game = kernel::R32(0x372B54);
    return game >= 0x10000 && mem::IsCommitted(game + 0x3C) && kernel::R32(game + 0x3C) != 0;
}
// Showoff's HUD ("hud_cruise", src/game/hud.cpp) was drawn since the race began: no frames between steps there. Its
// trail (each frame blends the one before over the new one) and the car's draws judder and jump with them, smooth with
// them off (Harry, 2026-11-19), as at 60 on the console. Cleared out of a race
std::atomic<bool> showoff{false};
bool SmoothMotionNow() {
    if (!InRace()) {
        showoff = false;
        return false;
    }
    return smooth_motion && !showoff.load();
}
uint32_t fov_percent = 100;   // the race camera's field of view (src/game/fov.cpp)
// the level of the engines and effects (the game's 3D DirectSound voices: not the music, the menus' sounds or the
// movies), percent. 50 is 6 dB under the disc's mix as mixed here, where a race came out about 15 dB over the menus
// and clipped (FINDINGS, Sound); a recording of the game played has it 6 to 10 dB over
uint32_t effects_percent = 50;
std::atomic<bool> intro_movie{false};  // the last movie opened is one of the boot's (HideFile)
namespace {
// keyboard (Windows virtual key codes: this file builds for the headless harness too), for the game's default layout
// (Options > Controls, option 1): RT throttle, LT brake, the left stick steers, A parking brake, B gear up, X gear down
// and reverse, Y nitrous, White the view, the right stick glances (its click the horn), Start pauses, Back driver info.
// The menus take Enter and Backspace as A and B, the arrows and W A S D move.
constexpr uint16_t kBackspace = 0x08, kTab = 0x09, kEnter = 0x0D, kEscape = 0x1B, kSpace = 0x20,
                   kArrowLeft = 0x25, kArrowUp = 0x26, kArrowRight = 0x27, kArrowDown = 0x28, kLeftShift = 0xA0;
std::array<uint16_t, 2> Keys(int c) {
    using namespace input;
    switch (c) {
        case kRT: return {'W', 0};                // throttle
        case kLT: return {'S', 0};                // brake
        case kLSLeft: return {'A', 0};            // steer
        case kLSRight: return {'D', 0};
        case kLSUp: return {0, 0};                // the menus' up and down are the arrows (W and S drive)
        case kLSDown: return {0, 0};
        case kA: return {kSpace, kEnter};         // parking brake; Enter selects in the menus
        case kB: return {'E', kBackspace};        // gear up; Backspace goes back in the menus
        case kX: return {'Q', 0};                 // gear down, reverse
        case kY: return {kLeftShift, 'N'};        // nitrous
        case kBlack: return {'C', 0};             // change view (White: the pad swaps them, Pad below)
        case kWhite: return {0, 0};
        case kRSLeft: return {'J', 0};            // glance left, right and back
        case kRSRight: return {'L', 0};
        case kRSDown: return {'K', 0};
        case kRSUp: return {'I', 0};
        case kRThumb: return {'H', 0};            // horn
        case kLThumb: return {'F', 0};
        case kUp: return {kArrowUp, 0};
        case kDown: return {kArrowDown, 0};
        case kLeft: return {kArrowLeft, 0};
        case kRight: return {kArrowRight, 0};
        case kStart: return {kEscape, 'P'};       // pause
        case kBack: return {kTab, 0};             // driver info
        default: return {0, 0};
    }
}

// Today's pad has RB and LB where White and Black were (the SDK reads RB as Black, LB as White). The game's change
// of view is White and Black does nothing in any of its three layouts, so they swap: RB changes the view, as the
// controls page's modern pad shows (src/common/buttons.cpp). The keyboard's C plays Black, so it does too.
// JUICED_PAD_FILE=path (tests, on Windows too: GPU-VM's injected keys do not drive the car): the controls the file
// names held while it names them, read again every 6 polls; words A B X Y LT RT UP DOWN LEFT RIGHT START BACK LSL LSR
void PadFile(input::Pad& pad) {
    static const char* path = std::getenv("JUICED_PAD_FILE");
    if (!path || !*path) return;
    static int every = 0;
    static uint16_t buttons = 0;
    static uint8_t analog[8] = {};
    static int16_t lx = 0;
    if (every++ % 6 == 0) {
        buttons = 0, lx = 0;
        std::fill(std::begin(analog), std::end(analog), uint8_t(0));
        if (std::FILE* f = std::fopen(path, "r")) {
            char w[16];
            while (std::fscanf(f, "%15s", w) == 1) {
                const std::string k = w;
                static const std::pair<const char*, int> kNames[] = {
                    {"UP", input::kUp},       {"DOWN", input::kDown}, {"LEFT", input::kLeft}, {"RIGHT", input::kRight},
                    {"START", input::kStart}, {"BACK", input::kBack}, {"A", input::kA},       {"B", input::kB},
                    {"X", input::kX},         {"Y", input::kY},       {"LT", input::kLT},     {"RT", input::kRT}};
                for (const auto& [name, c] : kNames)
                    if (k == name) {
                        if (c < input::kA) buttons |= uint16_t(1u << c);
                        else analog[c - input::kA] = 255;
                    }
                if (k == "LSL") lx = -32767;
                if (k == "LSR") lx = 32767;
            }
            std::fclose(f);
        }
    }
    pad.buttons |= buttons;
    for (int i = 0; i < 8; i++) pad.analog[i] = std::max(pad.analog[i], analog[i]);
    if (lx) pad.lx = lx;
}

// the player's car's speed in m/s: the race's first car (game +0x34, +0x10: its cars from +8 to +0xC), its physics
// (+0x168) velocity at +0x198 to +0x1A0 (as 0xC3B50 reads it). A headless race: 0 on the grid, 33 driving, -17
// reversing (the z part). Negative when there is no race or no car
float PlayerSpeed() {
    using kernel::R32;
    const auto ok = [](uint32_t va, uint32_t n) { return va >= 0x10000 && mem::IsCommitted(va) && mem::IsCommitted(va + n - 1); };
    const uint32_t game = R32(0x372B54);
    if (!ok(game, 0x40) || !ok(R32(game + 0x34), 0x14)) return -1;
    const uint32_t list = R32(R32(game + 0x34) + 0x10);
    if (!ok(list, 0x10) || R32(list + 0xC) <= R32(list + 8) || !ok(R32(list + 8), 4)) return -1;
    const uint32_t car = R32(R32(list + 8));
    if (!ok(car, 0x16C) || !ok(R32(car + 0x168), 0x1A4)) return -1;
    const uint32_t ph = R32(car + 0x168);
    float v[3];
    for (int i = 0; i < 3; i++) {
        const uint32_t u = R32(ph + 0x198 + 4 * i);
        std::memcpy(&v[i], &u, 4);
    }
    const float s = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return std::isfinite(s) ? s : -1;
}

// brake_reverse: LT pressed below this speed reverses, and keeps reversing while it stays held (about 2 mph)
constexpr float kReverseSpeed = 1.0f;
void BrakeReverse(input::Pad& pad) {
    static bool reversing = false;
    constexpr int kLTSlot = input::kLT - input::kA, kXSlot = input::kX - input::kA, kRTSlot = input::kRT - input::kA;
    const bool lt = pad.analog[kLTSlot] > 40, rt = pad.analog[kRTSlot] > 40;
    if (!brake_reverse || pad.wheel || !lt || rt || !InRace()) {
        reversing = false;
        return;
    }
    if (!reversing) {
        const float speed = PlayerSpeed();
        reversing = speed >= 0 && speed < kReverseSpeed;
    }
    if (reversing) {
        pad.analog[kXSlot] = std::max(pad.analog[kXSlot], pad.analog[kLTSlot]);
        pad.analog[kLTSlot] = 0;
    }
}

void Pad(int port, input::Pad& pad) {
    if (port != 0) return;
    PadFile(pad);
    BrakeReverse(pad);
    constexpr int kBlackSlot = 4, kWhiteSlot = 5;  // Pad::analog: A B X Y Black White LT RT
    std::swap(pad.analog[kBlackSlot], pad.analog[kWhiteSlot]);
    MenuPointer(pad);  // the mouse in the menus (src/common/pointer.cpp)
    // skip_intro: while one of the boot's movies is the last opened (HideFile), START is pressed for 3 polls in every
    // 24, as the player would to skip it; the title's movie (hub_01.bik) ends it before the title takes a press
    if (skip_intro && intro_movie.load()) {
        static uint32_t polls = 0;
        if (polls++ % 24 < 3) pad.buttons |= 1u << input::kStart;
    }
}

// F9 (not a repeat) in a race: the HUD hidden or shown, and saved (WM_KEYDOWN; the virtual key in wp, the repeat
// flag bit 30 of lp; numbers, as this file builds for the headless harness too)
bool Message(void* /*hwnd*/, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* /*result*/) {
    constexpr uint32_t kKeyDown = 0x100;
    constexpr uintptr_t kF9 = 0x78;
    if (msg != kKeyDown || wp != kF9 || (lp & (1 << 30)) || !InRace()) return false;
    hide_hud = !hide_hud;
    app::Save();
    LOGI("juiced: the HUD %s (F9)", hide_hud ? "hidden" : "shown");
    return true;
}

// after the settings load: JUICED_MODERN_BUTTONS and JUICED_SKIP_INTRO (1 or 0) override the settings, for the
// headless harness
void Init() {
    if (const char* e = std::getenv("JUICED_MODERN_BUTTONS"); e && *e) modern_buttons = *e == '1';
    if (const char* e = std::getenv("JUICED_SKIP_INTRO"); e && *e) skip_intro = *e == '1';
    if (const char* e = std::getenv("JUICED_HIDE_HUD"); e && *e) hide_hud = *e == '1';
    if (skip_intro) LOGR("juiced: the logo and intro movies are skipped (skip_intro)");
    if (const char* e = std::getenv("JUICED_FOV"); e && *e) fov_percent = uint32_t(std::atoi(e));
    fov_percent = std::clamp<uint32_t>(fov_percent, 50, 150);
    if (const char* e = std::getenv("JUICED_EFFECTS"); e && *e) effects_percent = uint32_t(std::atoi(e));
    for (const achievements::Def& d : app::kGame.achievements)
        if (std::string e = achievements::Check(d.condition); !e.empty()) LOGE("juiced: achievement %s: %s", d.id, e.c_str());
}

// GameInfo::hide_file. D:\\filelist.txt, the list of files the game opens (0x110D90), is hidden: its creation refused
// as on the console's read-only disc, it no longer lands in the assets folder. The files opened are seen too: while one
// of the boot's movies is the last movie opened (D:\\movies\\THQ.bik, juicegames.bik and the intro, JuicedCGI_NTSC.bik
// or _PAL), intro_movie is set; with skip_intro START is pressed through it (Pad)
bool HideFile(const std::string& path) {
    std::string n;
    for (char c : path) n += char(std::tolower(static_cast<unsigned char>(c)));
    if (constexpr std::string_view kList = "\\filelist.txt";
        n.size() >= kList.size() && n.compare(n.size() - kList.size(), kList.size(), kList) == 0)
        return true;
    if (n.size() < 4 || n.compare(n.size() - 4, 4, ".bik") != 0) return false;
    bool intro = false;
    for (const char* movie : {"\\thq.bik", "\\juicegames.bik", "\\juicedcgi_ntsc.bik", "\\juicedcgi_pal.bik"}) {
        const size_t len = std::char_traits<char>::length(movie);
        intro |= n.size() >= len && n.compare(n.size() - len, len, movie) == 0;
    }
    intro_movie = intro;
    return false;
}
}  // namespace
bool ButtonPictures(uint32_t header, uint32_t palette);  // src/common/buttons.cpp
std::vector<achievements::Def> AchievementSet();    // src/game/achievement_set.cpp
}  // namespace juiced

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"Juiced",               // window and splash
    .short_title = L"Juiced",               // message boxes
    .name = L"Juiced",                      // window classes, crash dumps, handover event
    .log = "juiced",                        // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "Juiced"}},  // each recompiled xbe and its exe, for title launches
    .options = {{"modern_buttons", &juiced::modern_buttons, nullptr, "Modern button pictures",
                 "The prompts and the controls page as today's Xbox pad shows them; with the keyboard, the keys. Off, the console's."},
                {"skip_intro", &juiced::skip_intro, nullptr, "Skip intro movies",
                 "Starts without the THQ and Juice Games logos and the intro movie. Takes effect at the next start."},
                {"fov_percent", nullptr, &juiced::fov_percent, "Field of view (%)",
                 "50 to 150; 100 is the game's. The race camera's field of view, changed at once."},
                {"speed_blur", &juiced::speed_blur, nullptr, "Speed blur",
                 "The blur towards the screen's edges at speed. Off, the race is drawn without it."},
                {"hud_sides", &juiced::hud_sides, nullptr, "HUD at the screen's sides",
                 "On a screen wider than 4:3 the race's dials, map and position sit at the screen's edges. Off, they stay "
                 "in the 4:3 centre, nearer the car. Changed at once."},
                {"hide_hud", &juiced::hide_hud, nullptr, "Hide the HUD",
                 "The race's dials, map, times and position left out, for screenshots and a clear view. F9 switches it in "
                 "a race; the pause menu still shows."},
                {"brake_reverse", &juiced::brake_reverse, nullptr, "Brake to reverse",
                 "Holding LT (or S) when the car has stopped reverses, as in modern racing games; release it to brake "
                 "again. The game's own reverse (X, or Q) still works. Not with a steering wheel."},
                {"smooth_motion", &juiced::smooth_motion, nullptr, "Experimental high FPS",
                 "Off by default. With a frame cap over 60, races are drawn at that rate: the frames between the game's "
                 "own 60 are made from the two before. Can judder or misdraw in some races; Showoff always runs at 60."},
                {"effects_percent", nullptr, &juiced::effects_percent, "Engine and effects level (%)",
                 "0 to 100; 100 is the disc's mix, which is far louder in a race than in the menus and clips. Music is not changed."}},
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = juiced::Init,                 // after the settings load: the harness's overrides
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = juiced::Message,           // F9 hides the HUD
    .ui = nullptr,                        // ImGui drawing each frame
    .pad = juiced::Pad,                   // RB plays White (the view), LB Black
    .rumble = nullptr,                    // changes a rumble before it is set
    .achievements = juiced::AchievementSet(),  // conditions on the player's race record (src/game/achievement_set.cpp)
    .hud_aspect = 4.0f / 3.0f,            // the 2D is laid out for 4:3 in both of the game's modes: squeezed to it
    .sample_scaled_viewport = true,       // the glow materials' draws (4X asked on the scene target) land where the rest do
    .hide_file = juiced::HideFile,        // hides nothing: notes when a boot movie is opened (skip_intro)
    .default_keys = juiced::Keys,         // the keyboard's defaults, after the game's own layout
    .texture_bound = juiced::ButtonPictures,  // today's pad in the button pictures (src/common/buttons.cpp)
    .step_interpolation = juiced::SmoothMotionNow,  // frames between steps, in a race but not in Showoff
    .interp_transform = 18,               // its programs' world-view-projection: c18 to c21 (c0 to c2 the world, c7 to c13 view and projection)
    .interp_view_projection = 14,         // c14 to c17: the crowd's program places its people through the world and these (dumped HLSL)
    .interp_world = 0,                    // c0 to c2: a draw whose world is the step before's is still (the buildings' shift)
    // the constants that move smoothly from step to step, blended: the matrices (c0 to c17), the camera's place and axes,
    // the lights' colours, the clock; the rest (c36, c41, c42, c88, c89, c95: animation frames, tints, toggles) are the
    // step's own (logged in a race, 2026-11-14). c33 is both: it creeps on the buildings' program (1.2061 to 1.2063 a step;
    // held, the buildings popped at each step) and jumps between the corner boards' arrow frames (blended, they flickered
    // twice as fast; Harry, 2026-11-15): blended only when it moves less than a hundredth
    .interp_blends_constant = [](int c, float was, float now) {
        // JUICED_HOLD (a test): registers held at the step's value, as "a-b,c,...", whatever the rules below say
        static const std::vector<std::pair<int, int>> hold = [] {
            std::vector<std::pair<int, int>> v;
            const char* e = std::getenv("JUICED_HOLD");
            for (const char* p = e; p && *p;) {
                char* end = nullptr;
                const int a = int(std::strtol(p, &end, 10));
                int b = a;
                if (*end == '-') b = int(std::strtol(end + 1, &end, 10));
                v.push_back({a, b});
                p = *end == ',' ? end + 1 : end;
                if (end == p && *p) break;
            }
            if (!v.empty()) LOGR("juiced: JUICED_HOLD=%s holds those registers at the step's value", e);
            return v;
        }();
        for (const auto& [a, b] : hold)
            if (c >= a && c <= b) return false;
        if (c == 33) {
            // JUICED_C33 (a test): 0 held, 1 blended, unset as below
            static const int mode = [] {
                const char* e = std::getenv("JUICED_C33");
                return e && *e ? std::atoi(e) : -1;
            }();
            if (mode >= 0) return mode == 1;
            return std::fabs(now - was) < 0.01f;
        }
        return (c >= 0 && c <= 17) || c == 28 || (c >= 30 && c <= 32) || c == 37 || c == 43 || c == 44 || c == 90 ||
               (c >= 92 && c <= 94);
    },
    .interp_scenery_takes_camera = true,
    .interp_skip_still_targets = true,    // its cube faces and small passes drawn once a step, not in every in-between frame
    .interp_fill_frames = true,           // its own drawing takes 4 ms of a step: at 240 the 4th frame fits only closer together
    .interp_rebuilt_on_car = true,        // the neon and shadow under the cars: laid on the road afresh each step
    .interp_peers_on_cars = true,         // the brake lights lit as the brakes come on: with the car's body
    .interp_shared_constants = true,      // the crowd without a match: the camera's view and projection (c7 to c13) blended
    .auto_fills_screen = true,            // the scene cameras follow the frame's shape past 16:9 (src/game/fov.cpp)
    .aspect_live = true,                  // every aspect choice at once: the game stays in its widescreen mode, fov.cpp shapes its views
    // the menus (the title, the front end, the boot's movies) are laid out for 4:3: shown so, in the middle, whatever
    // the setting, their bands, button bar and backdrop movie unstretched, with a blurred copy behind at the sides;
    // a race takes the setting's shape (Harry, 2026-11-17)
    .frame_aspect = [](float chosen) {
        const bool menus = chosen > 4.0f / 3.0f + 1e-3f && !juiced::InRace();
        juiced::menus_narrowed = menus;
        return menus ? 4.0f / 3.0f : chosen;
    },
    .bars_backdrop = [] { return juiced::menus_narrowed.load(); },
    .silent_clock = true,                 // without a sound device the race's start still waits on sounds ending
    .voice_3d_level = [] { return float(std::min<uint32_t>(juiced::effects_percent, 100)) / 100.0f; },  // engines and effects
    .squeeze_flat_programs = true,        // the 2D's programs: (v0 + c) * c + c, no matrix
    .line_width = 0.35f,                  // the overhead wires are line strips: fine, not a console pixel wide
    // optional, after these: .achievements (conditions on guest memory), .same_code (other releases' xbe SHA-1s with
    // the same code), .wheel (what a steering wheel's force feedback follows)
};
