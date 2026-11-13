#include "retro/mechanisms.h"
#include <string.h>

static retro_u8 overlaps(const retro_rect *first, const retro_rect *second)
{
    return first->x < second->x + second->width && first->x + first->width > second->x &&
           first->y < second->y + second->height && first->y + first->height > second->y;
}

retro_i16 retro_mechanism_find(const retro_mechanism_set *set, retro_u16 source_id)
{
    retro_u16 index;
    for (index = 0; index < set->count; ++index) {
        if (set->definitions[index].source_id == source_id) {
            return (retro_i16)index;
        }
    }
    return -1;
}

retro_u8 retro_mechanisms_init(retro_mechanism_set *set, const retro_mechanism_def *definitions, retro_u16 count,
                               const retro_u16 *targets, retro_u16 target_count)
{
    retro_u16 index, previous, target;
    memset(set, 0, sizeof(*set));
    if (count > RETRO_MECHANISM_CAPACITY || (count && !definitions) || (target_count && !targets)) {
        return 0;
    }
    for (index = 0; index < count; ++index) {
        if (definitions[index].target_first > target_count ||
            definitions[index].target_count > target_count - definitions[index].target_first) {
            return 0;
        }
        for (previous = 0; previous < index; ++previous) {
            if (definitions[index].source_id == definitions[previous].source_id) {
                return 0;
            }
        }
    }
    for (target = 0; target < target_count; ++target) {
        for (index = 0; index < count; ++index) {
            if (definitions[index].source_id == targets[target]) {
                break;
            }
        }
        if (index == count) {
            return 0;
        }
    }
    set->definitions = definitions;
    set->targets = targets;
    set->target_count = target_count;
    set->count = (retro_u8)count;
    for (index = 0; index < count; ++index) {
        set->states[index].enabled = definitions[index].enabled != 0;
        set->states[index].visible = 1;
        set->states[index].active = set->states[index].enabled;
    }
    return 1;
}

void retro_mechanisms_configure(retro_mechanism_set *set, const retro_mechanism_timing *timings)
{
    retro_u16 index;
    retro_mechanism_state *state;
    set->timings = timings;
    for (index = 0; index < set->count; ++index) {
        state = &set->states[index];
        if (set->definitions[index].kind == RETRO_MECH_SPIKES) {
            state->frame = state->enabled ? 32 : 0;
        } else if (set->definitions[index].kind == RETRO_MECH_BLOCK) {
            state->active = state->enabled ^ ((set->definitions[index].flags & RETRO_MECH_INVERTED) != 0);
            state->frame = state->active ? 14 : 0;
        } else if (set->definitions[index].kind == RETRO_MECH_LEVER) {
            state->progress = state->enabled ? 10 : 0;
            state->frame = state->enabled ? 22 : 0;
        }
        state->target_frame = state->frame;
    }
}

static void emit(retro_mechanism_set *set, retro_u16 source_id, retro_u8 kind, retro_u16 value)
{
    retro_mechanism_event *event;
    if (set->event_count == RETRO_MECHANISM_EVENT_CAPACITY) {
        set->overflow = 1;
        return;
    }
    event = &set->events[set->event_count++];
    event->source_id = source_id;
    event->kind = kind;
    event->value = value;
}

retro_u8 retro_mechanism_enable(retro_mechanism_set *set, retro_u16 source_id, retro_u8 enabled)
{
    retro_i16 index;
    index = retro_mechanism_find(set, source_id);
    if (index < 0) {
        return 0;
    }
    enabled = enabled != 0;
    if (set->states[index].enabled != enabled) {
        if (set->timings && (set->timings[index].flags & RETRO_TIMED_ANIMATION) &&
            (set->definitions[index].kind == RETRO_MECH_BLOCK || set->definitions[index].kind == RETRO_MECH_SPIKES)) {
            if (set->states[index].pending_count >= 8) {
                set->overflow = 1;
                return 0;
            }
            if (enabled) {
                set->states[index].pending |= (retro_u8)(1U << set->states[index].pending_count);
            }
            ++set->states[index].pending_count;
        }
        set->states[index].enabled = enabled;
        emit(set, source_id, RETRO_MECH_ENABLED, enabled);
    }
    return 1;
}

static void animate_switch(retro_mechanism_set *set, retro_u16 index)
{
    retro_mechanism_state *state;
    retro_u8 end, enabled;
    state = &set->states[index];
    end = set->definitions[index].kind == RETRO_MECH_SPIKES ? 39 : 31;
    if (state->frame != state->target_frame) {
        state->fraction += 2;
        if (state->fraction >= 3) {
            state->fraction -= 3;
            ++state->frame;
            if (state->frame >= end) {
                state->frame = 0;
                state->target_frame = 0;
            }
        }
    } else if (state->pending_count) {
        enabled = state->pending & 1;
        state->pending >>= 1;
        --state->pending_count;
        if (set->definitions[index].kind == RETRO_MECH_BLOCK && (set->definitions[index].flags & RETRO_MECH_INVERTED)) {
            enabled = !enabled;
        }
        state->active = enabled;
        state->target_frame = enabled ? (end == 39 ? 32 : 14) : end;
        state->fraction = 0;
    }
}

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

void retro_mechanisms_tick(retro_mechanism_set *set, const retro_rect *player, retro_u8 interact_pressed)
{
    retro_u16 index, target;
    retro_u8 inside, entering;
    retro_mechanism_state *state;
    const retro_mechanism_def *definition;
    set->event_count = 0;
    set->overflow = 0;
    for (index = 0; index < set->count; ++index) {
        definition = &set->definitions[index];
        state = &set->states[index];
        inside = overlaps(player, &definition->bounds);
        entering = inside && !state->inside;
        if (set->timings) {
            if (set->timings[index].flags & RETRO_TIMED_INTERVAL) {
                if (++state->timer >= (state->enabled ? set->timings[index].on_ticks : set->timings[index].off_ticks)) {
                    state->timer = 0;
                    retro_mechanism_enable(set, definition->source_id, !state->enabled);
                }
            }
            if (definition->kind == RETRO_MECH_COLLAPSING && state->enabled) {
                collapse_tick(set, index, player);
            } else if (set->timings[index].flags & RETRO_TIMED_ANIMATION) {
                if (definition->kind == RETRO_MECH_LEVER) {
                    animate_lever(state);
                } else {
                    animate_switch(set, index);
                }
            }
        }
        if (definition->kind == RETRO_MECH_LEVER) {
            if (inside && interact_pressed && !state->cooldown && !(definition->flags & RETRO_MECH_MISSING_HANDLE)) {
                for (target = 0; target < definition->target_count; ++target) {
                    retro_i16 slot;
                    slot = retro_mechanism_find(set, set->targets[definition->target_first + target]);
                    if (set->timings && (set->timings[slot].flags & RETRO_TIMED_ANIMATION) &&
                        (set->definitions[slot].kind == RETRO_MECH_BLOCK ||
                         set->definitions[slot].kind == RETRO_MECH_SPIKES) &&
                        set->states[slot].enabled == state->enabled && set->states[slot].pending_count >= 8) {
                        break;
                    }
                }
                if (target < definition->target_count) {
                    set->overflow = 1;
                    state->inside = inside;
                    continue;
                }
                retro_mechanism_enable(set, definition->source_id, !state->enabled);
                if (set->timings && (set->timings[index].flags & RETRO_TIMED_ANIMATION)) {
                    state->cooldown = 60;
                }
                for (target = 0; target < definition->target_count; ++target) {
                    retro_mechanism_enable(set, set->targets[definition->target_first + target], state->enabled);
                }
            }
        } else if (state->enabled || (definition->kind == RETRO_MECH_SPIKES && set->timings)) {
            if (definition->kind == RETRO_MECH_CHECKPOINT && entering && !state->triggered) {
                state->triggered = 1;
                emit(set, definition->source_id, RETRO_MECH_REACHED, 1);
            } else if (definition->kind == RETRO_MECH_SENSOR && inside != state->inside) {
                emit(set, definition->source_id, inside ? RETRO_MECH_ENTER : RETRO_MECH_LEAVE, inside);
            } else if ((definition->kind == RETRO_MECH_DAMAGE || definition->kind == RETRO_MECH_SPIKES) && inside) {
                if (definition->kind != RETRO_MECH_SPIKES || !set->timings ||
                    (state->frame >= 29 && state->frame <= 35)) {
                    emit(set, definition->source_id, RETRO_MECH_HIT, definition->value);
                }
            }
        }
        state->inside = inside;
    }
}

retro_u8 retro_mechanism_solid(const retro_mechanism_set *set, const retro_rect *bounds)
{
    retro_u16 index;
    retro_u8 enabled;
    const retro_mechanism_def *definition;
    if (!set) {
        return 0;
    }
    for (index = 0; index < set->count; ++index) {
        definition = &set->definitions[index];
        if (definition->kind != RETRO_MECH_BLOCK && definition->kind != RETRO_MECH_COLLAPSING) {
            continue;
        }
        enabled = set->states[index].enabled;
        if (definition->flags & RETRO_MECH_INVERTED) {
            enabled = !enabled;
        }
        if (definition->kind == RETRO_MECH_COLLAPSING ||
            (set->timings && (set->timings[index].flags & RETRO_TIMED_ANIMATION))) {
            enabled = set->states[index].active && set->states[index].enabled;
            if (definition->kind == RETRO_MECH_BLOCK) {
                enabled = set->states[index].active;
            }
        }
        if (enabled && overlaps(bounds, &definition->bounds)) {
            return 1;
        }
    }
    return 0;
}
