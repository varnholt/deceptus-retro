/* shared onoffblock/spikeblock animation, retaining their original frame rules. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

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
