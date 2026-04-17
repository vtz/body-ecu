#pragma once

#define ASYNC_CONFIG_TICK_IN_US (1000U)
#define ASYNC_CONFIG_NESTED_INTERRUPTS (1)

enum
{
    TASK_BACKGROUND = 1,
    TASK_BODY,
    TASK_SOMEIP,
    TASK_DIAG,
    TASK_SYSADMIN,
    // --------------------
    ASYNC_CONFIG_TASK_COUNT,
};

enum
{
    ISR_GROUP_TEST = 0,
    ISR_GROUP_COUNT,
};
