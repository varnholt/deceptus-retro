#ifndef RETRO_LOGIC_H
#define RETRO_LOGIC_H

#include "retro/mechanisms.h"

#define RETRO_LOGIC_OK 0
#define RETRO_LOGIC_BAD_CODE 1
#define RETRO_LOGIC_MISSING_TARGET 2
#define RETRO_LOGIC_BUDGET 3

/* executes bounded boolean callbacks compiled from source Lua. writes commit
 * only after END, so an unsupported target or malformed program changes nothing. */
retro_u8 retro_logic_execute(retro_mechanism_set *set, const retro_u8 *code, retro_u16 length, retro_u8 arguments);

#endif
