// Juiced's achievements (ACHIEVEMENTS.md has the memory map). Conditions are in RetroAchievements' syntax
// (sdk/runtime/core/achievements.cpp), so the set can go there as it is.
//
// The game object is at [0x372B54]: +0x3C set in a race, +0x34 the race, +0x6C its setup (+4 the kind: 5 a circuit
// race, 6 a sprint). The player's car is [[[[race + 0xC]]] + 4] (the first view's car, as the results page 0x63640
// takes it) and its race record [car + 0x494]: +0xF8 the finishing time in seconds (a float), +0x104 the place now,
// +0x110 the place it finished in, +0x118 set when it has finished (a sprint: each heat). Measured headless in Arcade's
// sprint and circuit (memory dumps mid-race and on the results page, 4th in 28.8 s and 2nd in 11.4 s).
#include <deque>
#include <string>
#include <vector>

#include "core/achievements.h"

namespace juiced {
namespace {
#define JUICED_GAME "I:0xX372b54_"
#define JUICED_RECORD JUICED_GAME "I:0xX34_I:0xXc_I:0xX0_I:0xX0_I:0xX4_I:0xX494_"
// the player finished this frame, in a race, with a time a race can have (5 s to an hour: a record's memory read
// while it is being made or freed is not a finish)
#define JUICED_FINISHED                                                                                             \
    JUICED_GAME "0xX3c!=0_" JUICED_RECORD "d0xX118=0_" JUICED_RECORD "0xX118=1_" JUICED_RECORD "0xXf8>h40a00000_" \
                JUICED_RECORD "0xXf8<h45610000_" JUICED_RECORD "0xX104>=1_" JUICED_RECORD "0xX104<=8"

const char* Kept(std::string s) {
    static std::deque<std::string> kept;
    return kept.emplace_back(std::move(s)).c_str();
}
// finished at `place` or better, in a race of `kind` (0 any)
const char* Finish(unsigned place, unsigned kind, const char* also = "") {
    std::string c = JUICED_FINISHED;
    if (place) c += "_" JUICED_RECORD "0xX104<=" + std::to_string(place);
    if (kind) c += "_" JUICED_GAME "I:0xX6c_0xX4=" + std::to_string(kind);
    return Kept(c + also);
}
}  // namespace

std::vector<achievements::Def> AchievementSet() {
    return {
        {"finish_race", "Off the Line", "Finish a race", 5, Finish(0, 0)},
        {"podium", "On the Box", "Finish a race in the top three", 5, Finish(3, 0)},
        {"win_race", "Winner's Circle", "Win a race", 10, Finish(1, 0)},
        {"win_circuit", "Lap Leader", "Win a circuit race", 10, Finish(1, 5)},
        {"win_sprint", "Straight Shooter", "Win a sprint heat", 10, Finish(1, 6)},
        // (10 s as a float's bits: positive floats order as their bits do)
        {"sprint_10s", "Ten Second Car", "Finish a sprint heat in under ten seconds", 25,
         Finish(0, 6, "_" JUICED_RECORD "0xXf8<h41200000")},
    };
}
}  // namespace juiced
