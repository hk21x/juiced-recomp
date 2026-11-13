# Juiced recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Harry on Windows, f6215b1 (3440x1440): "blur mostly fixed"; lines across the screen (the scene's wires, line strips a console pixel wide) and everything stretched. SDK 7c45c2e + src/game/widescreen.cpp: 4:3, 16:9 and the screen's shape, headless only so far
- Widescreen: 3D 16:9 by the game, wider by fov.cpp; 2D squeezed to a centred 4:3 by the SDK (flat programs); the race HUD sits in that 4:3 part (not spread to the edges yet)
- The music freeze (Harry on Windows, 4 s into a race) is fixed in the SDK (7168ae0): not yet confirmed on Windows
- Settings (F1 window): Modern button pictures (on), Skip intro movies (off)
- PC settings in the game's menus: Options > PC SETTINGS (title menus and the pause menu), three pages of seven rows; headless only so far
- Field of view (Game page of PC SETTINGS, fov_percent 50 to 150), the keyboard's own controls page, the mouse in the front end's menus (point, left click A, right click B): headless only so far
- Controls: modern button pictures (setting modern_buttons, on): the prompts' A B Y and the controls page's pad; keyboard defaults (W S throttle and brake, A D steer, ...), the prompts show the keys while the keyboard plays; RB changes the view (White and Black swapped)
- Milestone: 3 - a race headless: scripts/pad/race.txt drives race 1 (Arcade, Novice, San Ricardo route 1) at full throttle; SMOKE OK (916 draws a frame). Windows: builds and boots (windows-vm, 13ff78b; WARP only, no picture there)
- XDK: 5849 with D3D8LTCG, 157 of 278 functions, 6 of 13 data (first scan); 20 LTCG argument maps proven (config/juiced.toml)
- Push buffers: none created or replayed; game code writes NV2A commands into the device's push buffer itself
- GitHub: private hk21x/juiced-recomp; SDK on fork branch `juiced` (7c45c2e: flat programs' 2D squeeze, no_squeeze, line_width; 4c5a099: the tall frame's resolve pass keeps plain taps; 0de0637: the double height frame's stretch; 9ed6b2f: the threaded backend's freed header guard; on 7168ae0 = fork main 26f372b + SetViewport(NULL), a reference renderer fix, DSOUND: buffer SetRolloffFactor hosted and a stream's ENVELOPECOMPLETE status; not yet on fork main: needs the all-games gate)

## Log

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
