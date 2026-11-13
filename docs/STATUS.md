# Current status

Updated for local Playtest 07, 13 November 2026.

## Available targets

| Target | Current scope |
| --- | --- |
| GBA tutorial | Thirteen imported opening rooms; source UI, inventory/flags, selected interactions, menus and automatic connections |
| GBA curated preview | Four areas with a separately verified title-to-finish route, enemies, mechanisms and replay |
| SNES | Retained one-room clock/checkpoint/patrol/portal prototype |
| Native host | Shared simulation and small preview, plus engine/converter tests |
| Mega Drive | Planned; no implementation |

## Opening content

The tutorial imports The Cell, Lower Cells, Crossroads, Upper Cells, The Cellar,
Guards' Office, Main Hall, Altar of the Blind Owl, Clockwork Antechamber, The
Loop, The Sewers, Fool's Ledge and Mines Antechamber. Source room ids are retained in generated
reports. Original asset ownership and artwork remain in the main engine repo.

Implemented opening rules include source cellmate/bed text, the key and locker,
taking the lever handle, installing it, operating supported linked mechanisms,
room-state persistence, and the original darkness warning. The monk currently
uses an idle pose. Crossroads links toggle immediately; the original camera and
timed cutscene sequence is not reproduced.

The key/handle chain was exercised during earlier tuning. The entire opening
through the named Sewers has **not** been verified end to end with the current
movement settings. Room imports and isolated interactions are not completion
claims. Guards' Office and Fool's Ledge use authored arrival positions rather
than assuming every adjacent room's walkways have identical heights.

## Latest presentation fixes

- Original ground blob patrol, source animation, contact damage and stomp death.
- Basic swimming with source water detection and full directional swim sprites.
- Original shrine tile animation and one-shot falling-entry splash; no ambient looping splash.
- Underwater tint, a small exit hop, original upward wind and delayed wetsuit warning.
- Main Hall/Mines Antechamber passage verified in both directions.
- Mechanism files named after their original counterparts; public C namespace `deceptus_`.
- GitHub Actions build and source-revision-recorded cartridge artifacts.
- Original bitmap font at 13px; title headings use exact 2x copies of those pixels.
- Smooth affine title rotation, changing on 59 of 60 sampled display frames.
- Whole wooden one-way floor artwork, with original foreground occlusion.
- Separate room-title tile storage while world interaction cues are active.
- Pause header lowered two pixels, functional options/controls and confirmation.
- Lower Crossroads/Fool's Ledge connection and western cellar darkness warning.
- Player visible underwater, with water-body artwork behind the baked surface.

## Evidence

The local build passes nine native C test groups and forty-five Python conversion
checks, including an exact comparison of large text against a nearest-neighbour
2x copy of the original glyph rendering. SNES rebuild and Snes9x integration
checks pass for the retained prototype.

GBA checks boot the actual ROM and exercise menu navigation. Controlled-start
fixtures check floor/UI scenes, the darkness warning, underwater visibility,
water-surface rendering, original shrine frame advancement, a blob scene,
a Crossroads/Fool's Ledge return trip and the Main Hall/Mines connection.
A 240-display-frame scrolling water check recorded one simulation tick per
display frame, with zero missed ticks. The player pauses when unfocused and
reuses its display image buffer. This timing result covers the tested scene;
it is not a whole-game performance guarantee. The delayed wetsuit warning
keeps the simulation running while the original upward wind applies. Their reports
and screenshots are generated locally by `tools/verify_tutorial.py`. These are
fixture checks, not a full tutorial input route. No physical hardware testing or
complete-game performance guarantee is claimed.

## Remaining work

1. Finish and repeatedly play the original opening route under final movement
   tuning, including the Loop and descent to the Sewers. Resolve every required
   gate and source callback instead of adding progression shortcuts.
2. Refine swimming/entry/exit fidelity and add remaining traversal abilities;
   refine collision fidelity and profile demanding rooms.
3. Complete cutscenes, dialogue presentation and original menu/inventory behavior.
4. Port additional enemies and mechanisms to reusable C services, retaining
   source settings and differential checks where possible. Rat, arrow trap and
   normal ground blob are implemented; blob jump/drop variants and other enemy
   families remain unsupported.
5. Convert source music/SFX offline and add platform playback. Event ids have
   begun to be exposed, but no sound currently plays in the ROM or project player.
6. Add persistent save storage and versioned progression serialization.
7. Extend complete-level/game verification, hardware testing and release packaging.

The first commercial-quality target remains GBA. The reusable C engine can grow
alongside that work; SNES is a secondary compatibility/performance effort and
Mega Drive remains future work.

The Mines entrance is connected. Its farther locked iron door and actual wetsuit
acquisition remain unsupported. The current water blocker stays enabled.

GitHub Actions successfully built and tested revision e3c67cd and uploaded its
cartridge artifact; subsequent revisions require their own successful run.
