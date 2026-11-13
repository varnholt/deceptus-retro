/* original counterpart: src/game/mechanisms/lever.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void animate_lever(deceptus_mechanism_state *state)
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
        state->frame = (deceptus_u8)(22 + state->idle_ticks / 6);
        if (++state->idle_ticks >= 36) {
            state->idle_ticks = 0;
        }
    }
}

static void lever_configure(deceptus_mechanism_state *state)
{
    state->progress = state->enabled ? 10 : 0;
    state->frame = state->enabled ? 22 : 0;
}

static deceptus_u8 lever_interact(deceptus_mechanism_set *set, deceptus_u16 index, deceptus_u8 inside,
                                  deceptus_u8 interact_pressed)
{
    deceptus_u16 target;
    deceptus_i16 slot;
    deceptus_mechanism_state *state;
    const deceptus_mechanism_def *definition;
    state = &set->states[index];
    definition = &set->definitions[index];
    if (inside && interact_pressed && !state->cooldown && state->handle_available) {
        for (target = 0; target < definition->target_count; ++target) {
            slot = deceptus_mechanism_find(set, set->targets[definition->target_first + target]);
            if (set->timings && (set->timings[slot].flags & DECEPTUS_TIMED_ANIMATION) &&
                (set->definitions[slot].kind == DECEPTUS_MECH_BLOCK ||
                 set->definitions[slot].kind == DECEPTUS_MECH_SPIKES) &&
                set->states[slot].enabled == state->enabled && set->states[slot].pending_count >= 8) {
                break;
            }
        }
        if (target < definition->target_count) {
            set->overflow = 1;
            return 0;
        }
        deceptus_mechanism_enable(set, definition->source_id, !state->enabled);
        if (set->timings && (set->timings[index].flags & DECEPTUS_TIMED_ANIMATION)) {
            state->cooldown = 60;
        }
        for (target = 0; target < definition->target_count; ++target) {
            deceptus_mechanism_enable(set, set->targets[definition->target_first + target], state->enabled);
        }
    }
    return 1;
}
