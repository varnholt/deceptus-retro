# Engine architecture

The engine is a growing portable C90 runtime with a Deceptus game layer and
separate hardware backends. Its interfaces are not a frozen general-purpose
engine ABI. GBA is the first complete-game target; the SNES prototype exercises
shared code under a very different compiler and resource budget.

## Code boundaries

| Area | Responsibility |
| --- | --- |
| `include/retro/` | Shared types, input, world, entities, mechanisms, animation, campaign and adventure contracts |
| `src/engine/world.c` | Grid collision, dynamic blockers, one-way floors and body clearance |
| `src/engine/entities.c`, `enemies.c` | Bounded entity/projectile services and native enemy behavior |
| `src/engine/mechanisms.c`, `logic.c` | Mechanism state machines, queues and bounded compiled actions |
| `src/engine/adventure.c` | Inventory transactions, interaction conditions, dialogue/menu modes, portals and room state |
| `src/game/deceptus.c`, `campaign.c` | Player/game rules and the curated preview campaign |
| `src/platform/gba/` | Registers, input, DMA, tile residency, OAM, menus, UI and cartridge startup |
| `src/platform/snes/` | SNES/PVSnesLib video, input and prototype frontend |
| `src/platform/host/` | Native preview bridge |
| `tools/` | Offline authoritative-content import, conversion, compilation and verification |

Console registers and SDK headers belong only in platform code. There is no
runtime SFML, Lua interpreter, heap allocation or floating-point simulation.
The GBA uses a small freestanding runtime rather than a host C library.

## Simulation and memory

Positions are signed 12.4 fixed point in room-local coordinates. Large authored
world coordinates are converted to a room origin plus local position; rooms
must remain below 2048px per axis. Collision uses an 8px grid and bounded dynamic
rectangles instead of Box2D. Slopes and many complex physical mechanisms still
need explicit treatment.

Callers own all state. Entity, projectile, mechanism, event and draw queues have
fixed capacities. Content generation rejects unsupported features and resource
limits instead of inventing successful behavior. `retro_engine.content_error`
exposes runtime resource failures for debugging.

Crouching uses an 18px body instead of the standing 30px body and checks headroom
before standing. Crawling remains a separate skill. One-way drop-through has a
short bounded collision exemption. These rules are shared C; imported native
player frames supply the presentation.

## Adventure state

`retro_adventure` wraps the campaign/engine and stores inventory counts,
progression flags, active dialogue/page, menu selection, portal cooldown and
per-room mechanism snapshots. Current capacities are twelve rooms and sixteen
inventory slots, including reserved item zero. Progression is in-memory only.

Interactions refer to source identities and bounded rectangles. Required and
forbidden flags select available actions. Item consumption, item grant and
handle installation are validated before committing the transaction. Fitting
a handle consumes the interaction input so it cannot also operate the lever on
the same press.

Horizontal portals use the original authored entrance rectangles and arrival
positions. Room changes save and restore mechanism state and keep inventory and
flags. They do not preserve every enemy, projectile or arbitrary Lua global.
Dialogue and menu modes stop the normal adventure simulation. The title's
background has a separate presentation clock so it can keep rotating.

## Rendering

The engine emits logical frame/draw information. Platform code translates it
into hardware objects and palette/tile references. Source sprite sizes remain
unchanged; a smaller display reveals less of the room.

The GBA tutorial uses separate background and foreground planes around the
player, plus a UI plane. A reference-counted tile cache adds/removes only the
incoming/outgoing rows and columns during ordinary scrolling. Dirty animated
rectangles update separately. Initial loads or large camera jumps rebuild the
visible window. See `GBA.md` for address ranges and target constraints.

UI is baked offline from the original PSD layers and bitmap font. Small text
uses the authored 13px bitmap; headings use integer nearest-neighbour copies.
The title uses an affine hardware background and sine lookup table, not a
software 3D renderer. Water spring motion is also baked offline and played as
bounded tile clips.

## Adding another backend

Keep gameplay rules and source identities in shared code, then implement the
new platform's input, graphics/resource layout, audio and save adapters. Reuse
the lossless source import, not another console's reduced palettes or tiles.
Mega Drive currently has no backend; adding one is future work, not a build
option that already succeeds.
