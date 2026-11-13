#include "deceptus/logic.h"

deceptus_u8 deceptus_logic_execute(deceptus_mechanism_set *set, const deceptus_u8 *code, deceptus_u16 length, deceptus_u8 arguments)
{
    deceptus_u8 stack[16], enabled[DECEPTUS_MECHANISM_CAPACITY], visible[DECEPTUS_MECHANISM_CAPACITY];
    deceptus_u16 position, steps, operand, index;
    deceptus_i16 slot;
    deceptus_u8 depth, opcode, value;
    if (!code || !length || length > 2048 || set->count > DECEPTUS_MECHANISM_CAPACITY) {
        return DECEPTUS_LOGIC_BAD_CODE;
    }
    for (index = 0; index < set->count; ++index) {
        enabled[index] = set->states[index].enabled;
        visible[index] = set->states[index].visible;
    }
    position = 0;
    depth = 0;
    for (steps = 0; steps < 512; ++steps) {
        if (position >= length) {
            return DECEPTUS_LOGIC_BAD_CODE;
        }
        opcode = code[position++];
        if (opcode == 0) {
            if (depth) {
                return DECEPTUS_LOGIC_BAD_CODE;
            }
            for (index = 0; index < set->count; ++index) {
                if (set->timings && (set->timings[index].flags & DECEPTUS_TIMED_ANIMATION) &&
                    (set->definitions[index].kind == DECEPTUS_MECH_BLOCK ||
                     set->definitions[index].kind == DECEPTUS_MECH_SPIKES) &&
                    enabled[index] != set->states[index].enabled && set->states[index].pending_count >= 8) {
                    return DECEPTUS_LOGIC_BUDGET;
                }
            }
            for (index = 0; index < set->count; ++index) {
                deceptus_mechanism_enable(set, set->definitions[index].source_id, enabled[index]);
                set->states[index].visible = visible[index];
            }
            return DECEPTUS_LOGIC_OK;
        }
        if (opcode == 1 || opcode == 2 || opcode == 3 || opcode == 12) {
            if (depth >= 16) {
                return DECEPTUS_LOGIC_BAD_CODE;
            }
        }
        if (opcode == 1 || opcode == 2) {
            stack[depth++] = opcode == 2;
        } else if (opcode == 3) {
            if (position >= length || code[position] >= 8) {
                return DECEPTUS_LOGIC_BAD_CODE;
            }
            stack[depth++] = (arguments >> code[position++]) & 1;
        } else if (opcode == 4) {
            if (!depth) {
                return DECEPTUS_LOGIC_BAD_CODE;
            }
            stack[depth - 1] = !stack[depth - 1];
        } else if (opcode == 5 || opcode == 6) {
            if (depth < 2) {
                return DECEPTUS_LOGIC_BAD_CODE;
            }
            value = stack[--depth];
            stack[depth - 1] = opcode == 5 ? stack[depth - 1] && value : stack[depth - 1] || value;
        } else if (opcode >= 7 && opcode <= 12) {
            if (length - position < 2) {
                return DECEPTUS_LOGIC_BAD_CODE;
            }
            operand = code[position] | ((deceptus_u16)code[position + 1] << 8);
            position += 2;
            if (opcode == 10 || opcode == 11) {
                if (operand >= length || (opcode == 10 && !depth)) {
                    return DECEPTUS_LOGIC_BAD_CODE;
                }
                if (opcode == 11 || !stack[--depth]) {
                    position = operand;
                }
                continue;
            }
            slot = deceptus_mechanism_find(set, operand);
            if (slot < 0) {
                return DECEPTUS_LOGIC_MISSING_TARGET;
            }
            if (opcode == 12) {
                stack[depth++] = enabled[slot];
            } else if (opcode == 9) {
                enabled[slot] = !enabled[slot];
            } else {
                if (!depth) {
                    return DECEPTUS_LOGIC_BAD_CODE;
                }
                value = stack[--depth];
                if (opcode == 7) {
                    enabled[slot] = value;
                } else {
                    visible[slot] = value;
                }
            }
        } else {
            return DECEPTUS_LOGIC_BAD_CODE;
        }
    }
    return DECEPTUS_LOGIC_BUDGET;
}
