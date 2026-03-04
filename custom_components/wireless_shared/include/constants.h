#pragma once
#define NOT_IMPLEMENTED (0)
#define ENABLED (1)
#define DISABLED (0)

typedef enum {
    TRISTATE_TRUE = 1,
    TRISTATE_FALSE = 0,
    TRISTATE_UNINIT = -1
} tristate_bool_t;