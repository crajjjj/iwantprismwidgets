# iWant Widgets — Prisma Edition

A drop-in reimplementation of DaemonPrime's **iWant Widgets** (MIT) that renders
through **PrismaUI** (HTML/CSS/JS via Ultralight) instead of Scaleform/Flash.

**iWant Status Bars, its MCM, every Status Bars addon (e.g. SL Widgets), and
existing DDS icon packs run unchanged.** Consumers find the widget library at
runtime by catching the `iWantWidgetsReset` mod event and casting its sender to
the `iWant_Widgets` script type — no plugin master, no FormID binding — so a
same-named script backed by natives slots straight in.

(SL Widgets bundles its own patched fork of the Status Bars *scripts* — bug
fixes plus small accessors its NPC tracking needs. That fork lives in the SL
Widgets repo, not here; it is orthogonal to the rendering layer this mod
replaces and works with the Flash original too.)

> **Status: in-game and iterating.** Builds clean (DLL via VS2022/xmake, all
> scripts against real SKSE sources), plugin + SEQ authored via houseCARL, and
> loading/rendering has been exercised in a live load order. Rendering,
> menu/scene hiding, image formats (incl. animated), and the init handshake
> have each been fixed against in-game behavior — see the git log.

## Why

The Flash original has structural problems this edition removes at the root:

| Flash original | Prisma Edition |
|---|---|
| Widgets vanish on save load (Flash IDs die with the HUD menu) | View is session-lifetime; ids invalidated only by the explicit reset event, same rebuild flow consumers already implement |
| Return values polled from Flash (`outputReady` spin loops, race-prone) | Natives return directly; the `loadInProgress` mutex dance is gone |
| Pipe-delimited string protocol (documented Cyrillic issues) | JSON over PrismaUI interop |
| 1280×720 Flash stage upscaled by the game (blurry icons) | Same virtual 1280×720 coordinate space, but rendered at native resolution |
| Requires SkyUI's widget loader | SkyUI not required by the widget layer |
| Icons limited to DDS (SWF for anything fancier) | Decodes **DDS, PNG, JPG, GIF (incl. animated), and `_<N>f` spritesheets** — any format the consumer requests |

## Architecture

```
iWant Status Bars / addons (UNCHANGED .pex)
        │  same Papyrus calls
        ▼
iwant_widgets.psc      ← this repo: same script name & signatures, extends Quest
        │  Global Native calls
        ▼
iWantWidgetsNative.psc ↔ iWantWidgetsPrisma.dll   (id allocation, JSON ops,
        │                                          WIC image decode, metrics cache)
        ▼  InteropCall("iwCall", json)
PrismaUI/views/iwantwidgets/index.html            (widget registry, transforms,
                                                   tweens, shapes, meters, text)
```

- Widget model is preserved from `iWantWidgets.as`: `(x, y)` is the widget's
  **center**; rotation/scale/alpha apply to the whole widget. Images render on a
  `<canvas>` from raw RGBA (WIC-decoded in the DLL), and `setRGB` reproduces
  Flash's `ColorTransform.rgb` flat-tint per-pixel — Ultralight has no CSS
  `mask-image`, which is why the canvas path exists.
- `drawShapeLine/Circle/Orbit`, easing classes (`regular/strong/back/bounce/
  elastic` × `in/out/inout`), and TweenMax-style tween overwrite are ported 1:1.
- The `setSkyrim*` family pokes the **vanilla** Scaleform HUD via `UI.*` and
  never touched the Flash widget — carried over verbatim, works as before.
- Reset flow: player alias fires on `OnInit`/`OnPlayerLoadGame` →
  `triggerReset()` → native `Reset()` (view wipes all widgets, old ids go
  stale) → `SendModEvent("iWantWidgetsReset")` → consumers rebuild, exactly as
  they do today. The view outlives save-loads, so a reset is only needed once
  per game launch — when the fresh view's ids and a save's stored ids first
  disagree. The alias gates on the native `NeedsResync()` and skips the reset
  on later same-session loads, since each one costs every consumer a full
  icon reload.

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

Output: `iWantWidgetsPrisma.dll`. Image decoding uses Windows' built-in WIC, so
there is no third-party image dependency to fetch.

### Papyrus scripts

Compile the three `.psc` with the standard SkyrimSE compiler. Required import
sources: vanilla scripts + SKSE scripts (`UI.psc`, `Utility.psc`). **SkyUI
sources are NOT needed** (that's the point).

### The plugin — `iWant Widgets.esl`

The plugin ships under the **same filename as the original** (`iWant Widgets.esl`,
light-flagged) so a mod manager serves it in place of the original by load
priority — the same shadowing our `.pex` files already do for the scripts. It
is **not** a patch and takes **no master**: it is a self-contained light plugin
authored with houseCARL (no CK), carrying quest `iWantPrismaWidgetQuest` (Start
Game Enabled) with the `iwant_widgets` script plus a PlayerRef-forced
`PlayerAlias` running `iwant_widgets_prisma_alias`. `SEQ/iWant Widgets.seq`
accompanies it — without the SEQ a start-game-enabled quest never starts.

Because the filename matches, install this mod at **higher priority than the
original iWant Widgets**, which stays installed to supply its DDS icon assets
(this mod ships no icons). Our higher-priority `iWant Widgets.esl` +
`iwant_widgets.pex` override the original's plugin and script while its loose
assets still deploy. Consumers bind by the `iWant_Widgets` script type via the
`iWantWidgetsReset` event, so nothing depends on the plugin's FormIDs.

## Install layout (users)

```
Data/
├── iWant Widgets.esl                        ← same name as the original (replaces it)
├── SEQ/iWant Widgets.seq
├── Scripts/iwant_widgets.pex                ← overrides the original's script
├── Scripts/iWantWidgetsNative.pex
├── Scripts/iwant_widgets_prisma_alias.pex
├── SKSE/Plugins/iWantWidgetsPrisma.dll
└── PrismaUI/views/iwantwidgets/index.html
```

- Icon DDS are **not bundled** — they stay in their source mods: the original
  iWant Widgets provides its generic `.../library/*.dds` set, and each Status
  Bars addon (SL Widgets, packs) ships its own icons under the same
  `widgets/iwant/widgets/library/` tree. This mod is code/plugin/view only.
- **Keep the original iWant Widgets mod installed and enabled** (left pane) so
  its loose DDS deploy; this mod, at **higher priority**, overrides its
  `iWant Widgets.esl` + `iwant_widgets.pex` while leaving its assets in place.
  (SL Widgets' own icons live in the SL Widgets mods, so SL Widgets renders
  even if the original iWant Widgets library icons are absent.)
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
- **File formats.** The renderer decodes exactly the file the consumer asks
  for, by its extension:
  - `.dds` (incl. BC-compressed), `.png`, `.jpg`/`.jpeg` — static images.
  - `.gif` — animated (per-frame delays honored; composited via GIF disposal
    rules). A single-frame GIF renders static.
  - **Spritesheet** `name_<N>f[@ms].<ext>` — `N` frames played in sequence at
    `ms` per frame (default 100), e.g. `flame_8f.png` or `pulse_4f@70.dds`.
    Frames stack vertically (preferred) or horizontally; works for any of the
    formats above, DDS included. 64-frame cap.
  It is **format-agnostic and does no extension guessing or folder scanning** —
  which file to request is the consumer's job. Pack authors opt in per consumer:
  - **SL Widgets** auto-prefers a `.gif` then `.png` sibling of the expected
    `.dds` (`slw_util.resolveIconFiles`), so a pack just drops `aroused0.png`
    or an animated `aroused0.gif` beside `aroused0.dds` — ship all of one
    icon's states in the same format.
  - Other consumers get the format by passing that path to `loadWidget`
    directly (`loadWidget("…/foo_8f.png")`).
- `loadWidget` with a `.swf` path cannot render (logged, becomes invisible).
- Shape draws treat a nonexistent widget id as skippable even when
  `skipInvisible = False`; the Flash original consumed an angle/offset step in
  one sub-case. No known consumer hits this (Status Bars hard-codes
  `skipInvisible = True`).
- Meter visuals are a stylistic approximation of SkyUI's meter (334×30,
  light→dark gradient, flash overlay). `percent` transitions and fill
  directions (`left`/`right`/`both`) behave as documented.
- Fonts: `$EverywhereFont` etc. map to CSS stacks in `index.html` (`FONTS`).
  Drop `.ttf` files next to the view and add `@font-face` for exact matches.
- `doTransition` **snaps to the target value instead of animating.** Ultralight
  does not tick timers in an unfocused overlay the way Flash ticked frames, and
  consumers re-issue transitions (e.g. `setTransparency`) faster than any fade
  completes — which stranded icons part-way through a fade, positioned and
  sized but invisible. Snapping makes the end state deterministic; every
  transition still ends exactly where the Flash original ended, just without
  the intermediate frames. Delays are still honored.

## Credits

- **DaemonPrime** — iWant Widgets & iWant Status Bars (MIT), whose API design
  made a drop-in replacement possible.
- **StarkMP** — PrismaUI; `PrismaUI_API.h` is copied from the official example
  plugin as its header instructs.
