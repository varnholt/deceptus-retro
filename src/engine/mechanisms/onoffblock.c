/* original counterpart: src/game/mechanisms/onoffblock.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void on_off_block_configure(const retro_mechanism_def *definition, retro_mechanism_state *state)
{
    state->active = state->enabled ^ ((definition->flags & RETRO_MECH_INVERTED) != 0);
    state->frame = state->active ? 14 : 0;
}

static retro_u8 on_off_block_solid(const retro_mechanism_set *set, retro_u16 index, const retro_rect *bounds)
{
    retro_u8 enabled;
    enabled = set->states[index].enabled;
    if (set->definitions[index].flags & RETRO_MECH_INVERTED) {
        enabled = !enabled;
    }
    if (set->timings && (set->timings[index].flags & RETRO_TIMED_ANIMATION)) {
        enabled = set->states[index].active;
    }
    return enabled && overlaps(bounds, &set->definitions[index].bounds);
}
