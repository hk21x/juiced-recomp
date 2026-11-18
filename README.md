# Juiced - XBE Recomp

![Juiced - native PC port](docs/img/juiced-pc-port.webp)

An unofficial native PC port of the original Xbox version of Juiced. The game's own x86 code is statically
recompiled with the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool. It is not an emulator.

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of
> the game to build and play.

## Status

In progress. Races and the menus play on Windows, with widescreen, high frame rates and the PC settings
below. Not yet covered: a long career, split screen and wet races. With Smooth motion above 60 the neon under the cars
has been seen to flicker in Showoff events; turn Smooth motion off in F1 if you see that.

## Why a recompilation and not a mod of the PC version

Juiced also came out for Windows, and that version can be patched. We recompiled the Xbox game instead, for these
reasons:

- **The whole game is ours to change.** Recompiling turns the game's code into C++ that we build ourselves, and the
  renderer, sound and input under it are written new. A feature is then a change in source, not a patch laid over a
  closed program and the graphics calls it happens to make. The frames drawn between the game's steps, the menus kept
  at 4:3 with the race widened, and the PC Settings page inside the game's own menus were built that way.
- **One runtime for several games.** This port shares its runtime with the other Xbox ports made with the same
  toolkit. Widescreen, anti-aliasing, controllers and wheels, achievements and the settings window come from it, and a
  fix made for one game reaches the others.
- **It runs as a present-day program.** 64 bit, Direct3D 12, with no old graphics or input layers to keep working on
  new versions of Windows.
- **It starts from the Xbox disc.** Anyone with the console game can build and play it; the PC release is not needed.

Mods of the PC version are a good route too. This is a different one, and the two do not get in each other's way.

## Supported version

The USA/Europe Xbox release (one disc for both), title id 545100EF. `default.xbe` SHA-1
`b38b935794b05e46b70b164acdcca9338f0b692f`.

## Getting started

A launcher, as the other ports have, will come with the first release. Until then, build the game by hand: see
[Building](#building).

**You need:** Windows 10 or 11, 64 bit, a GPU with Direct3D 12, and your own copy of the game (an `.iso` image of
your disc, or a folder with its files).

**Minimum spec:** any 64 bit processor from the last ten years and a Direct3D 12 graphics card run the game at its own
60 frames a second. Frame rates above 60 (Smooth motion) need more: the frames in between are drawn in the time the
game leaves free in each of its 60 steps a second, so they depend on one fast processor core more than on the
graphics card. As a guide from our own machines: a Ryzen 7 5800X3D shows 180 to 240 frames a second at 3440x1440; a
Ryzen 5 5600G (in a virtual machine, with an Intel Arc A750) has no time left for extra frames at 1080p and stays at
60. When there is no room, the game just runs at 60: nothing breaks.

## Screenshots

<p align="center">
  <img src="docs/img/juiced-pc-port.webp" alt="A blue Mitsubishi FTO on the start line under the chequered gantry, at 3440x1440" width="800">
  <br>
  <i>On the start line, 3440x1440, with the HUD kept in its 4:3 centre.</i>
</p>

## Features

Set in the F1 window (or `settings.toml` beside the exe); the runtime's own display, graphics and controller options
are there too.

- **Widescreen** - True proportions on any screen, 21:9 and wider included. Races fill the screen with the 3D view
  widened. The menus, the title and the movies keep their 4:3 shape in the middle, with a dimmed, blurred copy of
  the picture filling the sides (Direct3D 12; black bars with Direct3D 11).
- **Aspect, changed as you play** - Auto, 4:3, 16:9 or Fill, in PC Settings or F1. It applies at once, in a race too;
  at 4:3 the race camera is the console's own.
- **HUD position** - On a wide screen the race's dials, map and position sit at the screen's sides, or in the 4:3
  centre nearer the car.
- **Hide the HUD** - F9 in a race (or PC Settings) leaves out the dials, map, times and position, for screenshots
  and a clear view. The pause menu still shows.
- **High frame rates** - The game runs its 60 steps a second as on the console, as its driving needs. Above a frame
  rate cap of 60, races draw the frames in between, made from the two steps before. Turn Vertical Sync off for the
  most frames.
- **Field of view** - 50 to 150% of the game's race camera.
- **Speed blur** - The blur towards the screen's edges at speed, on or off.
- **Engine and effects level** - The disc's race mix is much louder than its menus and clips; this sets the engines
  and effects against the music (50% by default).
- **Skip intro** - Starts without the logo movies, the intro and the warning pages.
- **Modern button pictures** - Prompts and the controls page show today's Xbox pad; on keyboard, your keys.
- **PC Settings** - In the game's own menus: Options > PC SETTINGS on the title menus and the pause menu.
- **Mouse in the menus** - Point at an item and click to select it; right click goes back. The pointer is drawn in
  the menus' own colours.
- **Window** - Fullscreen, borderless or a window; Alt+Enter switches.
- **Achievements** - A first set for race results (finishing, the podium, wins by race type, a ten second
  sprint), kept in `achievements.toml` beside the game. More, for the career, will follow.
- **Screenshots** - F12 saves the frame as a full resolution PNG in the `screenshots` folder beside the game, without
  the settings window.
- **Sharp 2D** - Text and HUD pictures enlarged with sharp edges.
- **Controllers and wheels** - Xbox, PlayStation and Switch pads with hot plugging and rumble; steering wheels with
  force feedback.

### Keyboard

| Control | Key |
| --- | --- |
| Throttle / brake | W / S |
| Steer | A / D |
| Handbrake | Space |
| Gear up / down (and reverse) | E / Q |
| Nitrous | Left Shift, N |
| Change view | C |
| Glance left / right / back | J / L / K |
| Horn | H |
| Hide the HUD | F9 |
| Pause | Esc, P |
| Driver info | Tab |
| Menus | Arrows move, Enter selects, Backspace goes back |

Keys can be changed in the F1 window. On a pad, RB changes the view.

## Graphics settings

In PC Settings or the settings window (F1, Graphics). These are shots of one spot at 3440x1440, taken one after
another.

<p align="center">
  <img src="docs/img/juiced-settings-wipe.webp" alt="A wipe across the start line between all settings off and render scale 200%, MSAA 4x and sharpening 50%" width="800">
  <br>
  <i>All off vs. render scale 200%, MSAA 4x and sharpening 50%.</i>
</p>

<p align="center">
  <img src="docs/img/juiced-settings-edges.webp" alt="Close-up of the start gantry's lattice and overhead wires with all off, MSAA 4x, FXAA and render scale 200%" width="800">
  <br>
  <i>Edges, enlarged twice: all off, MSAA 4x, FXAA and render scale 200%.</i>
</p>

- **MSAA** smooths the edges of the 3D scene and keeps textures sharp. The more samples, the slower.

<p align="center">
  <img src="docs/img/juiced-settings-msaa.webp" alt="The gantry and wires with MSAA off, 2x, 4x and 8x" width="800">
  <br>
  <i>MSAA off, 2x, 4x and 8x, enlarged twice.</i>
</p>

- **Edge smoothing** works on the finished frame, HUD included: FXAA softens it a little, SMAA keeps it crisper.

<p align="center">
  <img src="docs/img/juiced-settings-smaa.webp" alt="The gantry and wires with edge smoothing off and with SMAA" width="800">
  <br>
  <i>Edge smoothing off vs. SMAA, enlarged twice.</i>
</p>

- **Smooth alpha edges** soften fences, palm leaves and other cut-out textures, with MSAA on.
- **Texture filtering** keeps roads and walls sharp into the distance.
- **Scaling filter** chooses how the frame reaches the screen when render scale is not 100%: bilinear, or sharp.
- **Enhanced textures** double the 3D scene's textures in the background. They use more video memory and can cause a
  brief hitch as new ones appear.
- **Render scale** draws the picture larger than the screen: the smoothest fine detail, and the slowest.

<p align="center">
  <img src="docs/img/juiced-settings-sharpness.webp" alt="The gantry and wires with sharpening off, 50% and 100%" width="800">
  <br>
  <i>Sharpening off, 50% and 100%, enlarged twice.</i>
</p>

- **Sharpening** makes the picture crisper. Around 50% is a good start.

## Community and support

Join the [Discord server](https://discord.gg/PQjkKsdtc) for help getting the game running, to report problems, and
to talk about the port with other players.

## Tips and troubleshooting

- **The most frames above 60:** set a Frame Rate Cap above 60 and turn **Vertical Sync off**. The game's own 60 steps
  a second get whole frames between them, so the counter settles on 120, 180 or 240.
- **Flickering lights or neon:** turn **Smooth motion above 60** off in F1. The game then runs at 60.
- **Something wrong:** the first lines of `logs\juiced.log` beside the game list your settings and GPU. Please include
  that file when you report a problem.

## Building

For developers.

1. Clone with submodules: `git clone --recurse-submodules https://github.com/hk21x/juiced-recomp`.
2. Put your disc in `assets`: an `.iso`, or `default.xbe` with the game's folders beside it.
3. Install Visual Studio 2022 with **Desktop development with C++**, **C++ Clang tools for Windows** and **C++ CMake
   tools for Windows** (`build.bat` offers to add the last two), and run `build.bat`. The first build recompiles the
   game and takes a while. The exe is in `out\build\release`.

## Credits

- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp), the recompiler and runtime this is built on.
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) for the overridable function layout and instruction hooks.
- [Zydis](https://github.com/zyantific/zydis) for decoding x86 instructions, [toml++](https://github.com/marzer/tomlplusplus)
  for the configs, [Dear ImGui](https://github.com/ocornut/imgui) for the settings overlay and
  [SDL](https://github.com/libsdl-org/SDL) for the window, input and controllers.
