/* original counterpart: src/game/mechanisms/checkpoint.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void checkpoint_contact(retro_mechanism_set *set, retro_u16 index, retro_u8 entering)
{
    if (entering && !set->states[index].triggered) {
        set->states[index].triggered = 1;
        emit(set, set->definitions[index].source_id, RETRO_MECH_REACHED, 1);
    }
}
