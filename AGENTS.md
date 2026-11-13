# Project conventions

- `../deceptus_engine` remains the authoritative source for Deceptus artwork, maps, animations and content terms. Do not commit copies of those assets here. Import and convert them into ignored output directories.
- Preserve the artwork's original size and pixel grid at 1:1. Do not shrink or resample it to widen the view. Improve framing and scrolling instead. Console palette conversion is a separate constraint.
- Keep the lossless content-import stage separate from target palette/tile conversion. A future GBA build must read the original import, never a colour-reduced SNES image. Do not enable dithering without a deliberate visual review.
- Keep shared runtime and gameplay in portable C90. PVSnesLib's 65816 compiler has 16-bit `int` and `long`, and a 32-bit `long long`; use the types in `include/retro/types.h`.
- Use snake_case and lowercase explanatory comments. Keep braces on all branches and loops. Format C with the repository `.clang-format`.
- Keep platform SDK headers and registers inside `src/platform/`. Shared code must build on a desktop compiler without a console SDK.
- Keep reusable collision and entity allocation in `src/engine/`, Deceptus rules in `src/game/`, and build-time conversions in `tools/`.
- Use caller-owned memory, fixed-capacity pools and bounded event/render queues. Do not add heap allocation or floating point to frame-time runtime code.
- Positions are signed 12.4 fixed point in room coordinates. Velocities must stay below one 8px collision cell per tick. Larger worlds must use room/chunk origins, not overflow local coordinates.
- Treat logical frames and palette roles as game assets. Encode SNES/GBA/Mega Drive tile, palette, DMA and sprite details in converters and backends.
- Keep GBA and Mega Drive as future backends until they compile and run. Do not add placeholder implementations that report success.
- Verify changes with native engine/converter tests and, for SNES changes, the actual ROM integration route in `tools/emulator.py`.
- Keep unsupported mechanisms and Lua constructs explicit in content reports and build errors. A passing prototype build does not establish full-game support. Compare compiled callbacks against original Lua execution where possible.
- Preserve ownership and dependency notices. Do not add attribution trailers to commits.
