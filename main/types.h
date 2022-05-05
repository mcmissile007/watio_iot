#ifndef TYPES_H
#define TYPES_H

#define TODAY 0
#define TOMORROW 1
#define STEP 4
#define LEN_PRICES 24


typedef struct price_struct
{
    int day;
    int hour;
    int value;
} price_t;

#endif