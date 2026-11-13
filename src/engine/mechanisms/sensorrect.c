/* original counterpart: src/game/mechanisms/sensorrect.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void sensor_rect_contact(retro_mechanism_set *set, retro_u16 index, retro_u8 inside)
{
    if (inside != set->states[index].inside) {
        emit(set, set->definitions[index].source_id, inside ? RETRO_MECH_ENTER : RETRO_MECH_LEAVE, inside);
    }
}
