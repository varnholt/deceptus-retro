/* original counterpart: src/game/mechanisms/lever.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void animate_lever(retro_mechanism_state *state)
{
    if (state->cooldown) {
        --state->cooldown;
    }
    if (state->enabled && state->progress < 10) {
        ++state->progress;
        state->frame = state->progress;
        state->idle_ticks = 0;
    } else if (!state->enabled && state->progress) {
        --state->progress;
        state->frame = 11 + 10 - state->progress;
        state->idle_ticks = 0;
    } else if (state->enabled) {
        state->frame = (retro_u8)(22 + state->idle_ticks / 6);
        if (++state->idle_ticks >= 36) {
            state->idle_ticks = 0;
        }
    }
}

static void lever_configure(retro_mechanism_state *state)
{
    state->progress = state->enabled ? 10 : 0;
    state->frame = state->enabled ? 22 : 0;
}

static retro_u8 lever_interact(retro_mechanism_set *set, retro_u16 index, retro_u8 inside, retro_u8 interact_pressed)
{
    retro_u16 target;
    retro_i16 slot;
    retro_mechanism_state *state;
    const retro_mechanism_def *definition;
    state = &set->states[index];
    definition = &set->definitions[index];
    if (inside && interact_pressed && !state->cooldown && state->handle_available) {
        for (target = 0; target < definition->target_count; ++target) {
            slot = retro_mechanism_find(set, set->targets[definition->target_first + target]);
            if (set->timings && (set->timings[slot].flags & RETRO_TIMED_ANIMATION) &&
                (set->definitions[slot].kind == RETRO_MECH_BLOCK || set->definitions[slot].kind == RETRO_MECH_SPIKES) &&
                set->states[slot].enabled == state->enabled && set->states[slot].pending_count >= 8) {
                break;
            }
        }
        if (target < definition->target_count) {
            set->overflow = 1;
            return 0;
        }
        retro_mechanism_enable(set, definition->source_id, !state->enabled);
        if (set->timings && (set->timings[index].flags & RETRO_TIMED_ANIMATION)) {
            state->cooldown = 60;
        }
        for (target = 0; target < definition->target_count; ++target) {
            retro_mechanism_enable(set, set->targets[definition->target_first + target], state->enabled);
        }
    }
    return 1;
}
