#ifndef RETRO_MECHANISM_INTERNAL_H
#define RETRO_MECHANISM_INTERNAL_H

/* private cross-family helpers; these modules share one compilation unit. */
static retro_u8 on_off_block_solid(const retro_mechanism_set *set, retro_u16 index, const retro_rect *bounds);
static retro_u8 collapsing_platform_solid(const retro_mechanism_set *set, retro_u16 index, const retro_rect *bounds);
static void damage_rect_contact(retro_mechanism_set *set, retro_u16 index);

#endif
