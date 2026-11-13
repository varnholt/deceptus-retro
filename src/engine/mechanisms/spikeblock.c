/* original counterpart: src/game/mechanisms/spikeblock.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void spike_block_configure(retro_mechanism_state *state) { state->frame = state->enabled ? 32 : 0; }

static void spike_block_contact(retro_mechanism_set *set, retro_u16 index)
{
    if (!set->timings || (set->states[index].frame >= 29 && set->states[index].frame <= 35)) {
        damage_rect_contact(set, index);
    }
}
