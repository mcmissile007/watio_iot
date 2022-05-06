#ifndef TYPES_H
#define TYPES_H

#define TODAY 0
#define TOMORROW 1
#define STEP 4
#define LEN_PRICES 24
#define DEFAULT_STATE 0

typedef struct price_struct
{
    uint8_t day;
    uint8_t hour;
    uint16_t value;
} price_t;

typedef struct scheduler_struct
{
    bool state;
    int last_execution;
}scheduler_t;

#endif