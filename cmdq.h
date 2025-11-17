#ifndef CMDQ_H
#define CMDQ_H
#include <stdbool.h>
#include "types.h"

void cmdq_init(void);
void cmdq_push(CommandType t, int arg);
bool cmdq_pop(Command *out);
void cmdq_stop(void);

#endif
