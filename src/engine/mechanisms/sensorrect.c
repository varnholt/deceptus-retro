/* original counterpart: src/game/mechanisms/sensorrect.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void sensor_rect_contact(deceptus_mechanism_set *set, deceptus_u16 index, deceptus_u8 inside)
{
    if (inside != set->states[index].inside) {
        emit(set, set->definitions[index].source_id, inside ? DECEPTUS_MECH_ENTER : DECEPTUS_MECH_LEAVE, inside);
    }
}
