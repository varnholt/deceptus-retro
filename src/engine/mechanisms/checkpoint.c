/* original counterpart: src/game/mechanisms/checkpoint.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void checkpoint_contact(deceptus_mechanism_set *set, deceptus_u16 index, deceptus_u8 entering)
{
    if (entering && !set->states[index].triggered) {
        set->states[index].triggered = 1;
        emit(set, set->definitions[index].source_id, DECEPTUS_MECH_REACHED, 1);
    }
}
