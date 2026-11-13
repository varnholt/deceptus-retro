#include "deceptus/engine.h"
#include <string.h>

deceptus_entity *deceptus_spawn(deceptus_engine *engine, deceptus_u8 kind, deceptus_i16 x, deceptus_i16 y)
{
    deceptus_u16 index;
    deceptus_entity *entity;
    for (index = 0; index < DECEPTUS_ENTITY_CAPACITY; ++index) {
        entity = &engine->entities[index];
        if (!entity->active) {
            memset(entity, 0, sizeof(*entity));
            entity->active = 1;
            entity->kind = kind;
            entity->x = DECEPTUS_FIXED(x);
            entity->y = DECEPTUS_FIXED(y);
            return entity;
        }
    }
    return 0;
}
