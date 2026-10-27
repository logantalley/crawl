# crawl: First-Person Dungeon Crawler Engine Spec

Oct 23, 2026 · @Log

## Summary and pillars

`crawl` is a small C11 engine and toolset for grid-step, first-person party RPGs in the style of Etrian Odyssey and Might and Magic III–V. A campaign is plain text: ASCII maze maps, TOML events, dialogue, items and rules content, plus SVG art. One dev binary plays, edits, validates and packages campaigns. A packaged game is a small runtime binary plus a `.pak` file. Engine code is ISC. Rules sit behind a narrow C interface: the primary pack is 3.5e SRD under OGL 1.0a (`srd35`), alongside an original `lite` pack.

| Pillar | Meaning |
| --- | --- |
| Data over code | Maps, events, NPCs, items, monsters, spells, shops are files. Code holds only verbs. |
| Diffable text | Every source asset is hand-editable, readable in `git diff`, and validated by `crawl check`. |
| Tiny and terse | Six vendored single-file deps. Short names, short comments, no frameworks. |
| Swappable rules | The engine knows actors, HP, turns and a log. Rules packs own everything else. |
| Resolution independent | All art is SVG, rasterized and cached at window resolution. |

## Goals and non-goals

v1 ships a playable 3-floor demo with editor and packager on four OSes; everything else waits.

**v1 goals**

- Grid-step movement, 4 facings, thin walls on cell edges: wall, door, locked, secret, false wall, one-way.
- Slice-based first-person view from SVG tilesets; outdoor tilesets use the same format.
- Party of up to 6 with portraits; front/back rows (EO) or 6 slots (M&M), set by the rules pack.
- Event markers: dialogue, battle, shop, teleport, stairs, trap, chest, spinner, dark zone, gathering, flags.
- FOEs visible in view and on the automap, stepping each player step.
- Automap with auto-record plus EO-style player annotations.
- Menu town hub: inn, shop, guild, tavern.
- Turn-based menu combat through the rules API.
- Two rules packs: `lite` (original, ISC) and `srd35` (3.5e SRD, OGL 1.0a).
- Editor for maps, markers, edges, zones, FOE paths, stair links; live 3D preview and playtest.
- Packager producing runnable builds for Windows, Linux, macOS, OpenBSD.

**Non-goals (v1)**

- Real-time 3D, smooth-scroll movement (fades only), free camera.
- A scripting language (no Lua/Wren). Events are condition→action lists.
- Networking; runtime-loaded untrusted mods; Steam/console SDKs.
- Walkable maze towns (towns are menus; a maze with shop markers still works).
- Code signing / notarization.
- Rich dialogue editing. Use an external editor; the engine hot-reloads.
- `srd51` (CC-BY-4.0) pack: v2.

## Language and dependencies

C11: every dependency is a single-file C library, it builds identically with MSVC, clang and OpenBSD's toolchain, and it fits a terse style. Rust brings a crate tree and lagging SDL bindings; C++ is only justified by Dear ImGui, which the editor doesn't need. Memory-safety mitigations: load-time arenas freed whole, fixed-size IDs, ASan/UBSan in CI, parser fuzzing at M5.

| Dep | Use | License | Form |
| --- | --- | --- | --- |
| SDL3 | window, input, audio, 2D renderer, file dialogs, pref path | zlib | submodule, static; OpenBSD may use system package |
| nanosvg + nanosvgrast | SVG parse and rasterize | zlib | 2 headers |
| microui | editor UI and in-game menus | MIT | 2 files |
| tomlc17 | TOML parse | MIT | 2 files |
| miniz | `.pak` (zip) | MIT | 2 files |
| stb\_truetype | text (nanosvg has no `<text>`) | MIT / public domain | 1 header |
| OFL font | UI text | SIL OFL 1.1 | data |

Fallbacks: Nuklear (single header, more widgets) if microui is too thin for the editor; PlutoSVG if nanosvg fidelity falls short.

**SVG subset, enforced by `crawl check`:** paths, basic shapes, solid fills/strokes, linear/radial gradients, `viewBox`, transforms. No text, filters, masks, CSS or embedded images. Author in Inkscape, save as Plain SVG.

## Architecture

`core/` has no SDL dependency, so it is headless-testable and `crawl check` runs anywhere, including CI. Per the convergence decision, the generic half of `core/` moves into the shared `tome` library at crawl M2 (gate G1).

| Module | Responsibility |
| --- | --- |
| `core/db` | Load TOML into interned IDs and arena tables; resolve references |
| `core/map` | Parse ASCII maze; per-cell edge bitmask, zones, markers, FOEs |
| `core/ev` | Event VM: match conditions, step actions, yield on blocking verbs |
| `core/dlg` | Dialogue graph: nodes, options, conditions, portraits |
| `core/dice` | `NdM+K` and `$var` expressions, seeded RNG |
| `core/party` | Actors, inventory, gold, flags, vars (generic stat bag) |
| `core/save` | Save/load TOML in SDL pref dir |
| `core/pak` | Read/write `.pak`; find a pak appended to the exe |
| `core/check` | Validate refs, reachability, SVG subset, license metadata |
| `rules/` | `rules.h` + compiled-in packs |
| `gfx/svg` | SVG → RGBA → texture; cache keyed by (path, scale) |
| `gfx/view` | Compose first-person view from slices and sprites |
| `gfx/amap` | Automap and minimap |
| `gfx/ui` | microui glue, text, portraits, message box |
| `game/` | Play state machine: explore, dialogue, combat, town, menus |
| `edit/` | Editor screens and tools |
| `plat/` | SDL3 init, input map, main loop, file watch |

```
crawl/
  CMakeLists.txt  LICENSE(ISC)  README.md  CREDITS.md
  src/
    main.c                 # subcommand dispatch
    core/  db.c map.c ev.c dlg.c dice.c party.c save.c pak.c check.c
    rules/ rules.h reg.c common.c
           lite/lite.c
           srd35/srd35.c
    gfx/   svg.c view.c amap.c ui.c
    game/  play.c explore.c combat.c town.c menu.c
    edit/  edit.c canvas.c insp.c link.c
    plat/  sdl.c
  vendor/  SDL/ nanosvg.h nanosvgrast.h microui.[ch] toml.[ch] miniz.[ch] stb_truetype.h
  data/
    rules/lite/   LICENSE(ISC)  *.toml
    rules/srd35/  OGL.txt OGC.md SEC15.md *.toml
    tiles/stone/  LICENSE(CC0)  *.svg
    fonts/        OFL.txt *.ttf
  examples/demo/  campaign.toml maps/ ev/ dlg/ items/ art/
  tests/          *.c fixtures/
  .github/workflows/ci.yml
```

**Binaries:** one source tree, two CMake targets.

| Target | Contents | Use |
| --- | --- | --- |
| `crawl` | everything | `crawl play`, `edit`, `pack`, `check`, `slices` |
| `crawl-rt` | `-DCRAWL_NO_EDITOR`: play only | shipped inside packaged games |

The editor's 3D preview and playtest are the runtime, so one dev binary shares code for free. CI produces `crawl-rt` for all four OSes, so the packager can target any OS from any OS.

## Map file format

Recommendation: an ASCII edge grid as source of truth, inside a TOML file that also holds the legend, edge properties, zones and FOEs. At load it compiles to `u8 edge[4]` + `u16 mark` per cell.

| Option | Thin walls | Hand-editable | Diffable | Verdict |
| --- | --- | --- | --- | --- |
| **A. ASCII edge grid** (2W+1 × 2H+1) | native: one glyph per edge | excellent | excellent | **pick** |
| B. Bitmask grid + layers (int arrays) | native (4 bits/cell) | poor | poor | internal form after load |
| C. Tiled TMX | awkward: object layers or doubled tiles | Tiled only | XML noise | reject |

**Grid rules.** Map is W×H cells; text grid is (2W+1)×(2H+1). Origin top-left, x east, y south. Position (2x+1, 2y+1) is a cell; odd column on an even row is a horizontal edge; even column on an odd row is a vertical edge; `+` is a corner and ignored.

| Edge glyph | Meaning |
| --- | --- |
| `-` `\|` | wall |
| space | open |
| `d` | door |
| `l` | locked door (key in `[[edge]]`) |
| `s` | secret door (looks like wall until found) |
| `f` | false wall (walk through) |
| `^` `v` | one-way, passable only north / south |
| `<` `>` | one-way, passable only west / east |

Cell glyphs: `.` floor, `#` rock, anything else is a marker defined in `[legend]`. Zones (dark, damage, anti-magic, encounter rate) are rectangles, so a cell can be a chest *and* dark.

**Example `maps/b1.toml` (8×8):**

```toml
id    = "b1"
name  = "Thornwall Cellars B1"
grid  = "edge"
tiles = "stone"
kind  = "dungeon"                       # dungeon|outdoor
enc   = { rate = 6, table = "b1_rand" }  # % per step
map = """
+-+-+-+-+-+-+-+-+
|S . .|. . . . T|
+ +-+ +d+-+-+-+ +
|.|#|.|. . . .|.|
+ +-+ + +-+-+ + +
|. . . .|A . .|.|
+-+d+-+-+ +-+v+ +
|.|. . .|. . . .|
+ + +s+-+ +-+-+ +
|.|.|C|.|. . F .|
+ + +-+ + +-+-+-+
|. .|. . .|# # #|
+-+ +-+-+ +-+-+-+
|. . . .|. . . .|
+ +-+-+ + +-+ + +
|D . . . . .|. .|
+-+-+-+-+-+-+-+-+
"""

[legend]                   # glyph -> marker
S = { ev = "b1_up",     look = "stairs_up" }
D = { ev = "b1_down",   look = "stairs_dn" }
T = { ev = "b1_chest1", look = "chest" }
C = { ev = "b1_chest2", look = "chest" }
A = { ev = "b1_guard",  look = "npc_guard" }
F = { ev = "b1_gather", look = "gather" }

[[edge]]
at = [3, 0]; side = "s"; key = "bronze_key"

[[zone]]
k = "dark"; rect = [4, 5, 4, 3]       # x,y,w,h

[[foe]]
id = "wolf1"; grp = "wolves"; look = "foe_wolf"
ai = "patrol"; path = [[5,6], [7,6], [7,7], [5,7]]
```

`T` is (7,0); `C` is (2,4) behind a secret door; `D` is (0,7). Stair links are paired warp events; the editor's link tool writes both sides.

## Events, markers and dialogue

An event is a list of cases: on trigger, the first case whose `if` list is all true runs its `do` list. Actions are `"verb args"` strings. Blocking verbs (`say`, `ask`, `talk`, `battle`, `shop`) yield to the UI. Branching jumps to named events (`yes=`, `win=`), so there is no nested control flow. Verb names follow the shared `tome` table (see the convergence doc).

**Triggers (`on`):** `on` = entering the cell (default; `step` is an alias), `face` = standing in front, `use` = act button, `leave` = exiting, `load` = map entered.

| Condition | Meaning |
| --- | --- |
| `flag` / `!flag` | flag set / not set |
| `has:item` | party holds item |
| `gold>=50` | gold comparison |
| `v.name>=3` | var comparison |
| `dir:n` | facing n/e/s/w |
| `chance:25` | random % |
| `once` | auto-flags the event after first run |
| `day>=2` | days elapsed |
| `rules:<query>` | delegated to the rules pack, e.g. `rules:class=cleric`, `rules:check=search:15` |

| Action | Effect |
| --- | --- |
| `say <text\|@dlg>` | show text |
| `talk <dlg>` | run a dialogue |
| `pic <portrait>` | show portrait |
| `ask "<q>" yes=<ev> no=<ev>` | yes/no prompt |
| `set f` / `clr f` / `add v n` | flags and vars |
| `give item [n] [to=U]` / `take item [n]` / `gold n` | inventory, gold |
| `warp map x,y dir` / `stairs map x,y dir` | teleport (stairs adds fade + sound) |
| `face dir` / `spin` | set or randomize facing |
| `dark on\|off` / `reveal` | lighting, automap reveal |
| `battle grp win= lose= flee=` | start combat |
| `shop id` / `inn id` | town facilities |
| `trap id` | resolved by rules pack |
| `chest loot [trap=id]` | loot table |
| `gather tbl [max=3]` | gathering, refreshes daily |
| `foe kill\|spawn id` | FOE control |
| `edge x,y side open` | change a wall at runtime |
| `sfx id` / `wait ms` / `goto ev` / `end` | misc (`end` ends the event) |

```toml
# ev/b1.toml
[ev.b1_up]
case = [{ do = ["ask 'Climb to Thornwall?' yes=b1_up_go"] }]
[ev.b1_up_go]
case = [{ do = ["stairs town 0,0 s"] }]

[ev.b1_down]
case = [
  { if = ["!seal_broken"], do = ["say The stair is sealed with bronze."] },
  { do = ["stairs b2 0,7 n"] },
]

[ev.b1_chest1]
on = "use"
case = [{ if = ["once"], do = ["chest b1_t1 trap=needle", "give bronze_key"] }]

[ev.b1_guard]
on = "face"
case = [
  { if = ["seal_broken"], do = ["say The guard nods you past."] },
  { do = ["talk guard"] },
]

[ev.b1_gather]
on = "use"
case = [{ do = ["gather herbs_b1 max=3"] }]
```

**Dialogue** (`dlg/guard.toml`):

```toml
[dlg.guard]
who   = "Sgt. Varn"
pic   = "guard"              # art/pic/guard.svg
start = "hi"

[dlg.guard.n.hi]
txt = "Halt. The lower stair stays sealed."
opt = [
  { t = "Why?",              to = "why" },
  { t = "I carry the seal.", to = "seal",  if = ["has:bronze_seal"] },
  { t = "Bribe (50g)",       to = "bribe", if = ["gold>=50", "!bribed"] },
  { t = "Leave.",            to = "end" },
]

[dlg.guard.n.why]
pic = "guard.grim"           # falls back to guard.svg
txt = "Wolves. Worse below."
opt = [{ t = "Back.", to = "hi" }]

[dlg.guard.n.seal]
txt = "...Fine. Go."
do  = ["take bronze_seal", "set seal_broken"]
opt = [{ t = "Thanks.", to = "end" }]

[dlg.guard.n.bribe]
txt = "Didn't see you."
do  = ["gold -50", "set bribed", "set seal_broken"]
opt = [{ t = "...", to = "end" }]
```

**Portraits:** IDs map to `art/pic/<id>.svg`; `id.mood` falls back to `id`. Party portraits share the pool. Recommended `viewBox` is `0 0 256 320`.

## Rules module

The primary pack is `srd35`: 3.5e SRD mechanics (BAB, iterative attacks, Fort/Ref/Will saves, Vancian slots) under OGL 1.0a. `lite` is an original ISC d20 used for tests and as a licensing-free default. Packs are compiled-in C (300–800 lines each) plus TOML data; shared helpers live in `rules/common.c`.

| System | License | Feasible? | Handling |
| --- | --- | --- | --- |
| **3.5e SRD** (`srd35`, v1) | OGL 1.0a | **Yes, primary** | Ship full OGL text, declare Open Game Content, Section 15 list, no Product Identity or d20 logo |
| SRD 5.1 / 5.2.1 (`srd51`, v2) | CC-BY-4.0 | Yes | One attribution statement; don't mix 5.1 and 5.2 in one pack |
| ORC systems | ORC | Yes | Reciprocal; exclude Reserved Material; own pack |
| OGL retro-clones (OSRIC, Basic Fantasy) | OGL 1.0a / CC-BY-SA | Yes | Same OGL handling |
| **GURPS** | Proprietary (GURPS Lite is free-use, not open) | **No** | Ship nothing. The API could host a private 3d6 pack you write yourself |
| `lite` | ISC | Yes | Default for tests |

**Boundary: `src/rules/rules.h`** (merges into the shared `Rules` head + `RulesC` subtable at gate G2):

```c
// rules.h - swappable rules pack
typedef struct Db Db; typedef struct Actor Actor; typedef struct Log Log;
typedef struct { u16 id; u8 tgt; u8 n; } Act;  // ability, target mode, count
typedef struct { Actor **a; int n; } Side;

typedef struct Rules {
  const char *id, *lic;                         // "srd35", "OGL-1.0a"
  int  (*load)(Db *, const char *dir);
  int  (*mk)(Actor *, Db *, const char *tmpl, int lvl); // "cls.fighter+race.dwarf" | "mon.goblin"
  int  (*init)(Actor *);                        // initiative
  int  (*acts)(Actor *, Act *out, int max);     // menu options
  int  (*act)(Actor *src, Act, Side tgt, Log *);// resolve + log
  void (*ai)(Actor *, Side ally, Side foe, Act *out);
  int  (*q)(Actor *, const char *query);        // rules: conditions
  void (*rest)(Side, int kind);                 // 0 short, 1 inn
  int  (*xp)(Side pc, Side dead);
  int  (*lvl)(Actor *);
  void (*trap)(Side, const char *id, Log *);
} Rules;
const Rules *rules_get(const char *id);
```

The engine `Actor` holds only `name`, `pic`, `hp`, `hpmax`, `row`, `alive` and a `Kv stat[]` bag the pack fills (str, bab, ac, fort, slots...). The engine never interprets stat keys; stat sheets come from the pack's `sheet.toml`.

**Data (`data/rules/srd35/`):**

```toml
# monsters.toml - Open Game Content (OGL 1.0a), from 3.5e SRD
[mon.goblin]
name = "Goblin"; type = "humanoid"; size = "S"; cr = "1/3"
hd = "1d8+1"; ac = 15; bab = 1; init = 1; spd = 30
abil = { str = 11, dex = 13, con = 12, int = 10, wis = 9, cha = 6 }
save = { fort = 3, ref = 1, will = -1 }
atk  = [
  { id = "morningstar", hit = 2, dmg = "1d6",  t = "bludg+pierce" },
  { id = "javelin",     hit = 3, dmg = "1d4",  t = "pierce", rng = 30 },
]
pic = "mon/goblin"

# classes.toml
[cls.fighter]
name = "Fighter"; hd = 10; bab = "good"; save = { fort = "good", ref = "poor", will = "poor" }
skill = 2; prof = { armor = ["all", "shield"], weap = ["simple", "martial"] }
feat  = { 1 = ["bonus_feat"], 2 = ["bonus_feat"], 4 = ["bonus_feat"] }

# spells.toml (Vancian: prepared per slot level)
[spl.magic_missile]
name = "Magic Missile"; lvl = { wiz = 1 }; school = "evocation"
fx = [{ k = "dmg", n = "min(5, 1+(cl-1)/2)", dmg = "1d4+1", t = "force", auto = true }]

[spl.cure_light_wounds]
name = "Cure Light Wounds"; lvl = { clr = 1 }; school = "conjuration"
fx = [{ k = "heal", amt = "1d8+min(5,cl)" }]
```

`fx` kinds are a closed set (`dmg`, `heal`, `save`, `cond`, `buff`, `summon`) the pack's `act` interprets. Anything stranger becomes a named `trait` in pack C, so \~95% of content stays declarative.

**Attribution handling**

- Every pack has `pack.toml`: `id`, `lic` (SPDX), `notice`, `credit`.
- `srd35` ships `OGL.txt` (full OGL 1.0a), `OGC.md` declaring all pack data Open Game Content, and `SEC15.md` with the Section 15 entries copied verbatim from the SRD's own legal file. No Product Identity terms, no "d20 System" logo, no claim of WotC endorsement.
- `crawl check` fails if a used pack, tileset, font or art directory lacks `lic`, or an OGL pack lacks any of the three files. `crawl pack` writes `CREDITS.txt` (plus the OGL text) and the runtime shows it under Title > Credits.
- Engine code never embeds SRD text; rules text lives only in `data/`. A campaign declares one pack; mixing OGL and CC-BY packs makes `check` warn.

## Rendering

Hand-authored slices, not a raycaster or perspective quads: you get the authentic M&M look and full art control, and SVG doesn't map well onto `SDL_RenderGeometry`'s affine-per-triangle quads (seams, warping). The renderer stays around 200 lines. Cost: a tileset is a fixed set of files, and movement uses a cross-fade or short zoom instead of smooth motion.

**Viewport:** logical 640×480 (4:3), letterboxed. Visible cone is 4 depths (0 = your cell, 3 = farthest), widening laterally.

| Slice | Files per tileset | Count |
| --- | --- | --- |
| Background | `bg.svg` (floor + ceiling) | 1 |
| Front wall, depth d 1–3, offset o ∈ L2..R2 | `f{d}{o}.svg` | \~11 |
| Side wall, depth 0–3, side, offset | `s{d}{L\|R}{o}.svg` | \~12 |
| Overlays | `door_*.svg`, `lock_*.svg` in wall slots | \~23 |
| Sprites | `spr/<look>.svg`, scaled per depth (1, .62, .38, .24) | n |

**Algorithm:** for d = 3→0, offsets outer→inner: side wall → `s` slice; front wall → `f` slice + door overlay; then that depth's sprites (marker `look`s, FOEs). Secret and false walls draw as walls; one-way edges draw as doors on the passable side, walls on the other; dark zones clip d > 0.

`crawl slices <dir> --palette stone.toml` generates flat-shaded starter slices so a new tileset is playable in minutes and refined in Inkscape.

**SVG pipeline**

1. Parse each SVG once (`nsvgParse`), keep the `NSVGimage*`.
2. On first draw or resize: `scale = win_h / 480`, `nsvgRasterize` → RGBA → `SDL_CreateTexture`, cached by `(path, round(scale*64))`.
3. Resize clears the cache; refill lazily.
4. Crop each slice to its bounding box (store an offset). Budget: under 80 MB at 1080p.
5. Text: stb\_truetype glyph atlases, re-baked on resize.

**Automap**

- Auto layer: visited cells and edges seen in the view cone; bitsets per map in the save.
- Player layer (EO-style): per-cell floor color, icon stamps (chest, door, stairs, note, danger), short notes.
- Campaign setting `amap = "auto" | "draw" | "both"`. In `draw` only player-drawn walls show; the auto layer just tints visited cells.
- FOEs show as icons with facing arrows within `foe_vis` cells (default: whole floor).
- Minimap in the corner, full screen on `M`; icons from `art/amap/*.svg`.

## Editor UX

`crawl edit <campaign>`: project tree (left), map canvas (center), inspector (right), 3D preview (top right), problems panel (bottom). The shell (canvas, inspector, problems, hot reload, undo, canonical writer) is the part shared with `tac` via `tome`.

| Tool | Key | Action |
| --- | --- | --- |
| Wall | W | click near an edge to toggle wall/open; drag for a run |
| Edge type | 1–8 | door, locked, secret, false, one-way (arrow follows drag) |
| Cell | C | paint floor/rock; Shift-drag for rect |
| Marker | K | stamp a legend glyph; inspector edits `ev` and `look` |
| Zone | Z | drag rect, pick kind |
| FOE | F | place, click path points, Enter ends |
| Link | L | click cell on map A then map B: paired `stairs` events + legend entries |
| Probe | P | set preview position/facing |
| Playtest | F5 | play from probe with debug overlay (flags, vars, noclip, god) |

- **Edge hit-test:** a click within 25% of a cell border targets the edge, else the cell.
- **Inspector:** edge props; marker event cases as raw TOML with live parse errors; "Open in $EDITOR".
- **Hot reload:** polls `ev/`, `dlg/`, `art/` mtimes every 500 ms.
- **Problems panel:** `crawl check` output on save: dangling IDs, unreachable markers (flood fill respecting one-ways), missing art, SVG subset violations, missing licenses.
- **Save:** canonical ASCII with stable ordering, so hand and editor edits diff cleanly.
- **Undo:** snapshot ring of the map struct (maps are tiny).
- Town editor is a form view for `town.toml`. Dialogue gets a read-only node view in v1.

## Campaign package and game generator

The "game generator" is `crawl pack`: it validates a campaign, tree-shakes its assets, and emits runnable builds for any OS from any OS using prebuilt `crawl-rt` binaries.

```toml
# campaign.toml
id = "thornwall"; title = "Under Thornwall"; ver = "0.1.0"
rules = "srd35"; amap = "both"; party = 6
start = { map = "town", at = [0, 0], dir = "s" }
maps  = ["maps/town.toml", "maps/b1.toml", "maps/b2.toml"]
inc   = ["ev/*.toml", "dlg/*.toml", "items/*.toml", "town.toml"]
lic   = "CC-BY-4.0"; credit = "Log, 2026"
```

**`.pak`** = a zip (miniz) holding `manifest.toml` (id, version, engine ABI, rules id, file list + SHA-256), the normalized campaign tree, only referenced art, the rules pack data, and `CREDITS.txt` plus every `LICENSE`/`OGL.txt`/`NOTICE`. Plain zip, so `unzip` can inspect it.

```
crawl pack examples/demo --target linux-x64 \
      --rt dist/crawl-rt-linux-x64 -o out/thornwall
```

1. `check`; abort on errors.
2. Walk the reference graph from `start` and the maps; collect used assets, warn on unused.
3. Gather licenses into `CREDITS.txt`; fail on any missing `lic`.
4. Write `game.pak`.
5. Emit per target: linux/openbsd `thornwall` + `game.pak`; win `thornwall.exe` + `game.pak`; mac `Thornwall.app/Contents/{MacOS/thornwall, Resources/game.pak, Info.plist}`; `--single` appends the pak to the runtime with trailer `"CRWLPAK1" + u64 offset`.
6. Zip as `thornwall-<ver>-<target>.zip`.

The runtime looks for an appended pak, then `game.pak` beside the exe, then `--pak <path>`. Runtimes come from CI release artifacts; the default flow needs no network.

## CI workflow

There is no hosted OpenBSD runner, so OpenBSD builds in a QEMU VM on Ubuntu via `cross-platform-actions/action` (MIT), which syncs the workspace back so artifacts upload normally. Fallback: `vmactions/openbsd-vm`.

```yaml
name: ci
on:
  push: { branches: [main], tags: ["v*"] }
  pull_request:
defaults: { run: { shell: bash } }

jobs:
  build:
    strategy:
      fail-fast: false
      matrix:
        include:
          - { os: ubuntu-24.04, id: linux-x64 }
          - { os: windows-2022, id: win-x64 }
          - { os: macos-14,     id: mac-universal,
              xflags: "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" }
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
        with: { submodules: recursive }
      - if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y libx11-dev libxext-dev libxrandr-dev \
            libxcursor-dev libxi-dev libxss-dev libwayland-dev \
            libxkbcommon-dev libegl1-mesa-dev libasound2-dev libpulse-dev
      - run: cmake -B b -DCMAKE_BUILD_TYPE=Release ${{ matrix.xflags }}
      - run: cmake --build b --config Release -j 4
      - run: ctest --test-dir b -C Release --output-on-failure
      - run: SDL_VIDEODRIVER=dummy b/bin/crawl check examples/demo
      - uses: actions/upload-artifact@v4
        with: { name: crawl-${{ matrix.id }}, path: b/bin/ }

  openbsd:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with: { submodules: recursive }
      - uses: cross-platform-actions/action@v1.6.0
        with:
          operating_system: openbsd
          version: '7.9'
          architecture: x86-64
          shell: sh
          run: |
            sudo pkg_add -I cmake sdl3 || true      # else build vendored SDL3
            cmake -B b -DCMAKE_BUILD_TYPE=Release
            cmake --build b -j 2
            ctest --test-dir b --output-on-failure
            SDL_VIDEODRIVER=dummy b/bin/crawl check examples/demo
      - uses: actions/upload-artifact@v4
        with: { name: crawl-openbsd-x64, path: b/bin/ }
  # fallback: vmactions/openbsd-vm@v1 with release: '7.9' and the same script

  release:
    if: startsWith(github.ref, 'refs/tags/v')
    needs: [build, openbsd]
    runs-on: ubuntu-latest
    permissions: { contents: write }
    steps:
      - uses: actions/download-artifact@v4
        with: { path: dist }
      - run: for d in dist/*; do (cd "$d" && zip -r "../$(basename "$d").zip" .); done
      - env: { GH_TOKEN: "${{ github.token }}" }
        run: gh release create "$GITHUB_REF_NAME" dist/*.zip --repo "$GITHUB_REPOSITORY" --generate-notes
```

CMake notes: output to `b/bin` for multi-config generators too; `CRAWL_SYSTEM_SDL=ON` uses `find_package(SDL3)`, otherwise the static submodule; a Linux debug job runs tests with `-fsanitize=address,undefined`.

## Milestones

Six stages, each closed by a testable exit gate; `tome` extraction gates (G0–G4) from the convergence doc ride along.

| M | Scope | Exit criteria |
| --- | --- | --- |
| **M0** Skeleton | repo, CMake, vendored deps, TOML loader, `check` stub, CI (G0) | All 4 OS jobs green with artifacts; ASan job green |
| **M1** Maze | ASCII parse + canonical writer, movement/collision for all edge types, flat-color slices, auto automap | Walk the 8×8 example; one-ways and secret doors behave; parse→write round-trip byte-identical |
| **M2** Art + events | SVG cache, `crawl slices`, sprites, event VM, dialogue + portraits, flags/vars, save/load (G1) | B1 demo plays end to end: guard branches, chest, stairs, spinner, dark zone; resize stays crisp |
| **M3** Rules + combat | `rules.h`, `lite`, combat screen, party of 6 with rows, FOEs (patrol, chase), encounters, XP/levels; `srd35` subset: fighter/cleric/rogue/wizard L1–5, 20 monsters, 15 spells (G2) | `rules = "lite"` and `"srd35"` both play the demo; `check` enforces OGL files; fixed-seed combat log golden passes |
| **M4** Editor | canvas tools, inspector, link tool, 3D preview, playtest, problems panel, hot reload, undo (G4) | B2 authored entirely in the editor; files diff cleanly; F5 playtest works |
| **M5** Ship | town hub, player-drawn automap, packager (all targets, `--single`, `.app`), credits screen, fuzzing, 3-floor demo | Tagged release has `crawl` + `crawl-rt` for 4 OSes; a Linux-made Windows build runs; credits include the OGL text |

## Risks and open questions

1. **SVG fidelity vs. dependency size.** nanosvg has no text, filters or masks. Is the Plain SVG subset fine for your art style, or should PlutoSVG be the default from M2?
2. **microui for the editor.** Few widgets (no multiline text, no tree). Nuklear is the fallback; decide by M4.
3. **Packs as compiled C vs. embedded script.** C is small and fast, but third-party systems need a rebuild. Lua 5.4 (MIT) would make packs pure data + script at the cost of one dependency.
4. **OGL 1.0a revocability.** WotC said in 2023 it won't de-authorize 1.0a, but unlike CC-BY the license itself isn't stated irrevocable. Mitigation: `srd51` (CC-BY-4.0) in v2 as a fallback pack with the same data shape.
5. **Distribution.** Single-file executables or folder + pak? Unsigned macOS `.app` bundles hit Gatekeeper; is right-click → Open fine for v1?
6. **Vancian slots in a crawler.** Rest-to-recover pairs naturally with EO-style returns to town, but long dungeons may need camp items. Tune at M3.

**Unverified (check at M0):** OpenBSD 7.9 `pkg_add sdl3` availability; exact `cross-platform-actions/action` latest tag; the 3.5e SRD Section 15 text must be copied from the SRD's own legal file, not retyped.

**Sources**

- [cross-platform-actions/action](https://github.com/cross-platform-actions/action)
- [vmactions/openbsd-vm](https://packages.ecosyste.ms/registries/github%20actions/packages/vmactions/openbsd-vm)
- [System Reference Document (Wikipedia)](https://en.wikipedia.org/wiki/System_Reference_Document)
- [Open Game License text (AoN legacy)](https://legacy.aonprd.com/openGameLicense.html)
- [SRD 5.2 under CC-BY-4.0 (Tribality)](https://www.tribality.com/2025/04/23/dd-system-reference-document-v5-2/)
