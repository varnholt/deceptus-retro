/* original counterpart: src/game/mechanisms/damagerect.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

static void damage_rect_contact(deceptus_mechanism_set *set, deceptus_u16 index)
{
    emit(set, set->definitions[index].source_id, DECEPTUS_MECH_HIT, set->definitions[index].value);
}
