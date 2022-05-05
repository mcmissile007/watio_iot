#include "types.h"
#include "cJSON.h"

#ifndef REE_H
#define REE_H
void get_ree_request(const char *url, char *request);
int get_ree_url(char *url, int day, int start_h, int end_h);
int parse_json_response(const cJSON *json_response, price_t *prices, int current_num_prices);
#endif