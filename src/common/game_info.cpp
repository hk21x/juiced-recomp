#include <cmath>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

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
// In a race with the frame cap over 60: frames between the game's steps (sdk gpu/interp.cpp). The game times its
// simulation by the time between frames, but the car drives differently at another rate (at 120 frames a second it
// accelerated about 15% harder), so the game stays at 60 and the frames between are drawn from the last two steps.
bool smooth_motion = true;
// the game object's +0x3C is set in a race (src/common/pointer.cpp)
bool InRace() {
    const uint32_t game = kernel::R32(0x372B54);
    return game >= 0x10000 && mem::IsCommitted(game + 0x3C) && kernel::R32(game + 0x3C) != 0;
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
void Pad(int port, input::Pad& pad) {
    if (port != 0) return;
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

// after the settings load: JUICED_MODERN_BUTTONS and JUICED_SKIP_INTRO (1 or 0) override the settings, for the
// headless harness
void Init() {
    if (const char* e = std::getenv("JUICED_MODERN_BUTTONS"); e && *e) modern_buttons = *e == '1';
    if (const char* e = std::getenv("JUICED_SKIP_INTRO"); e && *e) skip_intro = *e == '1';
    if (skip_intro) LOGR("juiced: the logo and intro movies are skipped (skip_intro)");
    if (const char* e = std::getenv("JUICED_FOV"); e && *e) fov_percent = uint32_t(std::atoi(e));
    fov_percent = std::clamp<uint32_t>(fov_percent, 50, 150);
    if (const char* e = std::getenv("JUICED_EFFECTS"); e && *e) effects_percent = uint32_t(std::atoi(e));
}

// GameInfo::hide_file, used to see the files opened: no file is hidden. While one of the boot's movies is the last
// movie opened (D:\\movies\\THQ.bik, juicegames.bik and the intro, JuicedCGI_NTSC.bik or _PAL), intro_movie is set; with
// skip_intro START is pressed through it (Pad)
bool HideFile(const std::string& path) {
    std::string n;
    for (char c : path) n += char(std::tolower(static_cast<unsigned char>(c)));
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
                {"smooth_motion", &juiced::smooth_motion, nullptr, "Smooth motion above 60",
                 "With a frame cap over 60, races are drawn at that rate: the frames between the game's own 60 are made "
                 "from the two before. The game itself stays at 60, as its driving needs."},
                {"effects_percent", nullptr, &juiced::effects_percent, "Engine and effects level (%)",
                 "0 to 100; 100 is the disc's mix, which is far louder in a race than in the menus and clips. Music is not changed."}},
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = juiced::Init,                 // after the settings load: the harness's overrides
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = nullptr,                   // window messages first; true takes one
    .ui = nullptr,                        // ImGui drawing each frame
    .pad = juiced::Pad,                   // RB plays White (the view), LB Black
    .rumble = nullptr,                    // changes a rumble before it is set
    .hud_aspect = 4.0f / 3.0f,            // the 2D is laid out for 4:3 in both of the game's modes: squeezed to it
    .sample_scaled_viewport = true,       // the glow materials' draws (4X asked on the scene target) land where the rest do
    .hide_file = juiced::HideFile,        // hides nothing: notes when a boot movie is opened (skip_intro)
    .default_keys = juiced::Keys,         // the keyboard's defaults, after the game's own layout
    .texture_bound = juiced::ButtonPictures,  // today's pad in the button pictures (src/common/buttons.cpp)
    .step_interpolation = [] { return juiced::smooth_motion && juiced::InRace(); },  // frames between steps, in a race
    .interp_transform = 18,               // its programs' world-view-projection: c18 to c21 (c0 to c2 the world, c7 to c13 view and projection)
    // the constants that move smoothly from step to step, blended: the matrices (c0 to c17), the camera's place and axes,
    // the lights' colours, the clock; the rest (c36, c41, c42, c88, c89, c95: animation frames, tints, toggles) are the
    // step's own (logged in a race, 2026-11-14). c33 is both: it creeps on the buildings' program (1.2061 to 1.2063 a step;
    // held, the buildings popped at each step) and jumps between the corner boards' arrow frames (blended, they flickered
    // twice as fast; Harry, 2026-11-15): blended only when it moves less than a hundredth
    .interp_blends_constant = [](int c, float was, float now) {
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
    .interp_skip_still_targets = true,    // its cube faces and small passes drawn once a step, not in every in-between frame  // its buildings shook a little with any of their draws blended from its own match
    .auto_fills_screen = true,            // the scene cameras follow the frame's shape past 16:9 (src/game/fov.cpp)
    .voice_3d_level = [] { return float(std::min<uint32_t>(juiced::effects_percent, 100)) / 100.0f; },  // engines and effects
    .squeeze_flat_programs = true,        // the 2D's programs: (v0 + c) * c + c, no matrix
    .line_width = 0.35f,                  // the overhead wires are line strips: fine, not a console pixel wide
    // optional, after these: .achievements (conditions on guest memory), .same_code (other releases' xbe SHA-1s with
    // the same code), .wheel (what a steering wheel's force feedback follows)
};
