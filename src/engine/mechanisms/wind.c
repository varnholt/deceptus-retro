/* original counterpart: src/game/mechanisms/wind.cpp. */
/* private implementation, compiled through mechanisms.c. */
static void wind_contact(deceptus_mechanism_set *set, deceptus_u16 index)
{
    emit(set, set->definitions[index].source_id, DECEPTUS_MECH_FORCE, set->definitions[index].value);
}
