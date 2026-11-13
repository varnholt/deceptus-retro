# Current status

Updated for local Playtest 05, 13 November 2026.

## Available targets

| Target | Current scope |
| --- | --- |
| GBA tutorial | Twelve imported opening rooms; source UI, inventory/flags, selected interactions, menus and automatic connections |
| GBA curated preview | Four areas with a separately verified title-to-finish route, enemies, mechanisms and replay |
| SNES | Retained one-room clock/checkpoint/patrol/portal prototype |
| Native host | Shared simulation and small preview, plus engine/converter tests |
| Mega Drive | Planned; no implementation |

## Opening content

The tutorial imports The Cell, Lower Cells, Crossroads, Upper Cells, The Cellar,
Guards' Office, Main Hall, Altar of the Blind Owl, Clockwork Antechamber, The
Loop, The Sewers and Fool's Ledge. Source room ids are retained in generated
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

- Original bitmap font at 13px; title headings use exact 2x copies of those pixels.
- Smooth affine title rotation, changing on 59 of 60 sampled display frames.
- Whole wooden one-way floor artwork, with original foreground occlusion.
- Separate room-title tile storage while world interaction cues are active.
- Pause header lowered two pixels, functional options/controls and confirmation.
- Lower Crossroads/Fool's Ledge connection and western cellar darkness warning.
- Player visible underwater, with water body/foam behind the baked surface.

## Evidence

The local build passes eight native C test groups and forty Python conversion
checks, including an exact comparison of large text against a nearest-neighbour
2x copy of the original glyph rendering. SNES rebuild and Snes9x integration
checks pass for the retained prototype.

GBA checks boot the actual ROM and exercise menu navigation. Controlled-start
fixtures check floor/UI scenes, the darkness warning, underwater visibility,
water-surface rendering and a Crossroads/Fool's Ledge return trip. Their reports
and screenshots are generated locally by `tools/verify_tutorial.py`. These are
fixture checks, not a full tutorial input route. No physical hardware testing or
complete-game performance guarantee is claimed.

## Remaining work

1. Finish and repeatedly play the original opening route under final movement
   tuning, including the Loop and descent to the Sewers. Resolve every required
   gate and source callback instead of adding progression shortcuts.
2. Implement swimming, water entry/exit behavior and missing traversal abilities;
   refine collision fidelity and profile demanding rooms.
3. Complete cutscenes, dialogue presentation and original menu/inventory behavior.
4. Port additional enemies and mechanisms to reusable C services, retaining
   source settings and differential checks where possible. Rat and arrow trap
   are implemented; other placed enemy families remain unsupported.
5. Convert source music/SFX offline and add platform playback. Event ids have
   begun to be exposed, but no sound currently plays in the ROM or project player.
6. Add persistent save storage and versioned progression serialization.
7. Extend complete-level/game verification, hardware testing and release packaging.

The first commercial-quality target remains GBA. The reusable C engine can grow
alongside that work; SNES is a secondary compatibility/performance effort and
Mega Drive remains future work.
