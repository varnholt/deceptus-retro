# Authoritative content pipeline

The neighboring `deceptus_engine` owns game data. This repository maintains C
runtime code, conversion tools and selection/binding recipes. Generated artwork,
fonts, dialogue strings, tile data and imported sprites are ignored outputs.
Updating the source and rebuilding is the normal content workflow.

## Conversion stages

1. `content.py` catalogs original Tiled placements, properties, paths, source ids,
   hashes, reachable Lua modules and referenced host API calls.
2. `tiled.py` and `assets.py` render selected source layers losslessly, rasterize
   collision vectors and import native player animation frames. Nested offsets,
   flip flags and supported compressed map formats are handled at this stage.
3. `world_export.py`, `enemies.py` and mechanism/logic importers emit compact C
   definitions for supported behaviors. Unsupported content remains in reports
   or causes explicit export failures where required.
4. `gba_assets.py` fits target palette banks and packs tiles/objects. The SNES
   converter reads its own lossless import. No reduced SNES image feeds the GBA.
5. `campaign_assets.py` assembles room tables; `tutorial_assets.py` selects the
   opening's thirteen source rooms and binds supported interactions and portals.
6. Platform build scripts compile the shared C runtime, backend and generated
   resources into the cartridge image.

Room selection and narrative bindings are still curated code; importing Tiled
objects is not automatic support for the whole original level script. The
historical `tonight` scene uses a separate curated four-area route and goals.

## Artwork, layers and fonts

Preserve source sizes at 1:1. Background palette fitting is target-specific and
uses no dithering by default. Sprites retain opaque source RGB555 colours within
the supported palette reservations. Fidelity measurements are written to each
room's `budget.json`; background conversion does not guarantee every original
RGB value survives the console palette limit.

The tutorial separates foreground layers using source z values relative to the
player. Both `level_solid_onesided` and `level_soild_onesided_dec` are visual
layers as well as supplying one-way floor data; omitting their artwork leaves
only fragments of wooden platforms. Opaque water-body artwork is baked behind
the player. Ambient foam is excluded; the source player splash is an entry-only
sprite effect, independently of the baked waves.
Full original water compositing/stencil behavior remains unfinished.

`psd_layers.py` reads original PSD layer channels with their declared lengths.
`tutorial_ui.py` builds dialogue windows, page prompts, menus and interaction
cues from those layers and original sprite definitions.

`pixel_font.py` reads ASCII glyphs from the original `fonts/deceptum.png` atlas.
The native size is 13px, matching the source engine configuration. The original
13px TTF supplies advances; it is not rerasterized at 12px/24px to create the
visible glyphs. Larger text uses integer nearest-neighbour duplication. Current
bitmap coverage is ASCII; unsupported characters fail explicitly. Dialogue
wraps using the same advances as rendering, with at most two lines per page.

## Water and conditional scenery

`water.py` runs the source spring/neighbor equations offline using the original
surface dimensions, properties, emitters and gradient. Current clips have twelve
frames at six simulation ticks per frame. Random emitter timings become a
seeded periodic schedule so the clip loops deterministically.

`scenery.py` builds conditional source image/extra-animation patches: key,
locker contents, lever handle and the monk's current idle pose. Original frame
durations are compiled into bounded playback tables. Flags select patches;
picking up an item does not require maintaining a second hand-edited image.

Basic swimming now uses the original atmosphere tiles and full source player
frames. Falling water entry triggers the original splash once. Underwater tint
is a backend palette adjustment; it does not modify source art. Bubbles, complete
water effects and original cutscene actor choreography remain unfinished.

## Reports and limits

Useful outputs include `build/content/summary.json`,
`assets/generated/gba/budget.json`, per-area `budget.json`/`animations.json`,
`assets/generated/gba/tutorial.json`, and `build/gba/build.json`. The full-game
strict check deliberately fails while unsupported systems remain. A successful
normal build verifies a selected supported subset, not complete source coverage.

Never commit `assets/source/`, `assets/generated/`, downloaded tools or binary
build outputs. Preserve the original content ownership/dependency notices when
sharing a generated ROM.

## Original animated tiles

`tileanimation.py` reads Tiled animation frame ids and durations directly from
the original tilesets. The blue shrine column uses its original animation.
Background and foreground clips have separate pattern residency and alpha rules.
The ambient water-foam layer is deliberately excluded; entry splashes use the
original player animation instead.

Some source animations run faster than 60Hz. Frames with no display tick are
sampled out using cumulative timing; they never become zero-duration runtime
frames that reset a clip. Original source frame indices/durations remain in the
generated report. Visible pixels are copied from the source frames, never drawn
by a replacement effect. The existing spring-based water-surface bake is kept.

Normal ground blobs are imported from the original placement and `blob_2.lua`
constants. Jump/drop properties are explicitly rejected for this native subset.
The current shared GBA resource library has 494 frames, including full swim
poses, the blob's original idle, movement and death artwork, and entry splashes.
