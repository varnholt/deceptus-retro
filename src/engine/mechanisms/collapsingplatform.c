/* original counterpart: src/game/mechanisms/collapsingplatform.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void collapse_tick(deceptus_mechanism_set *set, deceptus_u16 index, const deceptus_rect *player)
{
    deceptus_mechanism_state *state;
    const deceptus_mechanism_def *definition;
    const deceptus_mechanism_timing *timing;
    deceptus_u8 standing;
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
        if (standing || ((timing->flags & DECEPTUS_COLLAPSE_ONCE_TOUCHED) && state->triggered)) {
            if (++state->timer > timing->collapse_ticks) {
                state->phase = 1;
                state->active = 0;
                state->timer = 0;
                state->triggered = 0;
                emit(set, definition->source_id, DECEPTUS_MECH_COLLAPSED, 1);
            }
        } else {
            state->timer = 0;
        }
    } else if (state->phase == 1) {
        if (state->timer < 65535) {
            ++state->timer;
        }
        state->frame = state->timer < 76 ? (deceptus_u8)(state->timer / 2) : 38;
        if (state->timer > timing->respawn_ticks && !overlaps(player, &definition->bounds)) {
            state->phase = 2;
            state->timer = 0;
            state->frame = 0;
        }
    } else if (++state->timer > timing->fade_ticks) {
        state->phase = 0;
        state->timer = 0;
        state->active = state->enabled;
        emit(set, definition->source_id, DECEPTUS_MECH_RESTORED, 1);
    }
}

static deceptus_u8 collapsing_platform_solid(const deceptus_mechanism_set *set, deceptus_u16 index,
                                             const deceptus_rect *bounds)
{
    return set->states[index].active && set->states[index].enabled && overlaps(bounds, &set->definitions[index].bounds);
}
