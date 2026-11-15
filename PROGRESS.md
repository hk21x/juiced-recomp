# Juiced recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Higher frame rates: 230 to 240 fps at a 240 cap at half render scale, 180 to 240 at full (Harry, 682ffbf on);
  the bind memo (a9b368f) and in-between frames filling the step (interp_fill_frames, Juiced on). The GPU takes
  2.2 to 2.8 ms a frame (XBE_PERF timestamps); what still holds full resolution at 180 is the swap's wait.
- Flicker with smooth motion (reversing and brake lights, neon, road streak): open, stopped after several attempts
  (log, 2026-11-15). Tools: XBE_SCREENSHOT_BURST + F12, JUICED_HOLD, XBE_INTERP_CONST_LOG, interp_table
- Smooth motion above 60 (frames between steps): arrow boards, buildings, cars and brake lights right on Windows (Harry, 5591379, 2026-11-15); scenery takes the camera's move (interp_scenery_takes_camera)
- Skip intro also skips the WARNING and legal pages (src/game/hooks.cpp; Harry, 2026-11-15)
- Harry on Windows, f6215b1 (3440x1440): "blur mostly fixed"; lines across the screen (the scene's wires, line strips a console pixel wide) and everything stretched. SDK 7c45c2e + src/game/widescreen.cpp: 4:3, 16:9 and the screen's shape, headless only so far
- Widescreen: 3D 16:9 by the game, wider by fov.cpp; 2D squeezed to a centred 4:3 by the SDK (flat programs); the race HUD and the pause screens' prompt bar anchored to the frame's sides (HudAnchor; headless only so far)
- The music freeze (Harry on Windows, 4 s into a race) is fixed in the SDK (7168ae0): not yet confirmed on Windows
- Settings (F1 window): Modern button pictures (on), Skip intro movies (off), Field of view, Speed blur (on), Engine and effects level (50%)
- PC settings in the game's menus: Options > PC SETTINGS (title menus and the pause menu), four pages of up to seven rows; headless only so far
- Field of view (Game page of PC SETTINGS, fov_percent 50 to 150), the keyboard's own controls page, the mouse in the front end's menus (point, left click A, right click B): headless only so far
- Controls: modern button pictures (setting modern_buttons, on): the prompts' A B Y and the controls page's pad; keyboard defaults (W S throttle and brake, A D steer, ...), the prompts show the keys while the keyboard plays; RB changes the view (White and Black swapped)
- Milestone: 3 - a race headless: scripts/pad/race.txt drives race 1 (Arcade, Novice, San Ricardo route 1) at full throttle; SMOKE OK (916 draws a frame). Windows: builds and boots (windows-vm, 13ff78b; WARP only, no picture there)
- XDK: 5849 with D3D8LTCG, 157 of 278 functions, 6 of 13 data (first scan); 20 LTCG argument maps proven (config/juiced.toml)
- Push buffers: none created or replayed; game code writes NV2A commands into the device's push buffer itself
- GitHub: private hk21x/juiced-recomp; SDK on fork branch `juiced` (7c45c2e: flat programs' 2D squeeze, no_squeeze, line_width; 4c5a099: the tall frame's resolve pass keeps plain taps; 0de0637: the double height frame's stretch; 9ed6b2f: the threaded backend's freed header guard; on 7168ae0 = fork main 26f372b + SetViewport(NULL), a reference renderer fix, DSOUND: buffer SetRolloffFactor hosted and a stream's ENVELOPECOMPLETE status; not yet on fork main: needs the all-games gate)

## Log

### 2026-11-15 - Higher frame rates: whole frames a step; the bind memo
- Harry's run of 74bfe9c (cap 240, render_percent 50): about 180 fps, a little neon flicker he thinks was there
  before. The log's fps sits on multiples of 60 (120.0, 180.0): the pacing draws whole frames a step, 3 now, and a
  4th needs the render thread under about 4.1 ms a frame (batches 4.26). Host draw fell 3.1 -> 2.9 ms, bind 2.1 -> 1.9;
  250 records a step left out of the replays.
- The stage memo had 0 hits in a race (2700 full binds a frame; the menus hit it): a palette left set on the stages
  turned it off, though only P8 textures read one. SDK a9b368f counts the palette only for P8 (same binds, all games).
  wincheck and SMOKE OK. Next: Harry's log, whether stages show memo hits and 240.
- Harry's two runs of 663c622 (cap 240): the memo hits (1318 last, 658 memo, 661 full a frame), bind 1.9 -> 1.2 ms,
  host 4.0 -> 3.3 ms; still 180. The render thread is no longer the limit: the pacing is. The title's own drawing
  takes 4.1 ms of the 16.7 ms step, so the frames fit in about 10.6 ms; at the 240 spacing (4.17 ms) the 4th
  would start at 12.5 ms, so it is dropped (4 planned, 3 shown).
- SDK: XBE_INTERP_FILL=1 (a test, off by default) puts the frames closer together when they would not all fit. They
  are still placed at their own times along the step. Headless at 240: 1619 frames per 10 s against 1309, no more
  late steps of ours. SMOKE OK, wincheck. Next: Harry at 240 with it, the fps and whether the motion stays even.
- Harry's run of 73e8707 with XBE_INTERP_FILL=1 (cap 240, render 100): still 180. 2 to 5 late steps of ours in
  10 s (0 without it). frame_seconds_ follows the render thread: batches 5.1 ms a frame (host 4.2: draw 2.45,
  present 1.7, swap 1.56; waiting for record 1.0). So the render thread is the limit again. Swap was 0.9 ms at
  render_percent 50 (the 17:30 run), so part of it is GPU back-pressure. FILL stays off.
  Next: render_percent 50 with the memo fix.
- Render 50% without fill: still 180, though the render thread takes 3.4 ms a frame (host 2.8, swap 0.64), so the
  pacing alone held it. Render 50% with XBE_INTERP_FILL=1: 230 to 240 fps (2321 frames for 601 steps, 0 late steps
  of ours). Harry saw no difference in jerkiness. Task Manager: no core full, GPU 10%.
- SDK: GameInfo::interp_fill_frames (default off; XBE_INTERP_FILL=0/1 overrides), Juiced on. SMOKE OK, wincheck.
  Next: full resolution. There the render thread takes 5.1 ms a frame, swap 1.56 ms of it.
- Harry, 682ffbf: half resolution 230 to 240 fps ("over 200"); full resolution (render scale 3.00) still 180, swap
  1.58 ms of a 4.2 ms render thread (draw 1.86). The swap waits on the GPU, though Task Manager shows it mostly idle.
- SDK: XBE_PERF times each submitted list on the GPU (timestamp lists sent before and after it) and logs
  "perf: GPU per frame". wincheck (it caught a member used from the free LogPerf), SMOKE OK.
- Harry: flicker on the roads and on the rear lights while driving. Captures: XBE_CAPTURE_DIR with
  XBE_CAPTURE_FRAMES=12, F3 while it flickers (consecutive frames, in-between ones among them).
- Harry's timing run of 08f7bdf: no GPU line and no failure logged (setup failed silently). SDK: each setup step logs
  its failure.
- Harry's run of 4efbf30: the timer works (100 MHz ticks). GPU per frame 2.8 ms at 180 fps, 2.2 ms in a stretch
  that held 230 to 240 at full resolution. A 240 frame has 4.17 ms, so the GPU is not the limit; the swap's wait is
  something else (presentation).
- The F3 captures (four of 12 frames) cannot show it: a captured frame takes about 0.1 s, so smooth motion stops
  and none of them is an in-between frame. They show the game swapping its 640x480 target between 83D86000 and
  83EB3000 each frame and drawing one cube face a frame; both are the game's own.
- Flicker (neon, rear and brake lights, floors): Harry, only with smooth motion on; still there with
  XBE_INTERP_SKIP_STILL=0, and with that and XBE_INTERP_FILL=0. Headless (bsn.sh: 8 frames braking) shows speckle
  and brightness changes, but they appear with smooth motion off too, so headless is no test for it.
  XBE_INTERP_CONSTANTS=0 seemed to steady the lights headless, but on the same uncertain grounds.
- Tools: SDK XBE_SCREENSHOT_BURST=N, F12 takes N frames as shown, each copied to its own read-back buffer with no
  wait, written once the last is done (screenshots\burst HH-MM-SS\NN.png). XBE_INTERP_CONST_LOG=1 logs the blended
  constants by program and register every 10 s. JUICED_HOLD="a-b,c" holds registers at the step's value (a test).
  wincheck, SMOKE OK. Next: Harry's bursts of the flicker, then JUICED_HOLD halves on Windows.
- Harry's bursts of b1e011b (7, 16 frames each, .local/win/bursts). The first, reversing: the white reversing-light
  glow sits off the lamp in some frames (13, 16 by the lamp's edge), so it is placed wrongly, not blended wrongly.
  The lamps' glow program is "still" (most of its draws go with the camera), so with interp_scenery_takes_camera
  every draw of it took the camera's move. c4e0dce saved only those with a car's exact matrix.
- SDK: a still program's draw whose own match moved like a moving program's draw (same target, same_move), not like
  the camera, takes that draw's move ("riders"; XBE_INTERP_RIDERS=0 turns it off). Headless: about 24 a step
  (14837 in 600 steps). SMOKE OK, wincheck. Next: Harry, whether the reversing, brake and neon lights hold steady.
- Harry's bursts of 235d903: the neon (20-54-37) and the road (20-54-53, 8 frames: the game was closed before the
  rest were written). By the race timer (the HUD is the step's own), the neon glow under the car is large for two
  steps, then small for three; the pale streak on the road beside the car is there for two steps and gone the
  next. Within a step they hold. So the game draws them differently from step to step: not a misplaced in-between
  frame. Guess: the glows fade by visibility tests, and with the replays' work queued ahead of a result, the title
  finds it not yet written. SDK: XBE_PERF logs how many presents a result takes to be written;
  XBE_INTERP_REPORTS=1 (a test) waits for the GPU before a step's replays when the step asked for results.
  wincheck, SMOKE OK.
- Harry's runs of 84bee25: no better with XBE_INTERP_REPORTS=1, "probably worse" (its wait made steps late: one
  burst has 7 frames of one step). The game asks for only 0.3 visibility results a frame, written 1.9 presents
  later without the switch and 0 with it. So the visibility tests are not the cause. Bursts 21-14-38/52/55.
  Next, with switches already in the build: XBE_INTERP_CONSTANTS=0 and XBE_INTERP_CAMERA_ONLY=0, one at a time.
  The flicker counts as a problem that has survived several attempts; if these settle nothing, write it up.
- Harry, with switches only (no build): XBE_INTERP_CONSTANTS=0 seemed to end the lights' and reversing lights'
  flicker, not the neon's; XBE_INTERP_CAMERA_ONLY=0 much worse. JUICED_HOLD="0-17": worse (reversing lights and
  buildings). "22-200": better. Then neither "28-33", "37-44", "90-92", "93-94" nor "28-44" helped, and on a
  longer run "22-200" flickered too, once he had reversed for long enough. So the bisect's answers were not
  repeatable: whether it flickers depends on how long and where he reverses, not on these switches alone.

### 2026-11-15 - F12's burst logs the in-between table too
- Harry: neither the lights' flicker nor the neon's is there with smooth motion off. To pair a burst with how each
  draw was placed, F12 with XBE_SCREENSHOT_BURST also logs the in-between table for the next 16 replays (no
  interp_table file to make by hand), and the table names riders. wincheck, SMOKE OK.
- Harry's bursts of 173e4b7 with the table (j4.log): one rider a step on Windows, against 24 headless; the lamps'
  glow draws were "derived" (the camera's move). A car whose move passed for the camera's (the player's, behind
  the chase camera) was left out of the cars' moves. SDK: the nearest of the camera's and the cars' moves, a car's
  only when twice as near (scenery keeps the camera's). Headless 25 to 60 riders a step. SMOKE OK, wincheck.
- The step's draw count alternates (948, 996): the game's own (cube faces and the like), as the F3 captures showed.
- README in the other ports' layout; comparison images from Harry's shots in docs/img (wipe, edges, MSAA, SMAA,
  sharpening). The FXAA and render-scale panels only looked shifted: the start gantry's board flashes.
- Harry's README screenshots are in .local/win (settings comparisons and play), for a README like Agent Under Fire's.

### Flicker with smooth motion: stopped after several attempts (2026-11-15)
What flickers, only with smooth motion: the cars' reversing and brake lights, the neon under the cars, a pale
streak on the road beside the car. Tried:
1. Bisecting the day's changes (Harry): not XBE_INTERP_SKIP_STILL, not XBE_INTERP_FILL; not the d3d12 bind memo
   (headless, with the other renderer, shows its own speckle with smooth motion off too, so headless is no test).
2. "Riders" (SDK 186800f): a glow program's draw whose own match moved like a car's takes the car's move. In the
   first F12 burst the reversing-light glow sat off the lamp in some frames. Not confirmed fixed; it still
   flickers after reversing for a while.
3. Visibility-test timing (XBE_INTERP_REPORTS): 0.3 results a frame; written at once, no change.
4. Holding constants (JUICED_HOLD bisect): answers not repeatable (above).
The neon and road changes fall on step boundaries by the race timer: the game draws them differently from step to
step, and the in-between frames show each step's way faithfully. Something in the game takes a different path
every other step or so; with smooth motion's extra frames between, that may read as flicker.
Next, if picked up: the interp_table file (a file named interp_table in the exe's folder logs the next 8 replays'
draws and how each was blended), made while reversing, with an F12 burst at the same moment. That names the
reversing-light draws and their mode (camera, rider, blend, step). Then compare with smooth motion off: an F12
burst with smooth motion off, to see whether the neon changes step to step there too.

### 2026-11-15 - Higher frame rates: measuring a replay on Windows
- The replay speed-up (c6f23f8) is back: it was not the buildings' cause. With XBE_PERF the 10 s line also gives a
  replay's time in the renderer's Draw (the threaded backend's capture: state deltas, header copies), and the d3d12
  perf lines give the title thread's recording time. Next: Harry's log with XBE_PERF=1 at the Unlimited cap.
- Harry's XBE_PERF run (cap 240, 3440x1440): 150 to 180 fps; a replay 1.5 to 1.9 ms (was 2.4 to 2.6); the title
  thread's recording 1.2 ms a frame. The render thread is what holds it: host 5 to 6 ms a frame, draw 3.1 (bind 2.1,
  consts 0.8, tables 0.3, geometry 0.25), present 1.0 to 1.5. Task Manager: GPU about half busy, no core full.
  (His render_percent 50 run still had 100 in the log.)
- SDK: GameInfo::interp_skip_still_targets (Juiced on; XBE_INTERP_SKIP_STILL=0/1): a target whose draws a replay would
  repeat (no camera move of its own, nothing sampled that was drawn this step, nothing copied in, not read between its
  draws, its depth buffer only theirs, not the shown picture) is left out of the replays; the step's picture stays.
  Juiced's cube faces (the car's reflections, now at the step's rate) and small passes: headless 33 to 38% of the
  records. 6f5e9c8 (SDK 3390bc4) did not build: a brace left over from taking out a temporary log, pushed because the
  smoke test's failure was lost in a pipe (smoke.sh | tail) and main was fast-forwarded on it. Fixed in the next
  commit; SMOKE OK (exit status checked), wincheck ok.

### 2026-11-15 - Draw distance: looked at, not changed
- Harry: lap number and pop-in seem fixed; can the draw distance be raised? The race camera's planes (projection
  0x15CB10: near camera + 0xB8, far + 0xBC): near 0.01, far 2,000,000, so the far plane is not what limits the view.
- The xbe has "Region2Object static occlusion policy" (precomputed visibility by region) and a script command
  LOD_OFF_MUL (table at 0x3304D8, handler 0x11FED0: object + 0x134 = its value); the scripts set it to 1 on many
  objects. Scaled 3x and 4x by a temporary hook: no clear change in draws or the start's picture (the flyby's camera
  differs between runs). Where + 0x134 is read for LOD was not found (the one multiply by it, 0xA91D0, is a sound's
  volume). Nothing changed.

### 2026-11-15 - Skip intro skips the WARNING and legal pages; the boards' sprites
- Harry: skip intro should skip the THQ pages at launch. Two pages before the movies, both timed, START not cutting
  them: "disclaimer" (the driving WARNING page, update 0x7A830) and "legals" (the legal page, update 0x7AAE0), screen
  classes registered in 0x7F370. Each counts down page + 0xC by the frame's time; at below 0 the first goes to legals
  and legals to the movies (legals also on a press once most of its time is gone).
- src/game/hooks.cpp: with skip_intro, both updates start with the countdown at -0.001. Headless: the title's movie
  opened at 1.7 s from launch (was 13 to 17 s), the title screen up at 4 s.
- Harry (40fc432): the corner arrow boards still broken. The scenery rule's dropped vertex count let the boards'
  arrows (small quads) take the camera's move; sprites (under kLeastVertices) are left as before.
- SMOKE OK, wincheck ok.
- Harry (d126d98, clip 12-26-44): the boards' arrows "flickering twice as fast": in the clip the chevrons switch between
  two places and a faint offset copy from one frame to the next. c33 (the boards' atlas cell) was blended again in
  a1979db for the buildings, which it did not fix (the unmatched scenery did): held again.
- Harry (d66d805): the boards fixed, some building pop back. c33 is both: creeping on the buildings' program and
  jumping between the boards' arrow frames. interp_blends_constant now takes the two values (SDK); Juiced blends c33
  only when it moves less than 0.01. SMOKE OK, wincheck ok.
- Harry (fc6b6a9): WARNING and legal pages gone, boards clean, the buildings shaking again. Since 271295c (buildings
  fine): c33 by size, peers and sprites out of the scenery rule. Test switches for each: JUICED_C33 (0 held, 1
  blended), XBE_INTERP_SCENERY_PEERS=1, XBE_INTERP_SCENERY_SPRITES=1.
- Harry: each of the three alone, the buildings flicker (c33 blended, the boards too); peers and sprites together,
  flicker; all three (as 271295c), both flicker. So 271295c's steady buildings were not these rules. What is known:
  XBE_INTERP_CAMERA_ONLY=1, the buildings steady. Split: =2 only still programs' draws take the camera's move (matched
  or not, peers, sprites), =3 only the other programs'. Headless =2 runs; SMOKE OK, wincheck ok.
- Harry: =2 the buildings stable; =3 a shaky car. SDK: GameInfo::interp_scenery_takes_camera (default off, other games
  as before) makes =2 the default; Juiced sets it. XBE_INTERP_CAMERA_ONLY=0 turns it off. SMOKE OK, wincheck ok.
- Harry (e37f1f1): "looks great", buildings, boards and cars; but the brake lights do not work. Headless, braking (LT by
  the run folder's cmd at 120 s, 4 frames snapped with XBE_INTERP_TEST=2): smooth motion off, lit; on, two frames of
  four unlit; XBE_INTERP_CONSTANTS=0 the same; XBE_INTERP_CAMERA_ONLY=0 all lit. The lit lights are drawn by a still
  program (the lamps' glow) with the car's matrix, and took the camera's move. A still program's draw sharing its
  matrix with a draw of another program keeps its own: all four lit. SMOKE OK, wincheck ok.
- Harry (5591379): brake lights fixed.
- Open: the replay speed-up (c6f23f8, reverted in 8f55ad4 while the buildings were tested) was not the cause and can
  come back; on Harry's machine the replay's time is the draws (2.6 ms), the next thing for higher frame rates.

### 2026-11-15 - Smooth motion: unmatched scenery takes the camera's move
- Harry (989aa16): the buildings still shift with the off-camera fix; with XBE_INTERP_CAMERA_ONLY=1 (no crash now) the
  background is steady and the cars flicker a little. So it is how scenery draws are matched or left.
- Headless (temporary log), each 3D draw's halfway place against the camera's move, by how it is drawn, near the race:
  matched draws of still programs ~0.1% off; unmatched draws of still programs left at the step's place 6057 of 7416
  off by more than 0.25% of the screen (about 10 a step; programs 005D1841, 00D3E7E1, 01430E51, 00E41971, 00CAECD1,
  00CAEAF1); the large "other" counts are the cars' programs (010E1AA1, 014308E1, 010E1BD1).
- SDK: an unmatched draw of a still program takes the camera's move whatever its vertex count, its program's share of new
  draws or its nearest matched draw (the pieces rules kept for other programs). XBE_INTERP_SCENERY=0 for the old rules.
  SMOKE OK, wincheck ok.
- Harry (271295c): buildings and cars fine, the corner arrow boards broken again. Peers (unmatched draws of a matched
  draw's matrix: a second pass over one surface, the arrows over the board) were given the camera's move too and lay a
  little off the board's own blend. A peer keeps following its matched draw. SMOKE OK, wincheck ok.

### 2026-11-15 - Smooth motion: buildings off the camera's move; the camera-only crash
- Harry: the buildings shift slightly (away from the road and back), clearest on the far left on a corner (his clip
  11-09-36, 2 to 4 s); with smooth motion off, none. Still there with XBE_INTERP_CONSTANTS=0, PREDICT=0, PIECES=0,
  with c33 blended again (a1979db) and without the replay speed-up (4143711). Every step of that run was blended.
- Correctly matched still scenery cannot move against itself (each one's blend is the camera's move applied to it), so
  it is wrong matches. Headless (temporary log): matched draws of still scenery programs whose own move is not the
  camera's, their in-between place against the camera's: most within 0.1% of the screen, but near the race ~9% at 1%
  to over 3% (35 to 100+ pixels at 3440), mostly programs 014307F1, 01066B51, 010E0BD1.
- SDK a903c86: such a draw (still program, off the camera's move by more than kOffCamera = 0.25% of the screen at
  halfway) takes the camera's move like scenery come into view (derived), before peers are found.
  XBE_INTERP_OFF_CAMERA=0 turns it off. Headless: 48 to 1503 a 10 s.
- The XBE_INTERP_CAMERA_ONLY crash (twice on Windows): it ran after peers were found, and a peer whose matched draw lost
  its match read before_[-1]. Moved before the peers. SMOKE OK, wincheck ok.

### 2026-11-14 - Smooth motion: in-between frames 4 to 5 times cheaper
- Harry's log (e5d7ae5): a replay 2.4 to 5 ms on his 5800X3D, with the title's 4 to 6 ms in a 16.7 ms step; from 90 s
  into the race no in-between frame fit (601 frames for 601 steps). Timed headless (temporary): blending the constants
  was three quarters of a replay (5.5 of 7.3 ms at ~1150 draws): Between for all 768 of every draw, every replay.
- SDK: Holds() split out of Between; each matched draw keeps the list of constants that differ and are blended
  (Rec::lerp), made once a step, and a replay blends only those; the rotated copy into the draw's registers is two
  memcpys. Headless: a replay 1.3 to 1.8 ms at 780 to 1080 draws (was 6 to 7.5 ms at ~1150). Same values as before.
  SMOKE OK, wincheck ok.
- Harry (8da4c0a): "textures shifting again", the buildings shift slightly; at cap 120, 240 and Unlimited with
  vertical sync alike. His log: a replay still 2.6 ms on his machine (the constants were not his cost; the draws are).
  The change gives the same values as far as read, but reverted (SDK 8f55ad4) to test it.
- Harry (4143711, the speed-up reverted, cap 120): the buildings still shift. So it came with e5d7ae5's held constants:
  of those, only c33.x changes smoothly on the buildings' program (00CAEAF1: 1.20613 to 1.2063 a step). c33 is
  blended again (src/common/game_info.cpp); the boards' atlas cells in it (0, 0.25, 0.5, 0.75) are held by the
  sixteenths rule. The speed-up stays out for now (2.6 ms a replay on Harry's machine either way).

### 2026-11-14 - Smooth motion: only the smooth constants blended; the arrow boards' flicker found
- Harry's A/B on the Release build of fb0e0da (clang 22.1.3; a Debug build at 21:29 had 32 ms steps and was rebuilt):
  no switch, flickers; XBE_INTERP_CAMERA_ONLY=1, a crash in game code 5 s into the race (guest read out of range,
  test switch only, not chased); XBE_INTERP_CONSTANTS=0, no noticeable flicker. So it was blending constants.
- Logged headless (temporary): the constants blended in the in-between frames. Smooth from step to step: c0 to c17
  (matrices), c28, c30 to c32, c37 (lights), c43, c44, c90.w (a clock), c92 to c94. Picked from a few values: c36
  (1, 0.67, 0.5, 0.4, 0.25, 0.14 on the scenery programs), c89.y, c41.w (30, 20, 35), c88, c95, c42 (tints), c33.
  Halfway between two of those is neither, and the leap and sixteenths rules (Between) missed them.
- SDK e199803: GameInfo::interp_blends_constant(reg); not given, as before. Juiced's (src/common/game_info.cpp) blends
  the smooth ones above; the rest take the step's value. Headless race with it: no errors; SMOKE OK; wincheck ok.
- Harry on Windows (e5d7ae5, 22:12 build): "little to no flickering at all", over 150 frames a second. His log: 1147
  frames for 601 steps at the 120 cap, then 1517 and 1662 for 600 (the cap raised in the run), 0 or 1 late steps of ours.

### 2026-11-14 - Smooth motion: pacing confirmed, the arrow boards still flicker (stopped again)
- Harry's log of 3cac4da (cap 120, vertical sync off): 1144 to 1197 frames for 600 steps, 2 planned, 0 or 1 late
  steps of ours. The pacing fix works. Harry: "still a fair bit of flickering".
- The recordings are Mirillis Action! window captures: about 72 distinct pictures a second at uneven gaps (1 to 8
  frames apart) while the game shows 120. They cannot count frames, so the earlier "in-between frames not reaching
  the screen" (from the clips) was the recorder.
- Harry's A/B: smooth motion off, the arrow boards do not flicker; smooth motion on with the speed blur off, they
  still do. The game has no motion blur setting of its own. So it is the in-between frames, not the blur.
- In the clip (16.2 to 16.9 s) the big orange arrow board shows, in some frames, a faint second copy of its arrows
  offset from the board. "BRAKE NOW!" flashing red and white is the game's own.
- Headless, logged in a race (temporary, removed): ~340 draws a step without a match. ~250 of them are in targets
  with no camera move (six 83301F80..83351F80 targets, cube faces or reflections, and the back buffer 83D86000);
  the scene is drawn into 01460000 (640x960), whose camera has ~180 votes. ~80 a step take a peer, few derive,
  few are left for any other reason; small unmatched scenery draws (< kLeastVertices) are ~0.5 a step. So the
  board's draws are matched and the fault is in how they are blended, not in the matching.
- Headless cannot reach the boards: the race runs at about 1/30 speed (race clock 3.7 s at 118 s), and its
  reference renderer has noise on windows. Windows is the only place to see it.
- Five fixes in all (constant leaps, transform register, animation cells, toggles and strays, pacing). Next, from
  Harry on Windows: the same corner with XBE_INTERP_PIECES=0, then XBE_INTERP_PREDICT=0, to rule each in or out; a
  way to name the board's draws (a hotkey that logs the draws of one step with their textures and how each was
  blended) would let the board be found in the log.
- Harry: XBE_INTERP_PIECES=0 flickers, XBE_INTERP_PREDICT=0 flickers. Two more test switches (SDK 4461e84):
  XBE_INTERP_CONSTANTS=0 (every constant past the transform at the step's value) and XBE_INTERP_CAMERA_ONLY=1 (every
  3D draw takes its target's camera move, none its own match). Headless with both: runs, SMOKE OK.

### 2026-11-14 - Smooth motion on Windows: the second frame of a step was dropped by late sleeps
- The VM's logs of Harry's runs (a377b97; cap 120, vertical sync off): 735 to 916 frames for 560 to 601 steps, 2
  planned a step. The second frame, timed to be done 1 ms before the title goes on, missed it when Windows' sleep
  woke 1 to 2 ms late (timeBeginPeriod(1) is set). One frame a step or two, unevenly: the judder Harry sees as flicker
  on the arrow boards and the crowd, and the recordings' runs of repeated frames.
- SDK interp.cpp SleepUntil: sleeps to 1.5 ms before the time and waits out the rest.
- Headless at FPS=120: 1200 and 1192 frames for 601 and 600 steps, none late of ours (Linux sleeps are close, so this
  checks nothing broke; Windows is the test). SMOKE OK.

### 2026-11-14 - Smooth motion on Windows: the recordings show about 60 distinct frames a second (stopped, 3 attempts)
- Harry (a377b97): the flicker is still at the corners, the arrow boards and turns 2 and 3. Three fixes for it
  (animation cells, small toggles, stray copies) changed nothing he could see.
- Both of his 240 fps clips (12-59-58 and 14-20-04), measured frame to frame (86x36 grey): three frames in four are
  identical to the frame before. One picture a step (the HUD timer moves once each 4 frames), with now and then an odd
  frame in between: that odd frame is the flicker. The in-between frames are mostly not reaching the screen there.
  Headless (XBE_INTERP_TEST) draws them evenly, so this is the pacing on Windows, not the blending.
- Not known yet: Harry's frame cap and vertical sync, and the log's "gpu: in-between frames" lines (frames for steps,
  the title's time, the replay's time, frames planned, limit, late steps). Suspects: the late-step limit holding at
  1 frame a step, or the sleeps between frames (Windows timer resolution) overrunning the step's time.
- Next: Harry's log from a race with smooth motion on.

### 2026-11-14 - Smooth motion: smaller toggles held; copies that land wrong are not blended
- Harry's 34 s clip of ece6708, 240 fps: about 60 single-frame events. The worst are the big orange arrow boards at the
  corners (the arrow layer off for one frame); some are the speed blur's own trails, not flicker.
- Logged headless in a race: c93.w toggles 0 and -0.13 on scenery and crowd draws, under the 0.2 leap. kConstantLeap is now
  0.05 (with the change at least half the value); what still blends by a large share is the light colours fading (c28,
  c32, c37).
- Copies: on a match among several copies of a still-scenery program (most of its draws go with the camera), the pick
  is checked against where the camera's move puts it. More than a quarter of the screen across, or 15% in depth, and
  another copy nearly as near: no self, and it takes the camera's move (as a piece come into view, whatever its vertex
  count). The screen check is in the projection's own scale: Juiced's x / w is not -1 to 1. Rejected now: a few
  draws a step, 2 to 3 times off in depth. The flyover's alike cars and anything moving clearly nearest itself are kept.
- Headless: frames in pairs, SMOKE OK.

### 2026-11-14 - Smooth motion: animation cells are not blended
- Harry on Windows (fc4b700): "much better", but still flicker on turns 2 and 3, and the corner boards' arrows flicker.
- The arrows run through atlas cells (c33.x 0, 0.25, 0.5, 0.75; c34 alike). The leap rule caught 0 to 0.5 but not 0.5 to
  0.75 (less than half its size), so the in-between frame drew the arrows half a cell along.
- SDK Between: a constant past the transform whose old and new values are both whole sixteenths takes the step's value
  (an animation's cell or a toggle, not a smooth move). SMOKE OK; the boards were too far off in the headless run to
  check by eye.

### 2026-11-14 - Smooth motion: Juiced's transform is c18, not c0; matching takes the camera's move off
- Harry on Windows (3f50b78): textures still flickered above 60. His 240 fps clip, frame by frame: a single frame differs
  from both neighbours about a dozen times in 8.7 s. The near shop fronts drawn shifted, traffic lights and walk signs jumped
  with a dark box round them, the crowd swapped places, a palm drawn elsewhere: copies of one mesh blended towards a
  neighbour copy.
- Cause: in-between frames took the first four constants (c-96 to c-93) as every program's transform. Juiced's are
  zero; its world-view-projection is c18 to c21 (c0 to c2 the world, c7 to c9 view, c10 to c13 projection, c14 to c17
  view-projection; dumped headless). So every draw had the same "matrix": copies of a mesh were matched in draw order,
  the leap and turn tests never fired, and the camera's move was never found ("0 of 0" draws).
- SDK: `GameInfo::interp_transform` (default -96, the old behaviour); a record's constants are kept turned so that
  register is first, and put back on replay. Juiced sets 18.
- SDK: matching runs twice: the camera's move found from the first match is taken back off the step before's
  matrices, and each draw is matched again to the nearest copy where the camera's move puts it (XBE_INTERP_PREDICT=0
  turns this off). XBE_INTERP_LOG logs matched draws not moving with the camera.
- Headless (XBE_INTERP_TEST=2): the main scene nearly all matched. The draws not following the camera come from two
  programs (76 and 54 draws a step, most likely the cars). The unmatched ones go to the small reflection targets
  (03301F80 and on, a different one each step), drawn at the step's place. Frames in steady pairs. SMOKE OK.

### 2026-11-14 - Smooth motion: constants that leap are not blended; the game's icon
- Harry on Windows: the race timer no longer flickers (sse_cvt, 2eb6014). With smooth motion and the cap raised,
  textures flickered.
- Cause: in-between frames blended every vertex shader constant of a matched draw. XBE_INTERP_LOG's new `wrap:` lines
  show constants past the transform that leap from step to step: c129.x 0 / 0.5 / 0.75 (an atlas cell), c132.y
  toggling 0.4 and 1, the crowd's tints in c138. Halfway between two of them is neither, so every in-between
  frame drew the wrong texture cell or tint.
- SDK interp.cpp `Between`: past the 4 transform registers, a constant that changed by 0.2 or more and by at
  least half its size takes the step's value; the rest blend as before (blend and peer paths).
- Headless (XBE_INTERP_TEST=2): consecutive frames now come in matching pairs; no crash in 150 s. SMOKE OK.
  Not yet seen on Windows.
- launcher/icon.ico from Harry's picture (.local/win), corners made transparent, 16 to 256 px: the exe's and
  the launcher's icon (CMakeLists.txt resource 1).
- Next: Harry to check smooth motion at a raised cap.

### 2026-11-14 - Loading measured; Xbox Live entries gone; PlayStation and Switch button pictures (SDK b63a566)
- Loading: a race loads in about 1.5 s here (245 files opened within 87.1 to 87.3 s, one 1.5 s gap in the frames);
  disc reads are not slowed for Juiced. The 25 s before the countdown is the game's track flyover ("Welcome to San
  Ricardo"), which a button skips. Nothing to change.
- Effects detail: the shadow, reflection and glow targets (128x128, 160x120 on the console) are already drawn at the
  render scale (3x at 1440 lines, captures of the 13th). Nothing to change.
- Xbox Live: the main menu's item "online" (captioned XBOX LIVE at 0x7C599) is removed as the menu opens, with a
  backdrop row, the way the game removes its "quit" (hook at 0x7C62E, the game's remove by name 0x1924C0). The
  Options menu's "xbox_live" gives its row to PC SETTINGS (AddPcItem) instead of a sixth row. 0x372DC4, which the
  Options menu tests before adding it, is the online manager object, not a flag: left alone. Headless: main menu
  CAREER ARCADE CUSTOM RACE MULTIPLAY OPTIONS EXTRAS, the d-pad's highlight on the right rows; the race's Options
  AUDIO GAME PC SETTINGS DONE. The main menu's footer line "Not Signed In" (gadgets ONLINEBOX, ONLINE,
  ONLINEICON) is hidden at 0x7C6E6, where the game hides its GameSpy logo the same way.
- The boot crash seen once on the 13th (0.43 s, no crash handler output): not seen in 12 boots in a row since, nor
  in any other run tonight. Open.
- Controllers are SDL3's already (Xbox, PlayStation, Switch pads, wheels). SDK b63a566: input::LastPadStyle, the
  kind of pad controller 1 was last played with (the harness: PAD_STYLE). With modern_buttons the prompts follow
  it: PlayStation's cross, circle, square and triangle in their colours (A, B, X, Y by place), Switch's B A Y X
  (by place). The controls page stays an Xbox pad. Headless: both, the menus' OK and BACK.
- SMOKE OK, wincheck ok. Next: Harry's run (a PlayStation pad if he has one).

### 2026-11-14 - Above 60 frames a second: in-between frames in a race (SDK 18182bb)
- Harry asked for PC quality-of-life work overnight, the frame rate first.
- The game times its simulation by the time between frames (0x12B9F0: game +0x1C is the step's seconds from a
  timer, +0x18 the sum; a debug flag at 0x372ED8 fixes it at 0.02). Paced by the frame cap instead of the present
  interval (an experiment, not kept), it ran at 120 headless with game time following real time; but with the same
  pad the car was at 94 mph at 4.95 s against 80 at 4.72 at 60, and spun at 9 s where at 60 it drove on: the
  driving depends on the frame rate. So the game stays at 60.
- Frames in between: the SDK's interpolator (gpu/interp.cpp, as EoN and True Crime use) replays each step with its
  vertex constants blended from the step before. Setting smooth_motion (on; F1 window, and PC SETTINGS page 4):
  in a race (game object +0x3C), with a frame cap over 60. Harry's cap is 60, so nothing changes until he raises it.
  Headless FPS=120: 1197 frames for 601 steps in 10 s, all blended, a replay 1.2 ms; four frames in a row show
  the in-between one. The glow's mask update replayed settles where it would (0.65 x old + new: the same limit).
- The interpolator read a stream's vertex buffer header after the game had freed it, as the race loads (crash at
  frame 6840 every time; the same stale binding as Harry's crash of the 13th). SDK 18182bb: skipped when not
  committed.
- SMOKE OK, wincheck ok. Next: Harry with the cap at 120 or more; loading times.

### 2026-11-13 - The race timer: CVTSS2SI rounds as MXCSR says (SDK 2eb6014)
- Harry on a7426b8: still happening. His 240 fps recording (.local/win, 610 frames) shows it: TIME reads 00:01:92,
  02:17, 03:65, 03:98, then 03:02, 03:28: the seconds one ahead from .50 to .99 of every second.
- The formatter 0x62740 ("%02d:%02d:%02d" at 0x2F708C) converts minutes, seconds and hundredths with 0x2D4B0, which
  is CVTSS2SI eax, [esp+4]. The game's startup sets MXCSR's rounding control to truncate (0x186AA0: or 0x6000); the
  recompiler's sse_cvt always rounded to nearest. 0x2D4B0 is the game's float to int everywhere (the glow's blend
  constant among others), so more than the timer was a unit off.
- SDK 2eb6014: sse_cvt(ctx, v) takes the rounding from ctx.mxcsr (CVTSS2SI, CVTPS2PI); generated code passes ctx.
  The one-argument form stays for code generated before.
- This also explains "01:00:00" in Harry's earlier shots (minutes rounded up from 30 s) and probably the lap digit's
  flicker he first reported. The "HUD texts keep their own gadget's anchor" change (41c9a57) was a deduction for a
  different mechanism; kept, it is right in itself.
- SMOKE OK. Next: Harry's run.

### 2026-11-13 - Harry on 0d831fe: the times jump and flicker for a few ms (SDK 41c9a57)
- Harry: needle and rivals' markers right; in a race the timings jump now and then and flicker for a few
  milliseconds.
- Likely cause (not reproduced headless): a gadget's text is pushed into the push buffer by the game itself and is
  only read at the next library call. That is usually one of the next gadget's state setters, before the hook at its
  quad changes the anchor; when the next gadget needs no state set, it is the quad's own draw, after the hook, and
  the text took the next gadget's squeeze_shift for that frame.
- SDK 41c9a57: d3d::Pending() reads what was pushed so far. SetAnchor (on a change), BandQuad and PassViewport
  call it before changing State. SMOKE OK, wincheck ok.
- Next: Harry's run.

### 2026-11-13 - Harry on c20619d: the rev needle beside its dial, the rivals' markers
- Harry (3440x1440): speed blur row works, pause screens fixed, HUD at the edges; the rev needle showed left of
  its dial as it moved (screenshot); the lap number "still occasionally" flickers.
- The needle's quad turns about the dial's middle and its corners pass the dial's rectangle (y 313 against the
  rectangle's 316 in the first log): HudAnchor's "inside the last gadget" test, by corners with 2 pixels of slack,
  failed then and the needle was drawn unanchored. Now by the quad's middle (and no larger than the gadget).
- The rivals' markers (RIVAL0 to RIVAL4, triangles sliding along the bottom) were anchored in three steps; now in
  proportion to their place, so they cross the frame's width evenly.
- Headless 21:9: the needle in the dial. SMOKE OK.
- The lap number: its big digit is small in some headless frames and full in others (the countdown, 5 s into the
  race) at 16:9 and 21:9 alike, anchored or not: the game seems to animate its size. Not known what Harry sees
  flicker; asked him.

### 2026-11-13 - The HUD at the frame's sides, the pause screens' prompt bar, the race's level, a speed blur row
- Harry on 15c4a3a: nitro and the pause menu's flickering header both fixed. Asked for: the race HUD at the screen's
  edges, the pause menu at 21:9, the race's loudness, a speed blur row in PC SETTINGS.
- HUD (src/game/widescreen.cpp, HudAnchor): the race's HUD is the window "hud_race", about 60 gadgets; each draws a
  quad of its rectangle through the 2D quad calls before its text, needle or meter. A quad matching a gadget whose
  middle is left of 240 or right of 400 anchors what follows to that edge of the frame
  (gpu::State::squeeze_beyond, squeeze_shift = +-320 (1 - k) / k) until a quad outside that gadget; the map's pass
  (PassViewport) moves with it; the present (0x12B9F0) ends it. Messages, the countdown and split times stay centred.
  Headless at 16:9 and 21:9: laps, times and map at the left edge, position and dials at the right.
  (The reference renderer at 21:9 draws the big lap and position digits as slivers, anchored or not; Harry's D3D12
  capture at 21:9 has them whole.)
- Pause screens: the menu stays in the centred part (the mouse's mapping unchanged); the bar at the bottom right
  with the A and B prompts (pictures and texts from y 400 to 445, x from 150) goes to the right edge, where it ends
  on a 4:3 frame. The dark strip from the left edge already reached it. First tried the pause menu's rows at the left
  edge: its Options screens are the same template and stayed centred, so the menu jumped sideways; dropped.
  Headless at 21:9: hud_pause and opts_ingame_menu.
- Sound: a race's 3D voices (about twenty, engines and effects, head relative) have headroom 0 where the menus'
  music (2D ADPCM) and the race's music (a PCM stream) have the library's 6 dB. SDK 81a5508: GameInfo::voice_3d_level.
  Setting effects_percent (default 50, -6 dB): menus -25.6 dB RMS as before, the race -13.5 (was -9.3), clipped
  samples in 10 s 0 to 2 (were 8651). A recording of the game played (the video Harry linked; platform unknown)
  has its race 6 to 10 dB over its menus; ours was 15, now about 12. A guess at the level, for Harry's ears.
- PC SETTINGS: four pages. GAME has SPEED BLUR and ENGINE AND EFFECTS LEVEL; SCALING FILTER, LOW LATENCY and
  RENDERER moved to SYSTEM (4/4). A page's unused rows hide everything in their bands. Headless: all four pages.
- One run of the harness crashed at boot (0.43 s, segmentation fault) right after a rebuild; two repeats and every
  run since were fine. Not understood.
- SMOKE OK, wincheck ok. Not seen on Windows yet.
- Next: Harry's run. Open: the race timer at 01:00:00 in his shots, smoke.sh telling a race from a menu.

### 2026-11-13 - Nitro's white-out: the front buffer slot held the back buffer (SDK 62cffef)
- Harry on b3f44c0: chevrons right, cables right, nitro "less overwhelming but still a very bright white", the pause
  menu's header band flickers (not new).
- The glow: draw N+1 (0x131C00) adds white where a 160x120 mask's alpha is set, then updates the mask from the
  frame's alpha: mask = (0.70 x frame alpha)^2 + 0.65 x mask (vertex alpha at object +0x16C, blend constant
  0x31397C). Harry's per-draw dumps had the HUD and the nitro vignette in the mask though the frame was clean
  after its resolve.
- Cause: the game keeps two frame-texture objects (three texture headers each, class vtable 0x313448), one made
  from the device's back buffer slot and one from its front buffer slot, each caching that buffer's memory address,
  and swaps the two after every Swap (0x12BAFA, device object +0x208 / +0x20C) as the library rotates the memory
  (its Swap 0x2AB2F0 rotates Data and Lock). The SDK put the back buffer in both slots (no FrameSurfaces in the
  5849 layout), so both objects cached one address: every other frame the glow's mask, the speed blur and the
  nitro passes sampled the frame before, HUD and vignette included, and the mask fed on it.
- SDK: the front slot (CDevice_FrontBuffer) holds the front buffer. Headless: the mask pass's texture 0 is the
  frame's own memory on both parities (03D86000 / 03EB3000), before it was 03D86000 always. SMOKE OK, wincheck ok.
  CreateDevice and Reset now log the multisample type and swap effect (Juiced: 0x11, flip).
- Not seen on Windows yet. May also explain a faint glow on the HUD and ghosting in the speed blur.
- Next: Harry's run; the pause menu's flickering header band; the race HUD in the 4:3 area.

### 2026-11-13 - The orange shapes: the turn indicators, drawn twice the size (SDK 915f909)
- Harry: the orange shapes are the track's turn indicators (chevrons on the corner boards). His second captures
  (XBE_CAPTURE_DRAWS=0-1500, pulled by scp from the VM into .local/win/captures/164806) show the target after each
  draw: the sheet appears at draw 525, a 22-vertex strip with the chevron texture (512x32 DXT5), additive, drawn
  "aa 2x2". Headless the same quad failed the depth test whole (colour 0), so the boards were blank there.
- Cause: the glow materials set D3DRS_MULTISAMPLERENDERTARGETMODE 4X (0x2A9900) around their draws on the 640x960
  scene target. The console's library (0x2AA390) then halves its viewport scale (device +599/+600 = 0.5) and sets
  the viewport again, so with the hardware's two samples a pixel each way the draw lands where the others do. Ours
  gave the programs the whole viewport in c-38/c-37 and drew 2x2 to a pixel: twice the size from the corner.
- SDK 915f909: GameInfo::sample_scaled_viewport (Juiced sets it): UpdateViewportConstants scales by 1 / aa_x, aa_y,
  and UpdateSampleScale refreshes them. Off for other titles (not tested against them).
- Evidence: headless, race frame 7000: the quad now writes 1292 pixels and the corner's boards show orange
  chevrons. SMOKE OK, wincheck ok. Not seen on Windows yet. The nitrous white-out is not explained by this (its
  glow pass adds what these materials write into the frame's alpha): to check on the rebuild.

### 2026-11-13 - Harry's captures of 21d3205 (F3, .local/win/1611xx): what they show so far
- Six frames: 161100 race start, 161114 / 161133 / 161216 orange shapes, 161159 pause menu, 161228 nitrous.
- Nitrous (161228, 1342 draws): after the resolve (draw 1204) draw 1205 adds a 160x120 buffer (0x819D0800, four taps,
  ps 0x3C17C0, blend SRCALPHA / ONE) over the frame; 1206 writes only that buffer's alpha from the frame (blend
  SRCALPHA / CONSTANTALPHA, colour write alpha only); then five 12-vertex lists and a 10-vertex fan draw the frame
  over itself. Headless the same draws exist and add nothing white. Why the buffer is white on D3D12: not known.
- Orange shapes: Harry says they are the track's turn indicators. Not tied to a draw. Ruled out: the HUD's corner
  arrow (a 2D quad, right), the car's decal and wheel draws, the trusses' textures (small struts). The litter
  particles (vs 0x53A681, 238 vertices, two passes) are a candidate only.
- Wires: 19 opaque grey line strips, the one read a sagging curve of 9 points: the game's cables, not stray lines.
  The reference renderer draws no lines at all (colour 0), so headless never showed them.
- The pause menu's header band and panel stay in the 4:3 area at 21:9 (161159): the band hooks do not reach it.
- A threaded draw does not copy its vertex buffer (render_thread.cpp Draw): a buffer rewritten between two draws
  of one batch would be read late. Not shown to happen here; noted.
- Next: a capture with XBE_CAPTURE_DRAWS=0-1500 (the target after every draw) of the orange shapes and of nitrous.

### 2026-11-13 - The menus' header and footer bands run the frame's width
- Harry: at 21:9 the bands stay in the 4:3 area; "anything nice we can do with those?"
- A band is two 256x64 tiles (P8), 0 to about 323 and 320 to about 643 of 640, moving a little, drawn by the 2D quad
  functions' D3DDevice_DrawVerticesUP calls. `[[hook]]`s at 0x12E4AB, 0x12E66A, 0x12E831, 0x12EB12, 0x12ED67 (and
  after each): a textured 4-vertex strip from one edge of the frame to its middle is drawn unsqueezed
  (gpu::State::no_squeeze, put back after), so each half runs from the middle to the frame's edge, its streaks wider.
  The flame art, logo and text over them stay in proportion in the 4:3 area.
- Evidence: headless main menu at 3440x1440 (FRAME_ASPECT_X100=239): both bands edge to edge. SMOKE OK, wincheck ok.
  Left: the black strip behind the footer's text still ends at the 4:3 area's sides. Not seen on Windows.

### 2026-11-13 - The mouse in the pause menu
- The pause menu's windows ('hud_pause', its message boxes, the in-race options) are in another UI manager than the
  front end's, made at the race's load; the pointer only knew the front end's (from 0x188790) and was off in a race.
- src/game/hooks.cpp: 0x18B780 (a page registered: manager, name, page) gives the manager in use
  (juiced::SetMenuManager); a race registers the pause menu's pages (0x99310), the front end its own on the way back.
  In a race the pointer works only over a window with rows, and a click off the rows is not A.
- Evidence (headless, JUICED_POINTER=@file): the pointer over RESTART moves the highlight; a click on ABANDON RACE
  opens "Quit race?", a click on YES returns to Arcade's race list, where the pointer's window is found again.
  SMOKE OK; wincheck ok. Not seen on Windows.

### 2026-11-13 - Correction: c6027d4's SurfaceOf check was not at fault (SDK 6e3fac6 only removes it)
- I blamed it for pad scripts missing the race (race2: a Lobby screen; race.txt: Career's Driver Betting). Wrong: with
  the check back and REF=5000 the race is reached. The cause is the capture rate: REF=60..150 slows the run, the
  pad's presses (timed in vertical blanks) land on other screens. A run with REF under about 1000 is not a route
  test; look at a frame before trusting one.
- The check stays out (it was a guess beside the confirmed SyncDeferred fix). 554d836 on Harry's PC is sound.
- Harry's run of 554d836: no crash. Reported: title bands end at the 4:3 area; the wires and orange arrows in a
  race; the mouse does nothing in the pause menu; nitrous puts a strong white gradient over the screen.
- The corner arrow of the HUD is an ordinary 2D quad (vs 4E69E1, P8 128x128, alpha test), right headless.

### 2026-11-13 - Harry's Windows run of d55de0f: a crash 55 s in (the menus)
- "exception C0000005 ... on guest thread 14, read guest 003EFB04", guest stack 0x12EB17 (after the 2D code's call of
  D3DDevice_DrawVerticesUP 0x2ABE70, vertices on the stack), 0x12EEAC, 0x195D0F.
- recomp-vm matched the host address against d3d8.cpp.obj: SyncDeferred, `R32(st.vb + 4)` of stream 0's vertex
  buffer at 0x3EFB00, a page not readable (past the image: heap given back). The same kind as 1d0d969's Arcade crash.
- SDK c6027d4: SyncDeferred leaves a stream whose header is not committed alone; SurfaceOf likewise. SMOKE OK, wincheck ok.
  Not reproduced headless; the fix is from the dump.
- Next: rebuild on recomp-vm, Harry's run.

### 2026-11-13 - d55de0f built on the new Windows VM (recomp-vm, C:\Testing\juiced-recomp)
- The old windows-vm is gone; the new one builds in `C:\Testing\juiced-recomp` (game files from `C:\ISOs`), VS 2022
  Build Tools 17.14 + clang, `build.bat`, no errors. WARP, about 3.7 frames/s. d55de0f had not been pushed; it is now.
- Its shots (not seen by me, described by recomp-vm): the intro movie is a 1073x725 picture in a 1924x1061 frame =
  640:432, the game's own top/bottom bars plus our side bars (the same headless at 16:9: 480x432 of 640x480). The
  title at 1600x900 (`JUICED_SKIP_INTRO=1`): the backdrop movie fills the width unstretched, the 2D in a centred
  1200x900 area. A black strip about 38 px high across the top of the title - not looked into.
- Next: Harry's run on his GPU (4:3, 16:9, 21:9; the wires; the mouse; the HUD's place; the orange quads).

### 2026-11-12 - Widescreen (4:3, 16:9, the screen's shape) and the wires

Harry at f6215b1: "Blur mostly fixed. Now some visual bugs of lines across the screen. Also, can we fix the 4:3, 16:9
and 21:9 ratios? The game is stretched." Five shots in .local/win (3440x1440, aspect 'screen').

The lines: black zigzags in the sky at the first junction. They are the scene's overhead wires: 19 line strips of 5
vertices (prim 4) with the buildings' programs, in the 640x960 target. The renderers draw a line a console pixel
wide (LineHlsl), which on the stretched target is 5.4 by 1.5 screen pixels; the reference renderer's 640x480 hardly
shows them. SDK: GameInfo::line_width (console pixels, 1), Juiced 0.35.

Widescreen, as designed on 2026-11-12 (below), now built:
- 3D: the game's own 16:9 in the console's widescreen mode. Past 16:9 (src/game/fov.cpp, after the game's 0x166A20):
  a scene camera (shape +0xC0 = 0.5625) gets tan(half angle) times frame / (16/9), its sines again, its projection
  rebuilt (vtable +0). The vertical angle stays.
- 2D: its programs are position = v0 * c[118] + c[119], then the viewport tail (dumped: FlatProgram true; the map's
  and the scene's are dp4 programs, false). SDK 7c45c2e: gpu::FlatProgram (position from v0 and constants by mov, mul,
  add, mad and an ILU mov, rcp, rcc, through registers), GameInfo::squeeze_flat_programs beside
  squeeze_screen_programs (ScreenDraw, the programs' hud clause, the reference renderer), the reference renderer's
  frame test taking a 960 or 1152 high target. GameInfo: hud_aspect 4:3, squeeze_flat_programs, auto_fills_screen.
- src/game/widescreen.cpp: a pass with its own viewport (hook at 0x12DB14, the pass's SetViewport: the race's map at
  56,324 112x112, a menu's car panel) has the viewport moved and narrowed with the 2D, its draws left as drawn
  (new gpu::State::no_squeeze); the speed blur's passes likewise (hooks at 0x130EAC and 0x130EB1).
- The mouse (pointer.cpp) maps into the squeezed 2D.
Evidence, headless (reference renderer, frames stretched to the shown shape): OUTPUT=1280x720 FRAME_ASPECT_X100=178
and OUTPUT=3440x1440 FRAME_ASPECT_X100=239: the main menu, Choose Race (its car panel and map in place), the race:
cars and buildings in proportion, the HUD and the map round, the map's road inside its ring (outside it before the
viewport hook). 4:3: SMOKE OK (922 draws a frame). wincheck ok. Not seen on Windows.
windows-vm (WARP, 3440x1440, 14451fd): no shader errors, the title as headless: logo and banners in a centred 4:3
panel, the backdrop movie across the frame; Harry's own f6215b1 race shots are sharp (the blur fix confirmed on his GPU).
That backdrop movie was stretched (a 4:3 picture across 21:9): widescreen.cpp now sets gpu::State::pillar_aspect 4:3
for YUY2 (Bink) draws spanning the frame: the boot's logos and intro pillarboxed (intro_movie), the menus' backdrops
filling the width at their shape, cropped above and below (pillar_fill), as PGR2. Headless at 239: both seen.
Known: the menus are a 4:3 panel over the full width backdrop (the banners end at its sides; the highlight bar and
the prompts' bar span the frame); the race HUD sits in the centred 4:3 part, beside the car at 21:9 (spreading it to
the edges needs a hook per HUD element: gpu::State::squeeze_beyond and squeeze_shift). In Harry's fifth shot large
orange translucent quads (sun glare?) by a building: not looked into.

### 2026-11-12 - Still blurry at 6f7d786, menus too: the resolve pass's spread taps

Harry: still blurry, the menus too, Speed blur no difference; his F12 shot (.local/harry/, 3440x1440): the whole
race frame soft as a 640x480 picture scaled up, though his log has "640x960 (3440x2880 here)".
- A menu frame (CAP, frame 2400) draws everything into the 640x960 target; one triangle (u = x, v = 2y) copies it
  into the 640x480 frame buffer. The race does the same for its scene. So every picture goes through that pass.
- BindTargetTexture gives a pass that shrinks a target spread taps (kTexWide: "each tap averaged over a console
  texel", for glow chains whose upscaled taps would skip texels). 640x960 into 640x480 is "shrinking" (gh larger), so
  each of its taps averaged 5.4 by 3 host texels: the console's resolution. 0de0637 widened that average (k follows
  the target's width), which is why it was no better.
SDK 4c5a099: gpu::ResolvesTallFrame (a target of the frame's width and twice its height read into a frame sized
target) keeps plain taps, in both backends; the tap's own filter averages the two rows. The race's glow (640x480 into
160x120) keeps its spread.
Evidence: wincheck ok, SMOKE OK (977 draws a frame). The path is Windows only: not seen here.
windows-vm, WARP at 3440x1440 (f6215b1 against 6f7d786, shots in .local/harry/): the Juiced logo's edges smeared
before, hard with the texture's own texel steps after. Harry's own look still to come.
Also noted, not acted on: at race load "MultiSampleMode 2 with target 004E6CB8 (640x960), not a frame buffer: drawn
2x2 to a pixel"; the picture's size is right in Harry's shot, so left alone.
The speed blur's half texel (blur.cpp) is unproven either way: Harry saw no difference with the setting off.

### 2026-11-12 - Harry's Windows run of 12b65c9: in the race, very blurry

Harry: "Game boots. However on a track it's incredibly blurry." So the Arcade crash is past (SDK 9ed6b2f). His log
(windows-vm; 3440x1440 borderless, aspect 'screen', render 100%, MSAA off): targets made
  640x480 colour and depth -> 3440x1440;  640x960 colour and depth -> 1920x2880;  160x120 -> 480x360.
A race frame (CAP draw list, frame 5400): the scene's 592 draws go to the 640x960 target (viewport 640x960), one
triangle copies it into the 640x480 frame buffer (v = 2y), then the glow (160x120, added) and six speed blur passes.
1. The scene target is not stretched: gpu::XStretch only widens a target of the frame buffer's height, so the race's
   3D had 1920 pixels across for a 3440 frame (0.56; 0.75 at 16:9), spread by the copy. SDK 0de0637: a target of
   twice the frame's height and at least its shape is stretched as the frame buffer is (640x960 -> 3440x2880 there).
   Draws into it are only squeezed where pretransformed or a title's screen programs, which Juiced has none of.
2. The speed blur (0x130910, D3DDevice_DrawVerticesUP at 0x130EAC: 12 vertices of 20 bytes, x y colour u v, the frame
   buffer drawn over itself six times, each larger about the centre): each u, v is the vertex's own x, y in texels,
   so with the console's pixel corners every pass reads the frame half a texel up and left. At 640x480 that softens;
   at a PC's size it moves the frame half a console pixel a pass. src/game/blur.cpp ([[hook]] at 0x130EAC) adds the
   half texel; setting speed_blur (on) off leaves the passes out; JUICED_BLUR=0 none, 1 as the disc draws it.
Evidence: SMOKE OK (925 draws a frame); wincheck ok. Headless shows neither: its renderer draws at 640x480, where
the same race frame with the blur as the disc's, off and corrected look alike. So both are reasoned from the log and
the draw list, not seen.
Next: Harry's look at the rebuild; if still soft, speed_blur off tells the two apart.

### 2026-11-12 - Harry's Windows run of 1d0d969: a crash choosing Arcade

Harry (RX 7900 XT, perf.bat): the menus at a solid 60.0 fps, host 0.6 to 0.85 ms a frame. Choosing Arcade: a black
screen, then "exception C0000005 ... on guest thread 14, read guest 003E9C80"; guest stack 0x12DB50 (the 2D pass
0x12DA30 after its call of D3DDevice_Clear 0x2AB9D0), 0x1288FF, 0x180889, 0x189443, 0x195F61.
- 0x3E9C80 is past the image (ends 0x3BACE0): heap, not committed then.
- windows-vm resolved the host frames from the PDB (publics only): the fault and the frames above it are in
  render_thread.cpp, below the hosted Clear in d3d8.cpp. On the guest thread that file reads guest memory for a
  Clear only in the state it sends: ThreadedBackend::Header copies each set texture, palette, vertex buffer, render
  target and depth header (kernel::R32(header), then 12 or 20 bytes).
- So: a header still set in a stage or stream after the game freed its heap block, read at the next Clear. The
  console never looks at it until a draw uses it.
SDK 9ed6b2f (fork branch juiced): Header returns none for a header whose memory is not committed, and copies 12
bytes when the 20 would run off committed memory.
Evidence: wincheck ok; headless with RT_THREAD=1 through Arcade into the race, no exception (it did not crash there
before either: the freed block's page stays committed with this heap's layout); SMOKE OK (1124 draws a frame).
Not confirmed: no source lines in the PDB, no repro here. If it crashes again the same way, ask windows-vm for a
RelWithDebInfo build for the exact line.

### 2026-11-12 - Field of view, the keyboard's controls page, the mouse in the menus

Harry: "Do 1-3" of the list offered (branch `extras`).

1. Field of view. src/game/fov.cpp: the camera set-up 0x166A20 takes the vertical field of view in radians at
   [esp+4]; an X86_FUNC scales its tangent by fov_percent for the race camera's four callers only (return addresses
   0x1272B, 0x175B5, 0x177DE, 0x17DF0, found by scaling every caller and logging which the race used), so the menus'
   cars and the 2D keep their shape. Setting fov_percent (50 to 150, JUICED_FOV for a run), the Game page's last row
   FIELD OF VIEW in steps of 5. Evidence: the same race frame at 100 and 130, the wider one showing more road.
2. The controls page with the keyboard. src/common/buttons.cpp KeysPage: while the keyboard plays
   (input::UsingKeyboard) the page's pad picture is drawn as key caps, each at the end of its button's line, from the
   bindings in use. Evidence: captures/1112-1347-ref_05801.png.
3. The mouse in the menus. src/common/pointer.cpp, from the pad hook:
   - The window showing is the last of the UI manager's stack (manager +0 to +4: "opts_bg", "opts_menu"), or the
     message box over it (manager +0x44). The manager is each window's +0x40, taken at the pages' common open
     0x188790 (X86_FUNC in src/game/hooks.cpp).
   - Rows: button gadgets (type 4; the lowest one that starts above the pointer) and the items of a menu gadget
     (type 6: items +0x11C to +0x120, current +0x13C; rows 22.3 lines apart for an item height of 0.04, from 2 below
     the menu's top). A menu whose items lie side by side (Arcade's series, item height 0.25 in a bar 24 high) is left
     alone.
   - The selection is walked to the row under the pointer by presses of up and down (left and right for rows on one
     line), 3 polls down and 5 up, only for 90 polls after the mouse last moved; a press that changes nothing stops
     the walk for that row. Left click: A once the row is reached, or at once off the rows and on a screen with none.
     Right click: B. Not while a window changes (manager +0xBC), not in a race (game +0x3C).
   - Windows: the arrow shows while such a window is up (the runtime hides the cursor over the game: its WM_SETCURSOR
     and its timer 0x5243 are answered first by a subclass of the window).
   - Tests: JUICED_POINTER=poll:x:y:buttons,... or @file (read as the run goes), JUICED_POINTER_LOG=1.

Evidence (headless, snaps, pointer from a file): the main menu's row follows the pointer (OPTIONS, ARCADE); a click
opens Options; PC SETTINGS pointed at and clicked; its rows pointed at, a click stepping Show frame rate to ON; right
click back to Options, to the main menu, then the quit message box, YES and NO pointed at, NO clicked; ARCADE, the
series and the race chosen by clicks into the race. SMOKE OK (915 draws a frame); wincheck ok for the six files.
Not done: the pause menu (in a race the pointer is off), lists that are not menus or buttons (Choose Race's list, the
car screens: a click there is A on the current row). Nothing of this seen on Windows; the pointer's mapping there
assumes the 2D spans the frame.

Next: Harry's Windows run (the list in the entries below, plus these three).

### 2026-11-12 - PC settings in the game's own Options menu

Harry asked for the PC settings as controllable items in the game's menus. src/game/pcmenu.cpp:
- The menus are windows of gadgets from text files in ui.dat (";; Jet UI Window File": BeginButton, BeginText,
  BeginMenu AddItem ...), each with a page object (vtable: open +0, message +4), registered by name with a UI manager
  (0x99210 the front end, 0x99310 the pause menu; 0x18B780 registers, 0x18AE70 loads).
- ui.dat still has the other platforms' graphics options window, opts_graphics (seven rows, RESET, DONE), and the
  Options handler still opens it for an item "graph" (0x95736); the Xbox build removes that item (0x952F7) and has no
  page for the window. Hooks at 0x99301 and 0x9939D register it with the Xbox Live options page's object (0x32E15C);
  a hook at 0x9539B adds a "pc_settings" item to the Options menu as the build adds "xbox_live" (0x191C00, 0x191E60);
  a hook at 0x95736 opens the window for it. The Xbox Live page's open 0x98D90 and message 0x98E30 are X86_FUNCs:
  our window's rows, else the game's own (the Xbox Live options still open).
- Rows: left/right (message types 3, 4) and A (5) step a setting, RESET_BUT is NEXT PAGE, DONE_BUT and B (0xC) go
  back to opts_menu or opts_ingame_menu (manager +0xBC = 7, the name at +0xC0). Captions through the game's 0x195390
  (ecx gadget + 0x10C, eax UTF-16). The two meter rows show label and value in the button; their meters are hidden
  (gadget +0x58). Pages: Display (mode, resolution, aspect, v-sync, frame cap, show frame rate, pause in background),
  Graphics (render scale, MSAA, edge smoothing, anisotropic, sharp 2D, world textures, sharpen), Game (modern buttons,
  skip intro, keyboard, scaling filter, low latency, renderer, alpha to coverage), as EoN's rows.

Evidence (headless, snaps): the item in Options; the page in the game's font; Aspect to 4:3, V-sync off, the cap 60 to
144 by left/right; NEXT PAGE to Graphics; A stepping Sharpen 10 to 20%; from the pause menu in a race, B back to
Options, the race resumed; the Xbox Live options page as before; SMOKE OK (949 draws a frame). Not seen on Windows;
the display rows (mode, resolution) do nothing headless.

Also noticed: the game writes D:\filelist.txt (the files it opened), which lands in the assets folder.

Next: Harry's look on Windows.

### 2026-11-12 - Skip intro setting; the widescreen design settled, not built

Skip intro movies (setting skip_intro, off; JUICED_SKIP_INTRO for the harness): while one of the boot's movies (THQ.bik,
juicegames.bik, JuicedCGI_NTSC/_PAL.bik) is the last movie opened (GameInfo::hide_file used only to see the opens), the
pad hook presses START for 3 polls in every 24, as the player would; hub_01.bik (the title's) ends it. Two other ways
failed: the files reported missing leaves the game on a black screen waiting for a movie that never ends; jumping the
movie step 0x103550 to its end at 0x103633 on the first step left an undecoded (green) movie up and the boot stalled.
Evidence: no presses, the title at 16 s (the legal screens are the game's 15 s), still PRESS START at 30 s; SMOKE OK.

Widescreen and ultrawide (investigated, nothing changed):
- Cameras: the setter 0x166A20 logged. Scene cameras get aspect 0.5625 in widescreen (set by 0x197E57, 0x1272B);
  2D and panel cameras 0.75 (some with +0x178 set: fov is a size), the cube map's 1. So a scene camera is told by its
  0.5625, and going wider than 16:9 is: after the game's own sums, hfov = 2 atan(tan(hfov / 2) * frame / (16/9)), the
  sine fields +0xAC and +0xB0 again, the vtable's first entry called again.
- 2D: every pass (0x12DA30) sets its viewport (0x12DB14), clears, and pushes c22 = (2/w, -2/h) and c23 through
  0x2AFC80 (D3DDevice_SetVertexShaderConstant1: ecx register + 96, edx four floats; not named yet) at 0x12DC5F. A
  squeeze of those constants or viewports on the game's side cannot tell a HUD element from a full screen effect
  (the speed blur's and fades' quads use the same programs), so it has to be per draw: the SDK's 2D squeeze
  (gpu::ScreenDraw, ScreenExtent, DrawSpan, the shaders' hud constant) with a third program form, position =
  (v0 + c[pre]) * c[mul] + c[add] then the viewport tail, and the 640x960 scene target accepted as a frame target
  (the reference renderer's test wants a height of 480 or 576). Then GameInfo: squeeze_screen_programs, hud_aspect 4:3,
  auto_fills_screen. That is shared SDK code and needs a GPU to check: for a session with Harry looking.

Next: Harry's Windows run; then the widescreen work above with him.

### 2026-11-12 - Audio check, the second-boot race, reflections, widescreen findings, menus, soak

1. Audio (WAV= of a boot to race, scripts/wavstats.py): sound all the way: the movies (-23 dB RMS), menu music (-25),
   the race (-10, clipping about 200 samples a second). The race's level comes from the game's own engine buffers, each
   near full level (FINDINGS, Sound): for Harry's ears, not changed.
2. scripts/pad/race2.txt: a later boot (KEEP_HDD=1 with a profile) through the Profile Manager and Load Successful to
   Arcade race 1: Harry's path, 200 s with RT_THREAD=1, no freeze. scripts/pad/soak.py (SOAK_PAD): race 1, random
   driving, the pause menu's Restart, again. scripts/soak.sh 12: no traps, frames to the end (frame 43020), memory flat
   (RSS 111 MB).
3. The coloured noise on car bodies and windows is the reference renderer drawing one frame on its own: the cube map's
   faces are drawn a few a frame (FINDINGS). Eight frames in a row show clean reflections. Nothing to fix.
4. Widescreen: the game's 3D is true 16:9 with the console's widescreen flag (it widens its own field of view); the
   2D is 4:3 and so a third too wide at 16:9, as on the console. Keeping the 2D in proportion, and filling an ultrawide
   screen, needs the SDK's 2D squeeze to recognise this game's 2D programs (scale and offset constants per viewport)
   or a game hook on those constants, and a GPU to look at the result: not started (FINDINGS has the addresses).
5. Menus walked live on a second boot without a trap: Xbox Live (sign-in, New Account, the dashboard question
   answered NO), Multiplay (System Link Play, Search), Extras, Custom Race (a sprint in San Ricardo, afternoon, 4
   cars), the pause menu and Restart. The Profile Manager's X DELETE icon is redrawn too (BE010B0D0670D0F0).
   Button pictures now check the palette as well: the game binds a texture before its palette, so the hook saw each
   icon with the previous texture's palette first and could have rewritten that one's colours; each known picture
   carries its own palette's hash and is taken only with it.

Evidence: SMOKE OK (914 draws a frame); the Profile Manager with X A B as today's discs (captures/1112-0924-ref_01900.png).
Not tried: answering YES to the dashboard question, a system link game, Career beyond its first screens, the tutorial
videos, Cheats.

Next: Harry's Windows run (the race freeze, frame rate, sound level, button pictures); then the 2D proportions
and ultrawide with him looking.

### 2026-11-12 - The music freeze fixed: a stream's ENVELOPECOMPLETE status

Cause: XACT stops a streaming cue with IDirectSoundStream_FlushEx (0x29F130, flags 1 or 5) and goes on decoding the
stream each update (0x2A433A -> 0x2A2F6C -> 0x2A3C60) until the stream's status has DSSTREAMSTATUS_ENVELOPECOMPLETE
(0x80000: 0x2A44D7 reads byte 2 bit 8), then clears its playing flag and calls Flush. The host's stream status had
READY only, so a stopped music track never finished stopping: its flags stayed 0x48 (stop seen, still playing) and it
was decoded for good. When its file was closed (the music teardown 0x29FE60 finds the player already stopped, state 3,
and closes at once; the track change 0x2A072E the same) the streamer's read failed and the WMA decoder spun.

Fix (SDK 7168ae0): a hosted stream reports ENVELOPECOMPLETE from a FlushEx until its next packet (the host flushes at
once, so the release is over). GameInfo::handle_reuse (6a39c37) only hid the bug on the first-boot race and is removed
from the SDK and the game.

Evidence: the second-boot repro (.local/hdd_second) runs on past 35 s (frame 3960 at 69 s) with XBE_HANDLE_REUSE=0;
the first-boot race 240 s without handle reuse and with RT_THREAD=1; a 480 s race (frame 28680, past a track's end), no
trap; SMOKE OK (1003 draws a frame). Not yet run on Windows.

Next: Harry's run on Windows (the race past its first seconds, his frame rate, the button pictures).

### 2026-11-12 - STOPPED: freeze when XACT closes a music track under its stream (three attempts)

Harry's Windows run (7ced15b, log fetched from the share): menus fine, the race loads (present 2351, 44 s), frames stop
at present 2540 (48.7 s, about 4 s in), no error; he closed it at 77 s. Not reproduced headless on a first boot (with
RT_THREAD=1, the throttle held through the countdown too). Reproduced headless on a second boot (the profile saved,
.local/hdd_second kept: `KEEP_HDD=1 scripts/harness.sh 60 scripts/pad/boot.txt` with that disk as .local/run/1/hdd):
frames stop at 35 s, frame 2100, after the profile's music settings load.

What happens (gdb, both cases): the main thread spins in XACT's update (game 0x19BE30 -> 0x29CF21 -> 0x2A433A ->
0x2A2F6C -> 0x2A3C60 -> 0x2A3F27) inside the WMA decoder (0x283B07 ... 0x287EFF), its fetch 0x2835F4 asked thousands
of times for bytes past the end of its buffer. The end: XACT's music teardown 0x29FE60 (here) or its track change
0x2A072E (the race load) closes the track's file (0x29FEC4, 0x2A0054) while the stream is still decoded; the
streamer's next ReadFile (0x28365B -> 0x1AF1BC) fails C0000008, it marks the end of the file (flags +0x40 bit 0x10)
mid-packet, and the decoder waits for data that never comes. At the teardown the music player's state is 3 (+0x5C) and
XACT's cue stop 0x29E400 is not called; the stream (0x3C5DE0, flags 0x48) stays active and keeps being decoded.

Attempts:
1. Handle reuse (GameInfo::handle_reuse, SDK 6a39c37, kept): the closed number goes to the next file opened, as on the
   console. Got the first-boot race going headless, but here no file is opened in between: the read still fails.
2. The fetch giving the bytes left past the end (an X86_FUNC of 0x2835F4): the decoder takes them and spins at the end.
3. An end of stream at the packet reader's wait (hook 0x284755: E_FAIL when the buffer has ended): never reached; the
   spinning fetches come through another of 0x283B5E's callers (0x2849C6, 0x284A83, 0x284B7B, 0x284C42 ...).
Both experiments are removed.

What I think is happening: on the console the stream is stopped before its file closes; here it is not. Candidates: a
stop or pause that XACT leaves to DirectSound (a hosted stream reporting a state XACT does not expect: the cue is never
seen as stopped), or the music thread (guest 5C, waiting in 0x2A01F3) running in an order the console's priorities
would not allow. Next options: (a) follow XACT's stop/pause of a streaming cue into the hosted DSound calls it makes and
compare with the library's own (the likely real fix); (b) keep a music file open after XACT closes it (defer the close)
so its stream reads real data until it does stop: simple, but the teardown frees the stream's buffers right after.

### 2026-11-12 - Modern button pictures, keyboard controls

Harry's priorities (via windows-vm): a race on Windows, 60 frames a second, modern controller and keyboard/mouse
prompts. Headless the race already presents a frame each vertical blank; his perf log will say how Windows fares.

The game's layouts (Options > Controls > a port, three options; option 1 the default): RT throttle, LT brake, the left
stick steers, A parking brake, B gear up, X gear down and reverse, Y nitrous, White change view, the right stick glances
(click: horn), Start pause, Back driver info. Black does nothing in any option.

The button pictures (JUICED_PICS_LOG=1 lists every 8-bit palettised texture bound with its texels' FNV-1a and writes its
picture): the prompts are 32x32 P8 textures with their own palettes, A D6B454E01B92FE99, B 80C366933D75C91E, Y
2CE618996326D735 (no X prompt met); the controls page's original pad is 256x256 158A8301606FDC3E, its lines and rings
in the picture, the labels the game's text. src/common/buttons.cpp (GameInfo::texture_bound) redraws them when bound as
the disc has them: the prompts as dark discs with coloured letters, or with the keyboard in use the key (ENTER, BKSP,
SHIFT); the page as today's pad (LT RT LB RB, View and Menu, the d-pad low left) with every line ending where the
console's did, broken where two cross. 4x pictures for the renderers (gpu::SetReplacement), and the texture's own
size into guest memory through its palette (quantised to its 256 colours). src/common/strokes.* come from True Crime.

Controls (src/common/game_info.cpp): keyboard defaults W throttle, S brake, A D steer, Space parking brake (Enter in
menus), E gear up (Backspace in menus), Q gear down, Shift nitrous, C change view, I J K L glance, H horn, Esc pause, Tab
driver info, arrows the d-pad. The pad hook swaps White and Black, so RB (the SDK's Black) changes the view, as the page
shows; LB has nothing. JUICED_MODERN_BUTTONS overrides the setting for the harness.

Evidence: REF of the controls page (captures/1112-0011-ref_05801.png: modern pad, lines meet the labels, A OK / B BACK
discs) and with KEYS (1112-0013: ENTER OK, BKSP BACK); SMOKE OK (970 draws a frame).

Next: Harry's look on Windows; a keyboard page for the controls screen (the diagram stays the pad with the keyboard);
other prompts with button pictures in the race or pause menu if any turn up (JUICED_PICS_LOG).

### 2026-11-11 - A race, the smoke test, the first Windows build

Menus (live, `cmd` presses and `snap`): Save Successful -> A -> main menu (Career, Arcade, Custom Race, Multiplay, Xbox
Live, Options, Extras) -> Arcade -> Novice series -> race 1. scripts/pad/race.txt does it from boot and holds RT.

Three stops on the way into the race:
- IDirectSoundBuffer_SetRolloffFactor (0x2781EB, .XTLID 0xB007A between SetRolloffCurve and SetVelocity) ran its
  console code on a host buffer: segfault in 0x277AD4. SDK: hosted as a no op, as SetDopplerFactor.
- A hang at frame 5520: XACT's track change (0x2A072E) stops the cue, closes the track's file (0x2A0054) and lets the
  next one start, but the old stream's WMA streamer (0x28365B, ReadFile 0x1AF1BC / GetOverlappedResult 0x1AF6F7) reads
  the closed handle once more. The read failed (C0000008), the streamer marked the end of the file at 0x160000 of a 3 MB
  track, and the decoder asked for the bytes past it for good (the fetch 0x2835F4 returns 0 past the end). On the
  console the next file opened takes the freed handle number, so that read succeeds. SDK: GameInfo::handle_reuse (on
  for Juiced) gives freed numbers back, last freed first (XBE_HANDLE_REUSE overrides).
- 0x5D780 is reached only through a pointer and follows a jump table: added as a [[function]].

Evidence: scripts/smoke.sh: SMOKE OK, race 916 draws a frame over the last 300 frames, no traps. REF frames: the grid
with HUD (frame 9600), lap 1 at 134 mph in 5th (frame 10800). Windows (windows-vm): 13ff78b builds clean and boots 103 s
without an error on WARP (0.6 frames a second, no picture or audio there).

Known: some textures (shop windows, the car's reflection) show coloured noise in the reference renderer; check on a GPU.
Bink movies' own loop waits on BinkWait (0x103550), so GameInfo::bink_frame_hold may want to be off: check movie speed.

Next: a Windows build with these on Harry's PC; then 60 frames a second and modern controller / keyboard prompts
(Harry's priorities).

### 2026-11-11 - Movies, title screen, a saved profile over the 3D city

The Bink quads' vertex program passes the vertex's w of 0 through; both renderers clamp it to 5.4e-20, which the
reference renderer then clipped (near_w 1e-6) while the GPU renderers keep it (their clip is w > 0). SDK 81598bb: a
triangle whose corners share one w is drawn at w = 1 in the reference renderer (x and y are screen positions already;
w only steers interpolation).

Evidence: scripts/harness.sh 170 (no pad): warning, THQ, Juice Games and the intro CGI play in pictures, hub_01.bik at
127 s, PRESS START over it at frame 9606, no trap. scripts/pad/boot.txt (START every 300 vblanks from 600): the movies
skip, the title passes, a profile is made and saved: "Save Successful" over the live city at frame 3606.

Next: the menus to a race (Arcade or Custom Race), then a smoke pad script (scripts/game.env SMOKE_PAD) and a first
Windows build.

### 2026-11-11 - The warning screen draws

Three missing names made every frame black, found from the reference renderer's draw lists (CAP, REF_DRAWLOG):
- D3DDevice_SetRenderState_Simple 0x2A9B60 (ecx header, edx value, 414 callers): without it no simple state (colour
  mask, blend, depth function) reached the renderer, so every draw had a colour mask of 0.
- SelectVertexShaderDirect 0x2AF740: the game draws everything with its own vertex programs and direct attribute
  formats; without it the renderer had no vertex declaration.
- The complex render state setters, all named from the library's table at 0x2B86A8 (state * 4; the scan's finds
  agree with it). TextureFactor 0x2A9530 was the one that mattered: the font combines texture with TFACTOR, which was 0,
  so the alpha test dropped every glyph.

SDK: a carry-over of the last vertex's attributes into the registers (xemu does this: pgraph_update_inline_value) was
tried and reverted; nothing here needs it yet (FINDINGS).

Evidence: scripts/harness.sh 16 REF=301: the WARNING screen with the Juice Games and THQ logos and all three lines of
text (captures/1111-2129-ref_00301.png).

Next: the Bink movies (a 640x480 YUY2 texture, format 0x24) draw black; then the title screen after the intro.

### 2026-11-11 - Boots through the logo movies headless; frames black

config/juiced.toml: every [abi] map the scan asked for, each proven from the function body (CDevice_KickOff edx;
DrawVertices stack/ecx/eax; SetStreamSource eax/edi/ebx; SetTexture ecx/eax; SetTransform eax/edx; Swap eax; ...).
Direct3D_CreateDevice gets only flags (eax), parameters (stack) and the out pointer (ecx): LTCG dropped the first three,
which the host never reads, so edx/ebx/esi stand in for them. The scan's D3DResource_IsBusy is BlockOnResource (as in
True Crime). Named by hand: SetTile 0x2AD8D0 (it crashed programming the GPU's tile registers), InitDefaultState 0x2AA7F0,
SelectVertexShaderDirect 0x2AF740 (ecx, eax), BinkWait 0x246290, BinkNextFrame 0x246D70 (from the game's movie step
0x103550; the SDK's Bink pacing called the unnamed BinkWait). Data: DirtyFlags, RenderState, IndexData, Streams,
DeferredTextureState, FvfShader (0x2AF990, called by game code, reads it from the device), and the simple render state
table 0x3108E0, found by True Crime's table contents and confirmed by its two readers.

SDK (branch `juiced`, bb0e1c0): SetViewport(NULL) keeps the viewport and clears the scissors, as the library's 0x2AD512
does; Juiced's render state setter 0x2A9900 passes NULL (the host read guest address 0).

Evidence: scripts/harness.sh 120: 7080 frames, no trap; THQ.bik, juicegames.bik and JuicedCGI_NTSC.bik open in turn.
A movie frame (CAP=1500) has 5 draws: two plain quads, the movie (640x480 YUY2) and a logo into a 640x960 target
0x01460000, then one triangle copies it into 0x03D86000 at 640x480. All use the game's own vertex programs through
SelectVertexShaderDirect.

Next: why the reference renderer shows black (REF_DRAWLOG wrote an empty list at frame 1499): the programs, their
constants (pushed by the game), or which surface is shown.

### 2026-11-11 - Repository set up

Disc: USA/Europe, title 545100EF, default.xbe SHA-1 b38b935794b05e46b70b164acdcca9338f0b692f, built 2005-03-25 as
d:\Src\game\mainGame\Xbox_Release\mainGame.exe. Libraries all 1.0.5849: XAPILIB XBOXKRNL LIBCMT D3D8LTCG D3DX8
XGRAPHCL DSOUND XACTENLT XONLINES XVOICE VOICMAIL. Code sections include BINK*, WMADEC, XACTENG, XONLINE, XNET.

Scan (fork main 26f372b): 157 of 278 functions, 6 of 13 data. The scan placed 17 LTCG D3D functions by constants
whose arguments are in registers; each needs its [abi] proven. Misses: about 70 D3D functions, the D3D data, Bink's
GetSummary/NextFrame/Wait (a 2002 Bink, BinkDoFrame matched only at 0.69), some DSOUND setters; SetFilter and SetEG
ambiguous.

Push buffers (PushBufferScan): no push buffer resource is created (no 0x20001 tag); the 0x20000 tests are the
inlined resource Register. Game code writes commands into the device push buffer through an inlined MakeSpace: push
pointer 0x2BC378, D3D_MakeRequestedSpace 0x2AE740 when short; 0x1575C0 writes SET_BEGIN_END, an INLINE_ARRAY quad,
End. About 50 such stores in .text (0x121240, 0x12A2F0, 0x1575C0-0x157C70, 0x1C8140, ...) and Bink's blitters
(0x244A30, 0x24CBE0-0x24D1C0).

Next: prove the [abi] maps, name the D3D misses, first boot.
