/* original counterpart: src/game/mechanisms/collapsingplatform.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void collapse_tick(retro_mechanism_set *set, retro_u16 index, const retro_rect *player)
{
    retro_mechanism_state *state;
    const retro_mechanism_def *definition;
    const retro_mechanism_timing *timing;
    retro_u8 standing;
    state = &set->states[index];
    definition = &set->definitions[index];
    timing = &set->timings[index];
    standing = player->y + player->height == definition->bounds.y &&
               player->x < definition->bounds.x + definition->bounds.width &&
               player->x + player->width > definition->bounds.x;
    if (!state->phase) {
        if (standing) {
            state->triggered = 1;
        }
        if (standing || ((timing->flags & RETRO_COLLAPSE_ONCE_TOUCHED) && state->triggered)) {
            if (++state->timer > timing->collapse_ticks) {
                state->phase = 1;
                state->active = 0;
                state->timer = 0;
                state->triggered = 0;
                emit(set, definition->source_id, RETRO_MECH_COLLAPSED, 1);
            }
        } else {
            state->timer = 0;
        }
    } else if (state->phase == 1) {
        if (state->timer < 65535) {
            ++state->timer;
        }
        state->frame = state->timer < 76 ? (retro_u8)(state->timer / 2) : 38;
        if (state->timer > timing->respawn_ticks && !overlaps(player, &definition->bounds)) {
            state->phase = 2;
            state->timer = 0;
            state->frame = 0;
        }
    } else if (++state->timer > timing->fade_ticks) {
        state->phase = 0;
        state->timer = 0;
        state->active = state->enabled;
        emit(set, definition->source_id, RETRO_MECH_RESTORED, 1);
    }
}

static retro_u8 collapsing_platform_solid(const retro_mechanism_set *set, retro_u16 index, const retro_rect *bounds)
{
    return set->states[index].active && set->states[index].enabled && overlaps(bounds, &set->definitions[index].bounds);
}
