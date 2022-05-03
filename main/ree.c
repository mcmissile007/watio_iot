#include <string.h>
#include <stdlib.h>
#include <time.h>

// sdk include
#include "cJSON.h"
#include "esp_log.h"


#include "ree.h"
#include "types.h"

#define TAG "ree"



void get_ree_request(const char *url, char *request)
{
    strcpy(request, "GET ");
    strncat(request, url, strlen(url));
    strcat(request, " HTTP/1.0\r\n");
    strcat(request, "Host: apidatos.ree.es\r\n");
    strcat(request, "User-Agent: esp-idf/1.0 esp32\r\n");
    strcat(request, "\r\n");
}

int get_ree_url(char *url, int day, int start_h, int end_h)
{
    /*
    if days = 0 just for today prices
    if days = 1 prices for tomowwow
    ...and so on
    start_h 0,1,2,3.....23
    end_h 0,1,2,3,4, ...23
    */
    time_t now;
    struct tm start;
    struct tm end;
    char strftime_buf[32];
    char start_buf[32];
    char end_buf[32];
    time(&now);
    now = now + (86400 * day);
    localtime_r(&now, &start);
    if (start.tm_year < (2022 - 1900))
    {
        ESP_LOGE(TAG, "Error in time no url query");
        return -1;
    }
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H:%M", &start);
    ESP_LOGI(TAG, "The current date/time  is: %s", strftime_buf);
    localtime_r(&now, &end);
    end.tm_hour = end_h;
    end.tm_min = 59;
    start.tm_hour = start_h;
    start.tm_min = 0;
    strftime(start_buf, sizeof(start_buf), "%Y-%m-%dT%H:%M", &start);
    strftime(end_buf, sizeof(end_buf), "%Y-%m-%dT%H:%M", &end);
    ESP_LOGI(TAG, "start: %s", start_buf);
    ESP_LOGI(TAG, "end: %s", end_buf);
    strcpy(url, "https://apidatos.ree.es/es/datos/mercados/precios-mercados-tiempo-real?start_date=");
    strncat(url, start_buf, strlen(start_buf));
    strcat(url, "&end_date=");
    strncat(url, end_buf, strlen(end_buf));
    strcat(url, "&time_trunc=hour&geo_ids=8741");
    ESP_LOGI(TAG, "url: %s", url);
    return 0;
}

int parse_json_response(const cJSON *json_response, price_t **prices, int total_num_prices, int len_new_prices)
{
    //allocated heap memory for prices that must be free.
    int num_prices = 0;
    int num_included = 0;
    const cJSON *json_data = NULL;
    const cJSON *json_errors = NULL;
    const cJSON *json_type = NULL;
    const cJSON *json_array_included = NULL;
    const cJSON *json_item_included = NULL;
    int new_size = total_num_prices +len_new_prices + 1;
    *prices = (price_t *) realloc(*prices,sizeof(price_t)*(new_size));
    json_errors = cJSON_GetObjectItemCaseSensitive(json_response, "errors");
    if (json_errors != NULL)
    {
        ESP_LOGE(TAG, "Errors in json response");
        return 0;
    }

    json_data = cJSON_GetObjectItemCaseSensitive(json_response, "data");
    if (json_data == NULL)
    {
        ESP_LOGE(TAG, "No data in json response");
        return 0;
    }
    json_type = cJSON_GetObjectItemCaseSensitive(json_data, "type");
    if (json_type == NULL)
    {
        ESP_LOGE(TAG, "No type in json response");
        return 0;
    }
    json_array_included = cJSON_GetObjectItemCaseSensitive(json_response, "included");
    if (json_array_included == NULL)
    {
        ESP_LOGE(TAG, "No json_array_included in response");
        return 0;
    }
    cJSON_ArrayForEach(json_item_included, json_array_included)
    {

        if (num_included == 0)
        {
            cJSON *json_attributes = cJSON_GetObjectItemCaseSensitive(json_item_included, "attributes");
            cJSON *json_array_values = cJSON_GetObjectItemCaseSensitive(json_attributes, "values");
            cJSON *json_item_value = NULL;
            cJSON_ArrayForEach(json_item_value, json_array_values)
            {
                price_t price = {0};
                int validate = 0;
                cJSON *json_value = cJSON_GetObjectItemCaseSensitive(json_item_value, "value");
                cJSON *json_datetime = cJSON_GetObjectItemCaseSensitive(json_item_value, "datetime");
                if (cJSON_IsString(json_datetime) && (json_datetime->valuestring != NULL))
                {
                    //ESP_LOGI(TAG, "datime: %s", json_datetime->valuestring);
                    strptime(json_datetime->valuestring, "%Y-%m-%dT%H:%M:%S.%f%z", &price.timeinfo);
                    // prices[num_prices].epoch = mktime(&prices[num_prices].timeinfo);
                    validate += 1;
                }
                if (cJSON_IsNumber(json_value))
                {
                    double value = json_value->valuedouble;
                    price.value = (int) value;
                    validate += 1;
                }
                if (validate == 2){
                    (*prices)[total_num_prices + num_prices] = price;
                    ++num_prices;
                }
            }
        }
        ++num_included;
    }

    return num_prices;
}