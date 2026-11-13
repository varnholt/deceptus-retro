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
    }
    return 1;
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
        set->states[index].enabled = enabled;
        emit(set, source_id, RETRO_MECH_ENABLED, enabled);
    }
    return 1;
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
        if (definition->kind == RETRO_MECH_LEVER) {
            if (inside && interact_pressed && !(definition->flags & RETRO_MECH_MISSING_HANDLE)) {
                retro_mechanism_enable(set, definition->source_id, !state->enabled);
                for (target = 0; target < definition->target_count; ++target) {
                    retro_mechanism_enable(set, set->targets[definition->target_first + target], state->enabled);
                }
            }
        } else if (state->enabled) {
            if (definition->kind == RETRO_MECH_CHECKPOINT && entering && !state->triggered) {
                state->triggered = 1;
                emit(set, definition->source_id, RETRO_MECH_REACHED, 1);
            } else if (definition->kind == RETRO_MECH_SENSOR && inside != state->inside) {
                emit(set, definition->source_id, inside ? RETRO_MECH_ENTER : RETRO_MECH_LEAVE, inside);
            } else if ((definition->kind == RETRO_MECH_DAMAGE || definition->kind == RETRO_MECH_SPIKES) && inside) {
                emit(set, definition->source_id, RETRO_MECH_HIT, definition->value);
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
        if (definition->kind != RETRO_MECH_BLOCK) {
            continue;
        }
        enabled = set->states[index].enabled;
        if (definition->flags & RETRO_MECH_INVERTED) {
            enabled = !enabled;
        }
        if (enabled && overlaps(bounds, &definition->bounds)) {
            return 1;
        }
    }
    return 0;
}
