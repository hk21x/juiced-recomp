#!/usr/bin/env python3
# Soak pad script for Juiced (scripts/game.env SOAK_PAD): soak.py SECONDS [SEED]
# A first boot to race 1 (scripts/pad/race.txt's presses), then until SECONDS: about 50 seconds of driving at random
# (the throttle with steering, braking, nitrous, gear and view changes), then the pause menu's Restart (START, DOWN, A,
# UP to YES, A) and again. Times are vertical blanks at 60 a second.
import random
import sys

secs = int(sys.argv[1]) if len(sys.argv) > 1 else 600
rng = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
end = secs * 60
lines = ["# the movies skipped, the title, a new profile saved, Arcade, Novice series, race 1"]
lines += [f"{t} START" for t in range(600, 3001, 300)]
lines += ["3900 A", "4300 DOWN", "4400 A", "4800 A", "5200 A"]
t = 6400
while t < end - 600:
    lines.append("# race")
    stop = min(t + rng.randint(2400, 3600), end - 600)
    while t < stop:
        r = rng.random()
        d = rng.randint(20, 150)
        if r < 0.55:
            move = "RT"
        elif r < 0.75:
            move = "RT+" + rng.choice(["LSL", "LSR"])
            d = rng.randint(10, 50)
        elif r < 0.82:
            move = "LT"
            d = rng.randint(10, 40)
        elif r < 0.88:
            move = "RT+Y"  # nitrous
        elif r < 0.93:
            move = "RT+" + rng.choice(["B", "X"])  # gears
            d = 8
        else:
            move = "RT+WHITE"  # the view
            d = 8
        lines.append(f"{t} {move} {d}")
        t += d + 2
    # pause, Restart, YES
    lines += [f"{t + 30} START", f"{t + 120} DOWN", f"{t + 180} A", f"{t + 270} UP", f"{t + 330} A"]
    t += 900
print("\n".join(lines))
