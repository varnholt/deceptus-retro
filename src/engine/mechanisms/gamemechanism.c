/* original counterpart: src/game/mechanisms/gamemechanism.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */
#include "mechanisminternal.h"

static deceptus_u8 overlaps(const deceptus_rect *first, const deceptus_rect *second)
{
    return first->x < second->x + second->width && first->x + first->width > second->x &&
           first->y < second->y + second->height && first->y + first->height > second->y;
}

static void emit(deceptus_mechanism_set *set, deceptus_u16 source_id, deceptus_u8 kind, deceptus_u16 value)
{
    deceptus_mechanism_event *event;
    if (set->event_count == DECEPTUS_MECHANISM_EVENT_CAPACITY) {
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
#include "wind.c"

deceptus_i16 deceptus_mechanism_find(const deceptus_mechanism_set *set, deceptus_u16 source_id)
{
    deceptus_u16 index;
    for (index = 0; index < set->count; ++index) {
        if (set->definitions[index].source_id == source_id) {
            return (deceptus_i16)index;
        }
    }
    return -1;
}

deceptus_u8 deceptus_mechanisms_init(deceptus_mechanism_set *set, const deceptus_mechanism_def *definitions,
                                     deceptus_u16 count, const deceptus_u16 *targets, deceptus_u16 target_count)
{
    deceptus_u16 index, previous, target;
    memset(set, 0, sizeof(*set));
    if (count > DECEPTUS_MECHANISM_CAPACITY || (count && !definitions) || (target_count && !targets)) {
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
    set->count = (deceptus_u8)count;
    for (index = 0; index < count; ++index) {
        set->states[index].enabled = definitions[index].enabled != 0;
        set->states[index].visible = 1;
        set->states[index].active = set->states[index].enabled;
        set->states[index].handle_available = !(definitions[index].flags & DECEPTUS_MECH_MISSING_HANDLE);
    }
    return 1;
}

void deceptus_mechanisms_configure(deceptus_mechanism_set *set, const deceptus_mechanism_timing *timings)
{
    deceptus_u16 index;
    deceptus_mechanism_state *state;
    set->timings = timings;
    for (index = 0; index < set->count; ++index) {
        state = &set->states[index];
        if (set->definitions[index].kind == DECEPTUS_MECH_SPIKES) {
            spike_block_configure(state);
        } else if (set->definitions[index].kind == DECEPTUS_MECH_BLOCK) {
            on_off_block_configure(&set->definitions[index], state);
        } else if (set->definitions[index].kind == DECEPTUS_MECH_LEVER) {
            lever_configure(state);
        }
        state->target_frame = state->frame;
    }
}

deceptus_u8 deceptus_mechanism_enable(deceptus_mechanism_set *set, deceptus_u16 source_id, deceptus_u8 enabled)
{
    deceptus_i16 index;
    index = deceptus_mechanism_find(set, source_id);
    if (index < 0) {
        return 0;
    }
    enabled = enabled != 0;
    if (set->states[index].enabled != enabled) {
        if (set->timings && (set->timings[index].flags & DECEPTUS_TIMED_ANIMATION) &&
            (set->definitions[index].kind == DECEPTUS_MECH_BLOCK ||
             set->definitions[index].kind == DECEPTUS_MECH_SPIKES)) {
            if (set->states[index].pending_count >= 8) {
                set->overflow = 1;
                return 0;
            }
            if (enabled) {
                set->states[index].pending |= (deceptus_u8)(1U << set->states[index].pending_count);
            }
            ++set->states[index].pending_count;
        }
        set->states[index].enabled = enabled;
        emit(set, source_id, DECEPTUS_MECH_ENABLED, enabled);
    }
    return 1;
}

void deceptus_mechanisms_tick(deceptus_mechanism_set *set, const deceptus_rect *player, deceptus_u8 interact_pressed)
{
    deceptus_u16 index;
    deceptus_u8 inside, entering;
    deceptus_mechanism_state *state;
    const deceptus_mechanism_def *definition;
    set->event_count = 0;
    set->overflow = 0;
    for (index = 0; index < set->count; ++index) {
        definition = &set->definitions[index];
        state = &set->states[index];
        inside = overlaps(player, &definition->bounds);
        entering = inside && !state->inside;
        if (set->timings) {
            if (set->timings[index].flags & DECEPTUS_TIMED_INTERVAL) {
                if (++state->timer >= (state->enabled ? set->timings[index].on_ticks : set->timings[index].off_ticks)) {
                    state->timer = 0;
                    deceptus_mechanism_enable(set, definition->source_id, !state->enabled);
                }
            }
            if (definition->kind == DECEPTUS_MECH_COLLAPSING && state->enabled) {
                collapse_tick(set, index, player);
            } else if (set->timings[index].flags & DECEPTUS_TIMED_ANIMATION) {
                if (definition->kind == DECEPTUS_MECH_LEVER) {
                    animate_lever(state);
                } else {
                    animate_switch(set, index);
                }
            }
        }
        if (definition->kind == DECEPTUS_MECH_LEVER) {
            if (!lever_interact(set, index, inside, interact_pressed)) {
                state->inside = inside;
                continue;
            }
        } else if (state->enabled || (definition->kind == DECEPTUS_MECH_SPIKES && set->timings)) {
            if (definition->kind == DECEPTUS_MECH_CHECKPOINT) {
                checkpoint_contact(set, index, entering);
            } else if (definition->kind == DECEPTUS_MECH_SENSOR) {
                sensor_rect_contact(set, index, inside);
            } else if (inside) {
                if (definition->kind == DECEPTUS_MECH_WIND) {
                    wind_contact(set, index);
                } else if (definition->kind == DECEPTUS_MECH_DAMAGE) {
                    damage_rect_contact(set, index);
                } else if (definition->kind == DECEPTUS_MECH_SPIKES) {
                    spike_block_contact(set, index);
                }
            }
        }
        state->inside = inside;
    }
}
