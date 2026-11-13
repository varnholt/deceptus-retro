/* original counterpart: src/game/mechanisms/onoffblock.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void on_off_block_configure(const deceptus_mechanism_def *definition, deceptus_mechanism_state *state)
{
    state->active = state->enabled ^ ((definition->flags & DECEPTUS_MECH_INVERTED) != 0);
    state->frame = state->active ? 14 : 0;
}

static deceptus_u8 on_off_block_solid(const deceptus_mechanism_set *set, deceptus_u16 index,
                                      const deceptus_rect *bounds)
{
    deceptus_u8 enabled;
    enabled = set->states[index].enabled;
    if (set->definitions[index].flags & DECEPTUS_MECH_INVERTED) {
        enabled = !enabled;
    }
    if (set->timings && (set->timings[index].flags & DECEPTUS_TIMED_ANIMATION)) {
        enabled = set->states[index].active;
    }
    return enabled && overlaps(bounds, &set->definitions[index].bounds);
}
