#ifndef DECEPTUS_MECHANISM_INTERNAL_H
#define DECEPTUS_MECHANISM_INTERNAL_H

/* private cross-family helpers; these modules share one compilation unit. */
static deceptus_u8 on_off_block_solid(const deceptus_mechanism_set *set, deceptus_u16 index, const deceptus_rect *bounds);
static deceptus_u8 collapsing_platform_solid(const deceptus_mechanism_set *set, deceptus_u16 index, const deceptus_rect *bounds);
static void damage_rect_contact(deceptus_mechanism_set *set, deceptus_u16 index);

#endif
