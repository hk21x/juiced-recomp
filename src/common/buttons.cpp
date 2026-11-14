// The button pictures as today's Xbox pad shows them (setting modern_buttons), or, while the keyboard plays, the keys.
//
// Juiced's button pictures are 8-bit palettised textures, each with its own palette: the prompts' A, B and Y (32x32
// discs: "A OK", "B BACK" under every menu) and the controls page's pad (256x256, Options > Controls > a port: the
// original controller with the lines to its labels; the labels are text drawn over it). When one is bound
// (GameInfo::texture_bound) as the disc has it (the FNV-1a of its texels), it is drawn again: the prompts as today's
// discs (a dark disc, the letter in its colour) or the key that plays the button, the pad as today's pad with the same
// lines ending where the labels are. The new picture goes to the renderers four times larger (gpu::SetReplacement) and
// into guest memory at the texture's size through its palette (the reference renderer and the first frame read it).
// Off, the disc's texels and palette go back.
//
// JUICED_PICS_LOG=1 (a diagnostic): each 8-bit palettised texture the game binds, up to 256 square, is logged once with
// the FNV-1a of its texels, and its picture written beside the exe as pic_<hash>_<w>x<h>.bgra (B G R A rows).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/input.h"
#include "core/log.h"
#include "gpu/texture.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "strokes.h"

namespace juiced {
bool modern_buttons = true;  // the setting (src/common/game_info.cpp)

namespace {
std::mutex g_lock;
constexpr int kSharp = 4;  // the renderers' picture, times the texture's size

uint64_t Fnv(const uint8_t* p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

// ---- the pictures

constexpr Colour kDark{24, 24, 26, 255}, kRim{120, 120, 124, 255}, kWhite{255, 255, 255, 255};
constexpr Colour kGreen{110, 200, 70, 255}, kRed{235, 70, 60, 255}, kBlue{60, 140, 235, 255}, kYellow{250, 200, 30, 255};

// a dark disc with a lighter rim
void Disc(Canvas& c, float cx, float cy, float r) {
    float rim = std::max(0.6f, r * 0.12f);
    Area a{cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1};
    Fill(c, a, kRim, [&](float x, float y) { return CircleDistance(x, y, cx, cy, r); });
    Fill(c, a, kDark, [&](float x, float y) { return CircleDistance(x, y, cx, cy, r - rim); });
}

// A B X Y: the letter in the button's colour on a dark disc
void Face(Canvas& c, float cx, float cy, float r, const char* letter, Colour colour) {
    Disc(c, cx, cy, r);
    Text(c, letter, cx, cy, r * 1.05f, std::max(0.55f, r * 0.2f), colour);
}

// a key: its name on a cap (the arrows' and longer names smaller)
void Key(Canvas& c, Area a, const std::string& label) {
    float h = a.y1 - a.y0, w = a.x1 - a.x0, round = h * 0.22f, rim = std::max(0.6f, h * 0.07f);
    Fill(c, a, kRim, [&](float x, float y) { return BoxDistance(x, y, a, round, round); });
    Area in{a.x0 + rim, a.y0 + rim, a.x1 - rim, a.y1 - rim};
    Fill(c, a, kDark, [&](float x, float y) { return BoxDistance(x, y, in, round, round); });
    float height = std::min(h * 0.42f, (w - 4 * rim) * 0.9f / std::max(0.1f, TextWidth(label.c_str(), 1)));
    Text(c, label.c_str(), (a.x0 + a.x1) / 2, (a.y0 + a.y1) / 2, height, std::max(0.45f, height * 0.17f), kWhite);
}

// a key's name on its cap
std::string KeyLabel(uint16_t vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, char(vk));
    if (vk >= 0x70 && vk <= 0x7B) return "F" + std::to_string(vk - 0x70 + 1);
    switch (vk) {
        case 0x08: return "BKSP";
        case 0x09: return "TAB";
        case 0x0D: return "ENTER";
        case 0x10: case 0xA0: case 0xA1: return "SHIFT";
        case 0x11: case 0xA2: case 0xA3: return "CTRL";
        case 0x12: case 0xA4: case 0xA5: return "ALT";
        case 0x1B: return "ESC";
        case 0x20: return "SPACE";
        case 0x2E: return "DEL";
        default: return "";
    }
}

// the key shown for a control: the prompts are the menus', so Enter and Backspace (the second key) come first when the
// first is a driving key
std::string KeyFor(int control) {
    std::array<uint16_t, 2> keys = input::ControlKeys(control);
    std::string first = KeyLabel(keys[0]), second = KeyLabel(keys[1]);
    if ((control == input::kA || control == input::kB) && !second.empty()) return second;
    return first.empty() ? second : first;
}

// the stick: a dark cap, its dished top a ring lighter
void Stick(Canvas& c, float cx, float cy, float r) {
    Area a{cx - r, cy - r, cx + r, cy + r};
    float rim = std::max(0.6f, r * 0.1f);
    Fill(c, a, kRim, [&](float x, float y) { return CircleDistance(x, y, cx, cy, r); });
    Fill(c, a, kDark, [&](float x, float y) { return CircleDistance(x, y, cx, cy, r - rim); });
    constexpr Colour kRing{70, 70, 74, 255};
    Fill(c, a, kRing, [&](float x, float y) { return std::abs(CircleDistance(x, y, cx, cy, r * 0.62f)) - rim * 0.7f; });
}

// the d-pad: a cross in a dark disc
void DPad(Canvas& c, float cx, float cy, float side) {
    float k = side / 20;
    Disc(c, cx, cy, side / 2);
    float w = 2.8f * k, l = side / 2 - 2.6f * k;
    constexpr Colour kArm{100, 100, 104, 255};
    Area arms[2] = {{cx - w, cy - l, cx + w, cy + l}, {cx - l, cy - w, cx + l, cy + w}};
    for (Area a : arms) Fill(c, a, kArm, [&](float x, float y) { return BoxDistance(x, y, a, 0.8f * k, 0.8f * k); });
}

// Menu (three lines) and View (two overlapping squares) on a dark disc
void MenuButton(Canvas& c, float cx, float cy, float r) {
    Disc(c, cx, cy, r);
    float k = r / 7;
    std::vector<Segment> lines;
    for (int i = -1; i <= 1; i++) lines.push_back({{cx - 2.8f * k, cy + i * 2.6f * k}, {cx + 2.8f * k, cy + i * 2.6f * k}});
    Fill(c, Area{cx - r, cy - r, cx + r, cy + r}, kWhite, [&](float x, float y) { return StrokeDistance(x, y, lines, std::max(0.3f, 0.6f * k)); });
}

void ViewButton(Canvas& c, float cx, float cy, float r) {
    Disc(c, cx, cy, r);
    float k = r / 7;
    auto square = [&](float x0, float y0, bool front) {
        Area s{x0, y0, x0 + 4.6f * k, y0 + 4.6f * k};
        std::vector<Segment> edges = {{{s.x0, s.y0}, {s.x1, s.y0}}, {{s.x1, s.y0}, {s.x1, s.y1}},
                                      {{s.x1, s.y1}, {s.x0, s.y1}}, {{s.x0, s.y1}, {s.x0, s.y0}}};
        Area around{s.x0 - 1, s.y0 - 1, s.x1 + 1, s.y1 + 1};
        if (front) Fill(c, around, kDark, [&](float x, float y) { return BoxDistance(x, y, s, 0.4f * k, 0.4f * k); });
        Fill(c, around, kWhite, [&](float x, float y) { return StrokeDistance(x, y, edges, std::max(0.3f, 0.5f * k)); });
    };
    square(cx - 3.8f * k, cy - 3.8f * k, false);
    square(cx - 0.8f * k, cy - 0.8f * k, true);
}

// a trigger (rounder at the top) or a bumper, with its name
void Shoulder(Canvas& c, Area plate, bool trigger, const char* label) {
    float w = plate.x1 - plate.x0, h = plate.y1 - plate.y0;
    float top = trigger ? w * 0.3f : h * 0.45f, bottom = trigger ? w * 0.08f : h * 0.45f, rim = std::max(0.6f, h * 0.1f);
    Fill(c, plate, kRim, [&](float x, float y) { return BoxDistance(x, y, plate, top, bottom); });
    Fill(c, plate, Colour{44, 44, 48, 255}, [&](float x, float y) { return BoxDistance(x, y, plate, top, bottom) + rim; });
    float height = std::min(h * 0.55f, (w - 2 * rim) * 0.8f / TextWidth(label, 1));
    Text(c, label, (plate.x0 + plate.x1) / 2, (plate.y0 + plate.y1) / 2, height, std::max(0.4f, height * 0.18f), kWhite);
}

// the white rings and lines of the controls page; a line broken where another passes over it
void Ring(Canvas& c, float cx, float cy, float r) {
    Fill(c, Area{cx - r - 2, cy - r - 2, cx + r + 2, cy + r + 2}, kWhite,
         [&](float x, float y) { return std::abs(CircleDistance(x, y, cx, cy, r)) - 0.75f; });
}
// each segment over its own bounds only: the page is drawn at 1024 square for the renderers
void Lines(Canvas& c, const std::vector<Segment>& s) {
    for (const Segment& seg : s) {
        Area a{std::min(seg.a.x, seg.b.x) - 2, std::min(seg.a.y, seg.b.y) - 2, std::max(seg.a.x, seg.b.x) + 2,
               std::max(seg.a.y, seg.b.y) + 2};
        std::vector<Segment> one = {seg};
        Fill(c, a, kWhite, [&](float x, float y) { return StrokeDistance(x, y, one, 0.75f); });
    }
}
// a horizontal line from x0 to x1 at y with gaps where the vertical lines at `cross` pass
std::vector<Segment> Broken(float x0, float x1, float y, std::vector<float> cross) {
    std::sort(cross.begin(), cross.end());
    std::vector<Segment> out;
    float from = std::min(x0, x1), to = std::max(x0, x1);
    for (float x : cross) {
        if (x <= from || x >= to) continue;
        out.push_back({{from, y}, {x - 3.0f, y}});
        from = x + 3.0f;
    }
    out.push_back({{from, y}, {to, y}});
    return out;
}

// the controls page's pad, 256 square, its lines where the console's ended (the labels around it are the game's text):
// top LT x 89 and RT x 171 (from y 28), right X y 40, Y 75, B 95, A 114, the view y 162, left steering y 99, pause
// y 137, driver info y 187, the right stick's line down at x 161 to y 225
void PadPage(Canvas& c) {
    constexpr Colour kBody{30, 30, 33, 255}, kBodyEdge{78, 78, 84, 255};
    // the body: a rounded top, its lower edge between the grips higher, and two grips flaring down and out
    Area top{34, 52, 222, 132};
    Area grip_l{18, 88, 102, 214}, grip_r{154, 88, 238, 214};
    auto body = [&](float x, float y, float grow) {
        auto g = [&](Area a) { return Area{a.x0 - grow, a.y0 - grow, a.x1 + grow, a.y1 + grow}; };
        float d = BoxDistance(x, y, g(top), 30, 16);
        d = std::min(d, EllipseDistance(x, y, g(grip_l)));
        d = std::min(d, EllipseDistance(x, y, g(grip_r)));
        return d;
    };
    // triggers and bumpers above the top edge, the bumpers outside the triggers
    Shoulder(c, Area{72, 30, 106, 54}, true, "LT");
    Shoulder(c, Area{154, 30, 188, 54}, true, "RT");
    Shoulder(c, Area{34, 42, 74, 58}, false, "LB");
    Shoulder(c, Area{186, 42, 226, 58}, false, "RB");
    Area all{0, 0, 256, 256};
    Fill(c, all, kBodyEdge, [&](float x, float y) { return body(x, y, 0); });
    Fill(c, all, kBody, [&](float x, float y) { return body(x, y, -2.0f); });
    // the controls
    Stick(c, 68, 99, 15);
    DPad(c, 98, 157, 24);
    Stick(c, 161, 148, 15);
    Face(c, 195, 74, 9.5f, "Y", kYellow);
    Face(c, 172, 95, 9.5f, "X", kBlue);
    Face(c, 210, 95, 9.5f, "B", kRed);
    Face(c, 195, 114, 9.5f, "A", kGreen);
    ViewButton(c, 117, 100, 6);
    MenuButton(c, 139, 100, 6);
    Disc(c, 128, 74, 8);  // the guide button
    Fill(c, Area{118, 64, 138, 84}, Colour{180, 180, 184, 255},
         [&](float x, float y) { return std::abs(CircleDistance(x, y, 128, 74, 4.5f)) - 0.8f; });
    // rings round what the lines point at
    Ring(c, 68, 99, 20);
    Ring(c, 161, 148, 20);
    for (auto [x, y] : {std::pair{195.f, 74.f}, {172.f, 95.f}, {210.f, 95.f}, {195.f, 114.f}}) Ring(c, x, y, 11.5f);
    Ring(c, 117, 100, 8);
    Ring(c, 139, 100, 8);
    // the lines
    std::vector<Segment> s = {
        {{89, 28}, {89, 31}},    {{171, 28}, {171, 31}},                          // LT brake, RT throttle
        {{172, 84}, {172, 40}},  {{172, 40}, {256, 40}},                          // X gear down
        {{48, 99}, {16, 99}},                                                     // steering
        {{161, 168}, {161, 225}},                                                 // the right stick
        {{139, 108}, {139, 137}},                                                 // pause: Menu
        {{117, 108}, {117, 187}}, {{117, 187}, {8, 187}},                          // driver info: View
        {{232, 162}, {256, 162}}, {{232, 162}, {232, 46}}, {{232, 46}, {226, 46}},  // the view: RB
    };
    for (auto& seg : Broken(206, 256, 75, {232})) s.push_back(seg);   // Y nitrous
    for (auto& seg : Broken(221, 256, 95, {232})) s.push_back(seg);   // B gear up
    for (auto& seg : Broken(206, 256, 114, {232})) s.push_back(seg);  // A parking brake
    for (auto& seg : Broken(8, 139, 137, {117})) s.push_back(seg);    // pause
    Lines(c, s);
    // the steering arrows at the line's end
    Fill(c, Area{0, 92, 14, 106}, kWhite, [&](float x, float y) {
        float l = PolygonDistance(x, y, {{1, 99}, {6, 95}, {6, 103}});
        float r = PolygonDistance(x, y, {{13, 99}, {8, 103}, {8, 95}});
        return std::min(l, r);
    });
}

// a control's first key by name (its second when the first has none), "" unbound
std::string DrivingKey(int control) {
    std::array<uint16_t, 2> keys = input::ControlKeys(control);
    std::string first = KeyLabel(keys[0]);
    return first.empty() ? KeyLabel(keys[1]) : first;
}

// the controls page while the keyboard plays: the keys in the pad's place, each where its button's line ended, so the
// game's labels around the picture name them (the page's three layouts move the labels, not the buttons)
void KeysPage(Canvas& c) {
    constexpr float kH = 15;  // a cap's height
    auto cap = [&](float cx, float cy, const std::string& label) {
        const float w = std::max(kH + 3, TextWidth(label.c_str(), kH * 0.42f) + 9);
        Key(c, Area{cx - w / 2, cy - kH / 2, cx + w / 2, cy + kH / 2}, label.empty() ? "-" : label);
        return w / 2;
    };
    std::vector<Segment> s;
    // the triggers, above
    for (auto [x, control] : {std::pair{89.f, input::kLT}, {171.f, input::kRT}}) {
        cap(x, 48, DrivingKey(control));
        s.push_back({{x, 28}, {x, 48 - kH / 2 - 1}});
    }
    // the face buttons and the view, to the right
    for (auto [y, control] : {std::pair{40.f, input::kX}, {75.f, input::kY}, {95.f, input::kB}, {114.f, input::kA}, {162.f, input::kBlack}}) {
        const float half = cap(214, y, DrivingKey(control));
        s.push_back({{214 + half + 1, y}, {256, y}});
    }
    // steering, pause and driver info, to the left
    {
        const float a = cap(40, 99, DrivingKey(input::kLSLeft));
        cap(40 + a + 14, 99, DrivingKey(input::kLSRight));
        s.push_back({{16, 99}, {40 - a - 1, 99}});
        const float p = cap(44, 137, DrivingKey(input::kStart));
        s.push_back({{8, 137}, {44 - p - 1, 137}});
        const float i = cap(44, 187, DrivingKey(input::kBack));
        s.push_back({{8, 187}, {44 - i - 1, 187}});
    }
    // the glances (left, back, right) and under them the horn, below
    {
        cap(137, 190, DrivingKey(input::kRSLeft));
        cap(161, 190, DrivingKey(input::kRSDown));
        cap(185, 190, DrivingKey(input::kRSRight));
        cap(161, 208, DrivingKey(input::kRThumb));
        s.push_back({{161, 208 + kH / 2 + 1}, {161, 225}});
    }
    Lines(c, s);
    Fill(c, Area{0, 92, 14, 106}, kWhite, [&](float x, float y) {
        float l = PolygonDistance(x, y, {{1, 99}, {6, 95}, {6, 103}});
        float r = PolygonDistance(x, y, {{13, 99}, {8, 103}, {8, 95}});
        return std::min(l, r);
    });
    Text(c, "KEYBOARD", 128, 128, 9, 1.0f, Colour{200, 200, 204, 255});
}

// ---- the textures

struct Look {
    bool modern = false, keys = false;
    int style = input::kStyleXbox;  // with modern: the pad's labels
    std::string key;  // the key's name, with keys
    bool operator==(const Look& o) const { return modern == o.modern && keys == o.keys && style == o.style && key == o.key; }
};

enum class Kind { kFaceA, kFaceB, kFaceX, kFaceY, kPad };

// PlayStation's face buttons by place (A south cross, B east circle, X west square, Y north triangle): the shape in
// its colour on a dark disc
void Symbol(Canvas& c, float cx, float cy, float r, Kind kind) {
    Disc(c, cx, cy, r);
    const float k = r * 0.48f, w = std::max(0.55f, r * 0.11f);
    const Area a{cx - r, cy - r, cx + r, cy + r};
    std::vector<Segment> s;
    Colour colour = kWhite;
    switch (kind) {
        case Kind::kFaceA:  // cross
            colour = {120, 160, 235, 255};
            s = {{{cx - k, cy - k}, {cx + k, cy + k}}, {{cx - k, cy + k}, {cx + k, cy - k}}};
            break;
        case Kind::kFaceB:  // circle
            Fill(c, a, {235, 90, 90, 255}, [&](float x, float y) { return std::fabs(CircleDistance(x, y, cx, cy, k * 1.02f)) - w; });
            return;
        case Kind::kFaceX: {  // square
            colour = {225, 130, 200, 255};
            const float q = k * 0.92f;
            s = {{{cx - q, cy - q}, {cx + q, cy - q}}, {{cx + q, cy - q}, {cx + q, cy + q}},
                 {{cx + q, cy + q}, {cx - q, cy + q}}, {{cx - q, cy + q}, {cx - q, cy - q}}};
            break;
        }
        default: {  // triangle
            colour = {70, 200, 170, 255};
            const float t = k * 1.12f;
            const Pt top{cx, cy - t}, left{cx - t * 0.94f, cy + t * 0.62f}, right{cx + t * 0.94f, cy + t * 0.62f};
            s = {{top, right}, {right, left}, {left, top}};
            break;
        }
    }
    Fill(c, a, colour, [&](float x, float y) { return StrokeDistance(x, y, s, w); });
}
struct Known {
    uint64_t hash, palette;  // FNV-1a of its texels and of its own palette's colours
    uint32_t size;
    Kind kind;
};
// the disc's pictures (JUICED_PICS_LOG on the menus, the race and the controls page)
constexpr Known kKnown[] = {
    {0xD6B454E01B92FE99ull, 0x2E81EA7719F5C6F0ull, 32, Kind::kFaceA},
    {0x80C366933D75C91Eull, 0x834D2B39FD26AC64ull, 32, Kind::kFaceB},
    {0xBE010B0D0670D0F0ull, 0xBEDCB7A2AE053FAAull, 32, Kind::kFaceX},  // the Profile Manager's "X DELETE"
    {0x2CE618996326D735ull, 0xACC212E34F794304ull, 32, Kind::kFaceY},
    {0x158A8301606FDC3Eull, 0x03E0A61CEAFBD7DBull, 256, Kind::kPad},
};

Look Wanted(Kind kind) {
    Look l;
    if (kind == Kind::kPad && input::UsingKeyboard()) {
        // the page's keys, named so that a changed binding draws it again
        l.keys = true;
        for (int control : {input::kLT, input::kRT, input::kX, input::kY, input::kB, input::kA, input::kBlack, input::kLSLeft,
                            input::kLSRight, input::kStart, input::kBack, input::kRSLeft, input::kRSDown, input::kRSRight, input::kRThumb})
            l.key += DrivingKey(control) + ",";
        return l;
    }
    if (kind != Kind::kPad && input::UsingKeyboard()) {
        int control = kind == Kind::kFaceA ? input::kA : kind == Kind::kFaceB ? input::kB : kind == Kind::kFaceX ? input::kX : input::kY;
        l.key = KeyFor(control);
        if (!l.key.empty()) {
            l.keys = true;
            return l;
        }
    }
    l.modern = modern_buttons;
    // the prompts follow the pad played with; the controls page stays an Xbox pad
    if (l.modern && kind != Kind::kPad) l.style = input::LastPadStyle();
    return l;
}

std::vector<uint32_t> Draw(Kind kind, const Look& look, uint32_t size, int scale) {
    Canvas c;
    c.scale = scale;
    c.size = int(size) * scale;
    c.px.assign(size_t(c.size) * c.size, {0, 0, 0, 0});
    float s = float(size);
    if (kind == Kind::kPad) {
        if (look.keys) KeysPage(c);
        else PadPage(c);
    } else if (look.keys) {
        Key(c, Area{1.5f, s * 0.2f, s - 1.5f, s * 0.8f}, look.key);
    } else if (look.modern && look.style == input::kStylePlayStation) {
        Symbol(c, s / 2, s / 2, s / 2 - 2.5f, kind);
    } else if (look.modern && look.style == input::kStyleNintendo) {
        // Nintendo's labels by place: B south, A east, Y west, X north, white
        const char* letter = kind == Kind::kFaceA ? "B" : kind == Kind::kFaceB ? "A" : kind == Kind::kFaceX ? "Y" : "X";
        Face(c, s / 2, s / 2, s / 2 - 2.5f, letter, kWhite);
    } else {
        const char* letter = kind == Kind::kFaceA ? "A" : kind == Kind::kFaceB ? "B" : kind == Kind::kFaceX ? "X" : "Y";
        Colour colour = kind == Kind::kFaceA ? kGreen : kind == Kind::kFaceB ? kRed : kind == Kind::kFaceX ? kBlue : kYellow;
        Face(c, s / 2, s / 2, s / 2 - 2.5f, letter, colour);
    }
    std::vector<uint32_t> out(c.px.size());
    for (size_t i = 0; i < out.size(); i++) {
        const auto& p = c.px[i];
        float a = p[3];
        auto ch = [&](float v) { return uint32_t(std::clamp(a > 0 ? v / a : 0.0f, 0.0f, 1.0f) * 255 + 0.5f); };
        out[i] = uint32_t(std::clamp(a, 0.0f, 1.0f) * 255 + 0.5f) << 24 | ch(p[0]) << 16 | ch(p[1]) << 8 | ch(p[2]);
    }
    return out;
}

// a picture as 8-bit indices and its palette (A8R8G8B8): the most used colours at 4 bits a channel, each pixel the
// nearest of them
void Quantize(const std::vector<uint32_t>& px, std::vector<uint8_t>& index, std::array<uint32_t, 256>& palette) {
    std::unordered_map<uint32_t, uint32_t> count;
    auto key = [](uint32_t c) { return c & 0xF0F0F0F0u; };
    for (uint32_t c : px) count[key(c)]++;
    std::vector<std::pair<uint32_t, uint32_t>> by(count.begin(), count.end());
    std::sort(by.begin(), by.end(), [](auto& a, auto& b) { return a.second > b.second; });
    size_t n = std::min<size_t>(by.size(), 256);
    palette.fill(0);
    for (size_t i = 0; i < n; i++) {
        uint32_t k = by[i].first;
        palette[i] = k | (k >> 4 & 0x0F0F0F0Fu);
    }
    std::unordered_map<uint32_t, uint8_t> nearest;
    index.resize(px.size());
    for (size_t p = 0; p < px.size(); p++) {
        uint32_t k = key(px[p]);
        auto it = nearest.find(k);
        if (it == nearest.end()) {
            int best = 0;
            int64_t best_d = INT64_MAX;
            for (size_t i = 0; i < n; i++) {
                int64_t d = 0;
                for (int sh = 0; sh < 32; sh += 8) {
                    int64_t e = int64_t(palette[i] >> sh & 0xFF) - int64_t(px[p] >> sh & 0xFF);
                    d += e * e;
                }
                if (d < best_d) best_d = d, best = int(i);
            }
            it = nearest.emplace(k, uint8_t(best)).first;
        }
        index[p] = it->second;
    }
}

struct Texture {
    uint32_t data = 0, palette_data = 0, size = 0, palette_entries = 0;
    Kind kind = Kind::kFaceA;
    std::vector<uint8_t> disc_texels, disc_palette;  // as the disc has them, to put back
    uint64_t written = 0;                           // the texels' FNV-1a as last written
    Look drawn;                                     // what they show
};
std::map<uint32_t, Texture> g_textures;  // by the texels' address

void Apply(Texture& t, const Look& look) {
    auto* texels = static_cast<uint8_t*>(kernel::HostPtr(t.data));
    auto* colours = static_cast<uint8_t*>(kernel::HostPtr(t.palette_data));
    const size_t bytes = size_t(t.size) * t.size;
    if (!look.modern && !look.keys) {
        std::memcpy(texels, t.disc_texels.data(), bytes);
        std::memcpy(colours, t.disc_palette.data(), t.disc_palette.size());
        gpu::ClearReplacement(t.data);
    } else {
        std::vector<uint32_t> small = Draw(t.kind, look, t.size, 1);
        std::vector<uint8_t> index;
        std::array<uint32_t, 256> palette;
        Quantize(small, index, palette);
        for (uint32_t y = 0; y < t.size; y++)
            for (uint32_t x = 0; x < t.size; x++) texels[gpu::SwizzledOffset(x, y, t.size, t.size)] = index[size_t(y) * t.size + x];
        std::memcpy(colours, palette.data(), std::min<size_t>(t.palette_entries, 256) * 4);
        gpu::SetReplacement(t.data, uint32_t(bytes), t.size * kSharp, t.size * kSharp, Draw(t.kind, look, t.size, kSharp));
    }
    t.written = Fnv(texels, bytes);
    t.drawn = look;
    LOGI("juiced: button picture %08X (%ux%u) drawn as %s", t.data, t.size, t.size,
         look.keys ? (t.kind == Kind::kPad ? "the keyboard's keys" : look.key.c_str()) : look.modern ? "today's pad" : "the disc has it");
}

bool LogPictures() {
    static const bool on = [] {
        const char* e = std::getenv("JUICED_PICS_LOG");
        return e && *e == '1';
    }();
    return on;
}

// the palette's colours hashed (D3DPalette: Common's top two bits the size code, Data physical at +4), 0 without one
uint64_t PaletteHash(uint32_t palette) {
    if (!palette) return 0;
    const uint32_t entries = 256u >> (kernel::R32(palette) >> 30), data = mem::VirtualFromPhysical(kernel::R32(palette + 4));
    if (!mem::IsCommitted(data) || !mem::IsCommitted(data + entries * 4 - 1)) return 0;
    return Fnv(static_cast<const uint8_t*>(kernel::HostPtr(data)), entries * 4);
}

void Discover(const gpu::TextureDesc& d, uint32_t palette, uint64_t hash) {
    static std::set<std::pair<uint64_t, uint64_t>> seen;
    const uint64_t pal = PaletteHash(palette);
    if (!seen.insert({hash, pal}).second) return;
    LOGR("juiced: P8 %ux%u at %08X hash %016llX palette %08X hash %016llX", d.width, d.height, d.data, (unsigned long long)hash,
         palette, (unsigned long long)pal);
    char name[96];
    std::snprintf(name, sizeof name, "pic_%016llX_%016llX_%ux%u.bgra", (unsigned long long)hash, (unsigned long long)pal, d.width,
                  d.height);
    if (FILE* f = std::fopen(name, "wb")) {
        const auto* texels = static_cast<const uint8_t*>(kernel::HostPtr(d.data));
        const uint32_t colours = palette ? mem::VirtualFromPhysical(kernel::R32(palette + 4)) : 0;
        for (uint32_t y = 0; y < d.height; y++)
            for (uint32_t x = 0; x < d.width; x++) {
                uint8_t i = texels[gpu::SwizzledOffset(x, y, d.width, d.height)];
                uint32_t px = colours ? kernel::R32(colours + i * 4u) : 0xFF000000u | i * 0x10101u;
                std::fwrite(&px, 4, 1, f);
            }
        std::fclose(f);
    }
}
}  // namespace

// GameInfo::texture_bound
bool ButtonPictures(uint32_t header, uint32_t palette) {
    gpu::TextureDesc d;
    if (!gpu::Describe(header, d) || d.format != 0x0B || d.cube || d.depth > 1 || d.width != d.height) return false;
    const bool log = LogPictures();
    if (!log && d.width != 32 && d.width != 256) return false;
    const size_t bytes = size_t(d.width) * d.height;
    if (!mem::IsCommitted(d.data) || !mem::IsCommitted(uint32_t(d.data + bytes - 1))) return false;
    std::lock_guard<std::mutex> hold(g_lock);
    const auto* texels = static_cast<const uint8_t*>(kernel::HostPtr(d.data));
    const uint64_t hash = Fnv(texels, bytes);
    if (log) Discover(d, palette, hash);
    auto it = g_textures.find(d.data);
    if (it != g_textures.end() && it->second.written != hash) {
        g_textures.erase(it);  // the memory holds something else now
        it = g_textures.end();
    }
    if (it == g_textures.end()) {
        const Known* known = nullptr;
        for (const Known& k : kKnown)
            if (k.hash == hash && k.size == d.width) known = &k;
        // the game binds a texture before its palette: the stage's palette is still the last texture's then, and the
        // call that follows the palette's bind has the pair. Only that one is taken (the other's colours are not ours)
        if (!known || !palette || PaletteHash(palette) != known->palette) return false;
        Texture t;
        t.data = d.data;
        t.size = d.width;
        t.kind = known->kind;
        t.palette_entries = 256u >> (kernel::R32(palette) >> 30);
        t.palette_data = mem::VirtualFromPhysical(kernel::R32(palette + 4));
        if (!mem::IsCommitted(t.palette_data) || !mem::IsCommitted(t.palette_data + t.palette_entries * 4 - 1)) return false;
        t.disc_texels.assign(texels, texels + bytes);
        const auto* colours = static_cast<const uint8_t*>(kernel::HostPtr(t.palette_data));
        t.disc_palette.assign(colours, colours + t.palette_entries * 4);
        t.written = hash;
        it = g_textures.emplace(d.data, std::move(t)).first;
    }
    Look look = Wanted(it->second.kind);
    if (look == it->second.drawn) return false;
    Apply(it->second, look);
    return true;
}

}  // namespace juiced
