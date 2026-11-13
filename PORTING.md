# Full-game port

The objective is the playable game described by the current `deceptus_engine`
checkout, with its original pixel grid, upstream artwork and gameplay data.
GBA is the first complete-game target, with SNES retained as a secondary backend.
GBA now has a thirteen-room opening under development and a separate four-area
curated playable preview. SNES retains its one-room prototype. The complete
game is not ported. See [current status](docs/STATUS.md) for verified scope.

## Source inventory

`python tools/content.py` follows `data/config/levels.json`, reads the original
Tiled objects and parses the reachable Lua modules. The current checkout has:

- Three configured levels: catacombs, intro and graveyard.
- 145 named room rectangles, 66 enemy placements and 16 placed enemy script types.
- 979 Tiled objects in total, including visual effects and room definitions.
- 29 reachable Lua modules referencing 88 registered engine API functions.

The generated catalog retains source object ids, geometry, paths, typed
properties, script dependencies, source hashes and referenced host calls.
It is written to `build/content/catalog.json`; the compact report is
`build/content/summary.json`. These are generated outputs, not maintained
copies of the game. Unused script files are not mistaken for placed enemies.

## How the code is divided

| Original system | Port implementation | Current state |
| --- | --- | --- |
| Tiled placements/properties | Build-time object catalog and compact C definitions | Full catalog; supported objects in the prototype crop are imported |
| C++ mechanisms | Reusable C components with source ids and event queues | Checkpoints, sensors, levers, blocks, damage, timed spikes and collapsing platforms; four linked GBA preview areas playable |
| Lua mechanism callbacks | Parse source, resolve selectors, compile bounded action programs | Two original callbacks verified; complete scripts unsupported |
| Lua enemies | Native C state machines plus shared movement/animation/weapon services | Rat, arrow trap and normal ground blob implemented; blob variants and 13 placed script types remain |
| Box2D | Fixed-point character/world collision and specific analytical mechanisms | Grid collision, dynamic rectangles and preview wall jumping; slopes/joints and full water fidelity incomplete |
| Level/cutscene logic | Persistent flags, dialogue, inventory, authored room arrivals and selected interactions | Opening subset implemented; full progression and cutscenes incomplete |
| Graphics | Lossless import followed by target-specific palette/tile packing | GBA and SNES prototypes; original artwork remains 1:1 |
| Platform services | Audio, controller mapping, save storage and ROM resource loading | GBA/SNES controls and video; host preview; audio and persistent saves incomplete |

The reusable C runtime does not include SFML, Lua or a console SDK. Backend
registers stay in platform code. The GBA build uses the original content import
and its own graphics encoder, rather than reuse the SNES-reduced images.

## Native mechanism batch

Reusable behavior is implemented in C, while Tiled remains authoritative for
placements, names, links, initial states and timing properties. The current
source placement counts for these families are:

| Family | Placements | Ported behavior |
| --- | ---: | --- |
| Collapsing platforms | 44 | Foot contact, continued countdown once touched, collapse, blocked respawn, restore delay and dynamic collision |
| On/off blocks | 30 | Linked/interval control, inverted state, bounded transition queue and animation/collision state |
| Spike blocks | 12 | Linked/interval control, original animation and damage on frames 29–35 |
| Sensors | 12 | Rectangle enter/leave events |
| Blocking rectangles | 6 | Enabled/inverted dynamic collision |
| Checkpoints | 6 | Source identity and respawn activation |
| Levers | 6 | Two-state links, missing-handle restriction, travel/idle frames and interaction cooldown |
| Damage rectangles | 3 | Configured contact damage |

Counts describe source placements in supported families, not completed coverage
of every room. The importer rejects unsupported modes, geometry and timer or
resource budgets. Moving platforms, ropes, crushers, doors, narrative triggers
and the remaining systems still require their own implementations. The GBA
collapsing-platform route verifies an original placement through standing,
collapse, falling, return and restart. Native tests cover interval/inverted
blocks, queue saturation, deadly animation boundaries and touched-only rules.

Visual differences remain: deterministic collapse shake/base destruction speeds,
and no gradual alpha fade during return. Full level Lua dispatch remains
incomplete. The tutorial adventure layer now preserves mechanism state,
inventory and flags across its selected room graph, but not arbitrary Lua globals.

## Lua stays upstream

The desktop Lua runtime is not copied onto the SNES. The first offline compiler
uses a real Lua syntax tree and supports a deliberately narrow subset:

- Boolean arguments/literals and `and`, `or`, `not`.
- `if` / `elseif` / `else`, boolean local aliases and same-file helper calls.
- Compile-time constant arrays with unrolled `ipairs` loops.
- Resolved `setMechanismEnabled`, `setMechanismVisible` and `toggle` calls.

Callbacks have at most eight boolean arguments. Programs are at most 2,048
bytes; the C executor has a 16-value stack and a 512-instruction limit per call.
Mechanism writes commit only when execution reaches a valid END. Invalid code,
missing objects and exhausted budgets leave mechanism state unchanged.

This is not a general Lua implementation. Dynamic state, runtime loops, timers,
numeric calculations, arbitrary tables/metatables, module method calls,
coroutines, physics queries and engine calls outside that list are unsupported.
They must be added with explicit semantics and tests before their scripts can
ship. The compiler rejects unsupported statements; it does not remove them.
Known direct mutations prevent globals being treated as compile-time constants.
The current subset is intended for individually reviewed callbacks, not an
automatic whole-script translation claim.

As an initial proof, the original `setSwordCageEnabled` loop compiles to 21 bytes,
and `releaseSwordRing` plus its helper compiles to 31 bytes. Their selectors
resolve to the original ring object and four cage object ids. Native tests
execute the compiled program for both boolean arguments. If the main checkout's
Lua C sources are available, the test also executes the complete original Lua
file and compares the resulting mechanism states. Both reference cases pass in
this workspace. No second hand-edited Lua version is maintained.

The source callbacks are tested against a C mechanism fixture. They are not yet
wired to the owl puzzle in a streamed game world. Compiling two callbacks does
not make the level script, its events or the puzzle playable.

## Current playable addition

The prototype imports checkpoint object 1058 (`checkpoint_01`) from the original
catacombs map. Entering its rectangle changes the in-memory respawn point to the
clock's position. The placeholder portal and contact damage use that point.
The emulator route checks that the original id was activated and that the player
returns to it. Its clock art is still static and there is no battery-backed save.

Levers, block collision changes, sensors and damage are tested in native
fixtures. Their complete graphics, animation, inventory requirements and level
script event integration are not playable in this crop. Interval blocks are
unsupported. A lever without its handle cannot be operated; handle insertion
is implemented in the tutorial adventure, but not this older standalone crop. Out-of-crop/unresolved lever targets fail export
rather than being dropped. Every remaining crop object is listed with a reason
in `assets/source/mechanisms.json`.

## Work order and completion checks

1. **Whole-room loading and streaming.** Export adjacent room resources from
   upstream geometry, stream visible tiles/actors, implement vertical camera
   movement and preserve state across room changes. Check traversal and return
   trips before increasing the content loaded at once. Large source coordinates
   must become a room origin plus local 12.4 positions, not overflow that type.
2. **Mechanism chains.** Make the original lever/block puzzles, moving and
   collapsing platforms, spikes/traps and doors work, including their graphics,
   collision, timing, links and reset rules. Test original object chains rather
   than only isolated component fixtures.
3. **Real enemies and combat.** Replace the demo patrol with original placements,
   sprites and behavior, starting with rat and simpler patrol types. Expand the
   native movement, animation and projectile services needed by each type. Compare
   C state transitions and actions against original Lua traces; keep unsupported calls visible.
4. **Player abilities and traversal.** Port slopes, water/swimming, wall movement,
   weapons, ropes and the other abilities the level geometry requires. Match
   the original progression gates and tune against recorded input.
5. **Inventory, puzzles and narrative.** Add stable item/treasure/skill ids,
   progression flags, dialogues, cutscene actions and cross-level transitions.
   Verify the owl/rubies/sword puzzle and level entry/exit rules end to end.
6. **Audio and save backends.** Convert audio from upstream sources, consume game
   events on each platform and serialize progression with versioning and integrity
   checks. Test checkpoint reload, corrupted saves and room revisits.
7. **Complete-game runs.** Traverse all configured levels with normal gameplay,
   check every required enemy/mechanism/script path, and profile the worst scenes
   on the actual target. Emulator success alone does not establish hardware
   performance or complete original-physics fidelity.

The default curated GBA preview build loads four areas with visible-tile caching and a
complete title-to-finish preview route. Rat and arrow-trap placements use native
C, with Lua-derived settings and a differential firing-schedule test. This does
not finish the original room graph: preview exits are curated, revisiting rooms
does not preserve arbitrary original progression. The separate tutorial preserves
its selected flags and mechanism states; unsupported enemies and
level scripts remain explicit.

## Commands and truthful build status

```powershell
python tools/content.py
cmake --build build/host --config Debug
ctest --test-dir build/host -C Debug --output-on-failure
python tools/build_snes.py
python tools/emulator.py
python tools/build_gba.py
python tools/gba_emulator.py
python tools/package_preview.py --build
```

The native build generates the source callback fixture from the original files.
`LUA_REFERENCE_SOURCE_DIR` can point to a Lua source checkout; by default it uses
the main engine's fetched sources in `build_release/_deps/lua-src` when present.
Without them, expected-state checks still run, but the Lua differential check is
not performed. No Lua interpreter is included in the console ROM.

`python tools/build_snes.py --full-game` requests complete content support. It
currently fails explicitly and writes the missing-feature report. The ordinary
build remains the playable prototype; it must not be presented as a full-game
ROM because its compiler and emulator tests pass.
