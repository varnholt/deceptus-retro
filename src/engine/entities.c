#include "retro/engine.h"
#include <string.h>

retro_entity *retro_spawn(retro_engine *engine, retro_u8 kind, retro_i16 x, retro_i16 y)
{
    retro_u16 index;
    retro_entity *entity;
    for (index = 0; index < RETRO_ENTITY_CAPACITY; ++index) {
        entity = &engine->entities[index];
        if (!entity->active) {
            memset(entity, 0, sizeof(*entity));
            entity->active = 1;
            entity->kind = kind;
            entity->x = RETRO_FIXED(x);
            entity->y = RETRO_FIXED(y);
            return entity;
        }
    }
    return 0;
}
