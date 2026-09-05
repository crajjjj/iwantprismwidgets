# iWant Widgets — Prisma Edition

A drop-in reimplementation of DaemonPrime's **iWant Widgets** (MIT) that renders
through **PrismaUI** (HTML/CSS/JS via Ultralight) instead of Scaleform/Flash.

**iWant Status Bars, its MCM, every Status Bars addon (e.g. SL Widgets), and
existing DDS icon packs run unchanged.** Consumers find the widget library at
runtime by catching the `iWantWidgetsReset` mod event and casting its sender to
the `iWant_Widgets` script type — no plugin master, no FormID binding — so a
same-named script backed by natives slots straight in.

> **Status: builds clean, not yet tested in game.** The DLL compiles
> (VS2022/xmake), all three scripts compile against real SKSE sources, and the
> ESP + SEQ are authored (via houseCARL — no CK session needed). What remains
> is the in-game smoke test below.

## Why

The Flash original has structural problems this edition removes at the root:

| Flash original | Prisma Edition |
|---|---|
| Widgets vanish on save load (Flash IDs die with the HUD menu) | View is session-lifetime; ids invalidated only by the explicit reset event, same rebuild flow consumers already implement |
| Return values polled from Flash (`outputReady` spin loops, race-prone) | Natives return directly; the `loadInProgress` mutex dance is gone |
| Pipe-delimited string protocol (documented Cyrillic issues) | JSON over PrismaUI interop |
| 1280×720 Flash stage upscaled by the game (blurry icons) | Same virtual 1280×720 coordinate space, but rendered at native resolution |
| Requires SkyUI's widget loader | SkyUI not required by the widget layer |

## Architecture

```
iWant Status Bars / addons (UNCHANGED .pex)
        │  same Papyrus calls
        ▼
iwant_widgets.psc      ← this repo: same script name & signatures, extends Quest
        │  Global Native calls
        ▼
iWantWidgetsNative.psc ↔ iWantWidgetsPrisma.dll   (id allocation, JSON ops,
        │                                          DDS→PNG decode, metrics cache)
        ▼  InteropCall("iwCall", json)
PrismaUI/views/iwantwidgets/index.html            (widget registry, transforms,
                                                   tweens, shapes, meters, text)
```

- Widget model is preserved from `iWantWidgets.as`: `(x, y)` is the widget's
  **center**; rotation/scale/alpha apply to the whole widget; `setRGB`
  reproduces Flash's `ColorTransform.rgb` flat-tint via CSS `mask-image`.
- `drawShapeLine/Circle/Orbit`, easing classes (`regular/strong/back/bounce/
  elastic` × `in/out/inout`), and TweenMax-style tween overwrite are ported 1:1.
- The `setSkyrim*` family pokes the **vanilla** Scaleform HUD via `UI.*` and
  never touched the Flash widget — carried over verbatim, works as before.
- Reset flow: player alias fires on `OnInit`/`OnPlayerLoadGame` →
  `triggerReset()` → native `Reset()` (view wipes all widgets, old ids go
  stale) → `SendModEvent("iWantWidgetsReset")` → consumers rebuild, exactly as
  they do today.

## Repo layout

```
Source/Scripts/iwant_widgets.psc            drop-in fork (same public API as 1.33)
Source/Scripts/iWantWidgetsNative.psc       native declarations
Source/Scripts/iwant_widgets_prisma_alias.psc  reset trigger (player alias)
PrismaUI/views/iwantwidgets/index.html      the renderer
plugin/                                     SKSE plugin (xmake + CommonLibSSE-NG)
```

## Building

### SKSE plugin

Mirrors the PrismaUI example plugin toolchain (VS2022, C++23, xmake):

```sh
cd plugin
git submodule add -b ng https://github.com/alandtse/CommonLibVR.git lib/commonlibsse-ng
git submodule update --init --recursive
xmake f -m release
xmake
```

Output: `iWantWidgetsPrisma.dll`. DirectXTex is pulled via xmake-repo
(`add_requires("directxtex")`).

### Papyrus scripts

Compile the three `.psc` with the standard SkyrimSE compiler. Required import
sources: vanilla scripts + SKSE scripts (`UI.psc`, `Utility.psc`). **SkyUI
sources are NOT needed** (that's the point).

### The plugin (.esp)

`iWant Widgets Prisma.esp` ships in the repo root (authored with houseCARL, no
CK needed): quest `iWantPrismaWidgetQuest` (Start Game Enabled) carrying the
`iwant_widgets` script, plus a PlayerRef-forced `PlayerAlias` carrying
`iwant_widgets_prisma_alias`. `SEQ/iWant Widgets Prisma.seq` accompanies it —
without the SEQ a start-game-enabled quest in an .esp silently never starts.

Any plugin filename works — verified: `iWant Status Bars.esp` has **no
masters** and no reference to the original `iWant Widgets.esl`.

## Install layout (users)

```
Data/
├── iWant Widgets Prisma.esp
├── SEQ/iWant Widgets Prisma.seq
├── Scripts/iwant_widgets.pex               ← overrides the original's script
├── Scripts/iWantWidgetsNative.pex
├── Scripts/iwant_widgets_prisma_alias.pex
├── SKSE/Plugins/iWantWidgetsPrisma.dll
├── PrismaUI/views/iwantwidgets/index.html
└── Interface/exported/widgets/iwant/widgets/library/*.dds   ← bundled (MIT)
```

- **Disable** the original iWant Widgets mod (its ESL, `iWantWidgets.swf`, and
  `iwant_widgets.pex`); the library DDS icons are bundled here, so nothing
  from it is needed. If you keep it enabled instead, this mod must win the
  `iwant_widgets.pex` conflict.
- **Keep** iWant Status Bars installed and untouched.
- Requires: SKSE, Address Library, [PrismaUI](https://www.nexusmods.com/skyrimspecialedition/mods/148718)
  (+ its Media Keys Fix requirement).
- Mid-playthrough swap works: the widgets script holds no meaningful save
  state; expect one-time Papyrus log warnings about the orphaned original
  quest.

## First build & smoke test

1. Build DLL + compile scripts + make the ESP (above).
2. In-game with iWant Status Bars + an addon (SL Widgets): load a save →
   `iWantWidgetsPrisma.log` should show view creation and DOM ready; bars
   should appear at their configured positions.
3. Exercise: move a bar in Status Bars' MCM (line + circle + orbit types),
   toggle icons, save + reload (widgets must come back), check icon tinting
   (state colors) and NPC name labels (SL Widgets `loadText`).
4. `coc qasmoke` from main menu for a clean-save variant of the same.

## Parity notes / known gaps

- **VR is not supported** (PrismaUI limitation). VR users stay on the Flash
  original.
- `getXsize`/`getYsize` on **text** widgets are measured in the view and
  reported back asynchronously — a call in the same Papyrus instant as
  `loadText`/`setText` may read 0. (DDS widgets report instantly from the
  decoder; Status Bars never calls these.)
- `loadWidget` with a `.swf` path cannot render (logged, becomes invisible).
  DDS/PNG/JPG all work, from loose files or BSAs.
- Shape draws treat a nonexistent widget id as skippable even when
  `skipInvisible = False`; the Flash original consumed an angle/offset step in
  one sub-case. No known consumer hits this (Status Bars hard-codes
  `skipInvisible = True`).
- Meter visuals are a stylistic approximation of SkyUI's meter (334×30,
  light→dark gradient, flash overlay). `percent` transitions and fill
  directions (`left`/`right`/`both`) behave as documented.
- Fonts: `$EverywhereFont` etc. map to CSS stacks in `index.html` (`FONTS`).
  Drop `.ttf` files next to the view and add `@font-face` for exact matches.
- `doTransition` keeps the original's quirk of running at half the advertised
  frame rate (fps=30 vs 60-frame defaults) because consumers tuned against it.

## Credits

- **DaemonPrime** — iWant Widgets & iWant Status Bars (MIT), whose API design
  made a drop-in replacement possible.
- **StarkMP** — PrismaUI; `PrismaUI_API.h` is copied from the official example
  plugin as its header instructs.
