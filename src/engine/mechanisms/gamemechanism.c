/* original counterpart: src/game/mechanisms/gamemechanism.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */
#include "mechanisminternal.h"

static retro_u8 overlaps(const retro_rect *first, const retro_rect *second)
{
    return first->x < second->x + second->width && first->x + first->width > second->x &&
           first->y < second->y + second->height && first->y + first->height > second->y;
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

#include "blockingrect.c"
#include "checkpoint.c"
#include "collapsingplatform.c"
#include "damagerect.c"
#include "lever.c"
#include "onoffblock.c"
#include "sensorrect.c"
#include "spikeblock.c"
#include "switchanimation.c"

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
        set->states[index].handle_available = !(definitions[index].flags & RETRO_MECH_MISSING_HANDLE);
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
            spike_block_configure(state);
        } else if (set->definitions[index].kind == RETRO_MECH_BLOCK) {
            on_off_block_configure(&set->definitions[index], state);
        } else if (set->definitions[index].kind == RETRO_MECH_LEVER) {
            lever_configure(state);
        }
        state->target_frame = state->frame;
    }
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

void retro_mechanisms_tick(retro_mechanism_set *set, const retro_rect *player, retro_u8 interact_pressed)
{
    retro_u16 index;
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
            if (!lever_interact(set, index, inside, interact_pressed)) {
                state->inside = inside;
                continue;
            }
        } else if (state->enabled || (definition->kind == RETRO_MECH_SPIKES && set->timings)) {
            if (definition->kind == RETRO_MECH_CHECKPOINT) {
                checkpoint_contact(set, index, entering);
            } else if (definition->kind == RETRO_MECH_SENSOR) {
                sensor_rect_contact(set, index, inside);
            } else if (inside) {
                if (definition->kind == RETRO_MECH_DAMAGE) {
                    damage_rect_contact(set, index);
                } else if (definition->kind == RETRO_MECH_SPIKES) {
                    spike_block_contact(set, index);
                }
            }
        }
        state->inside = inside;
    }
}
