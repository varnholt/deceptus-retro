/* original counterpart: src/game/mechanisms/blockingrect.cpp. */
/* private mechanism implementation; compiled through ../mechanisms.c. */

retro_u8 retro_mechanism_solid(const retro_mechanism_set *set, const retro_rect *bounds)
{
    retro_u16 index;
    if (!set) {
        return 0;
    }
    for (index = 0; index < set->count; ++index) {
        if (set->definitions[index].kind == RETRO_MECH_BLOCK) {
            if (on_off_block_solid(set, index, bounds)) {
                return 1;
            }
        } else if (set->definitions[index].kind == RETRO_MECH_COLLAPSING) {
            if (collapsing_platform_solid(set, index, bounds)) {
                return 1;
            }
        }
    }
    return 0;
}
