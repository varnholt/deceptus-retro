/* original counterpart: src/game/mechanisms/blockingrect.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

deceptus_u8 deceptus_mechanism_solid(const deceptus_mechanism_set *set, const deceptus_rect *bounds)
{
    deceptus_u16 index;
    if (!set) {
        return 0;
    }
    for (index = 0; index < set->count; ++index) {
        if (set->definitions[index].kind == DECEPTUS_MECH_BLOCK) {
            if (on_off_block_solid(set, index, bounds)) {
                return 1;
            }
        } else if (set->definitions[index].kind == DECEPTUS_MECH_COLLAPSING) {
            if (collapsing_platform_solid(set, index, bounds)) {
                return 1;
            }
        }
    }
    return 0;
}
