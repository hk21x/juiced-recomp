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

namespace juiced {
extern bool modern_buttons;  // src/common/buttons.cpp
bool skip_intro = false;      // no logo and intro movies at the start (Pad presses START through them)
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
                 "Starts without the THQ and Juice Games logos and the intro movie. Takes effect at the next start."}},
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = juiced::Init,                 // after the settings load: the harness's overrides
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = nullptr,                   // window messages first; true takes one
    .ui = nullptr,                        // ImGui drawing each frame
    .pad = juiced::Pad,                   // RB plays White (the view), LB Black
    .rumble = nullptr,                    // changes a rumble before it is set
    .hide_file = juiced::HideFile,        // hides nothing: notes when a boot movie is opened (skip_intro)
    .default_keys = juiced::Keys,         // the keyboard's defaults, after the game's own layout
    .texture_bound = juiced::ButtonPictures,  // today's pad in the button pictures (src/common/buttons.cpp)
    // optional, after these: .achievements (conditions on guest memory), .same_code (other releases' xbe SHA-1s with
    // the same code), .wheel (what a steering wheel's force feedback follows)
};
