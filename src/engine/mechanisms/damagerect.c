/* original counterpart: src/game/mechanisms/damagerect.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void damage_rect_contact(retro_mechanism_set *set, retro_u16 index)
{
    emit(set, set->definitions[index].source_id, RETRO_MECH_HIT, set->definitions[index].value);
}
