#include <string.h>
#include <stdlib.h>
#include <time.h>

// sdk include
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "lwip/apps/sntp.h"
#include "esp_tls.h"
#include "cJSON.h"

#include "watio_app.h"
#include "info.h"
#include "http.h"
#include "types.h"
#include "ree.h"

#define TAG "watio_app"

#define MAX_PRICES_LEN 48

void setup_sntp()
{
    /*
    https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
    https://ftp.fau.de/aminet/util/time/tzinfo.txt
    The offset specifies the time value you must add to the local time to get a Coordinated Universal Time value.
    as CET is UTC+1 from CET to obtain UTC is necessary substract 1
    The dst string and offset specify the name and offset for the corresponding Daylight Saving Time zone;
    if the offset is omitted, it defaults to one hour ahead of standard time.
    If omitted time, the default is 02:00:00*/
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "es.pool.ntp.org");
}
static int get_min_position(price_t *prices, int len, int start)
{
    int min = INT_MAX;
    int min_index = 0;
    for (int i = start; i < len; i++)
    {
        if (prices[i].value < min)
        {
            min_index = i;
            min = prices[i].value;
        }
    }
    return min_index;
}
static void sort_prices(price_t *prices, int len)
{
    for (int i = 0; i < len - 1; i++)
    {
        int min_index = get_min_position(prices, len, i + 1);
        if (prices[min_index].value < prices[i].value)
        {
            price_t price_i = prices[i];
            prices[i] = prices[min_index];
            prices[min_index] = price_i;
        }
    }
}

static int get_prices(int day, int start_h, int end_h, price_t **prices, int total_num_prices)
{
    char ree_url[180];
    char ree_request[256];
    int num_prices = 0;
    cJSON *json_response = NULL;
    int len_new_prices = (end_h - start_h) + 1;
    ESP_LOGI(TAG, "total_num_prices: %d", total_num_prices);
    ESP_LOGI(TAG, "len_new_prices: %d", len_new_prices);
    if (get_ree_url(ree_url, day, start_h, end_h) == ESP_FAIL)
    {
        return num_prices;
    }
    get_ree_request(ree_url, ree_request);
    json_response = http_request(ree_url, ree_request);
    if (json_response == NULL)
    {
        goto exit;
    }
    num_prices = parse_json_response(json_response, prices,total_num_prices, len_new_prices);
    //ESP_LOGI(TAG, "num_prices: %d", num_prices);
    // ESP_LOGI(TAG, "value: %d",(int) prices[0].value);
    cJSON_Delete(json_response);
    // sort_prices(prices,num_prices);

    exit:
        cJSON_Delete(json_response);
    return num_prices;
}

static int get_all_available_prices(price_t **prices){
    int total_num_prices = 0;
    int num_prices = 0;
    for (int d=0; d<2; d++){
        for (int h =0; h<24; h+=4){
            ESP_LOGI(TAG, "return prices: %d:%d:%d ",d, h, h+3);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            num_prices = get_prices(d, h, h+3, prices, total_num_prices);
            ESP_LOGI(TAG, "return num_prices: %d", num_prices);
            if (num_prices <= 0){
                ESP_LOGE(TAG, "num_prices: %d", num_prices);
                return ESP_FAIL;
            }
            total_num_prices +=  num_prices;
            ESP_LOGI(TAG, "total num_prices: %d", total_num_prices);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    return total_num_prices;
}

static int get_today_prices(price_t **prices){
    int total_num_prices = 0;
    int num_prices = 0;
    for (int h =0; h<24; h+=4){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        num_prices = get_prices(0, h, h+3, prices, total_num_prices);
        ESP_LOGI(TAG, "return num_prices: %d", num_prices);
        if (num_prices <= 0){
            ESP_LOGE(TAG, "num_prices: %d", num_prices);
            return ESP_FAIL;
        }
        total_num_prices +=  num_prices;
        ESP_LOGI(TAG, "total num_prices: %d", total_num_prices);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    return total_num_prices;
}

static int get_tomorrow_prices(price_t **prices){
    int total_num_prices = 0;
    int num_prices = 0;
    for (int h =0; h<24; h+=4){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        num_prices = get_prices(1, h, h+3, prices, total_num_prices);
        ESP_LOGI(TAG, "return num_prices: %d", num_prices);
        if (num_prices <= 0){
            ESP_LOGE(TAG, "num_prices: %d", num_prices);
            return ESP_FAIL;
        }
        total_num_prices +=  num_prices;
        ESP_LOGI(TAG, "total num_prices: %d", total_num_prices);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    return total_num_prices;
}

static int get_prices_by_day(price_t **prices, int day){
    //day = 0 today
    //day = 1 tomorrow
    int total_num_prices = 0;
    int num_prices = 0;
    for (int h =0; h<24; h+=4){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        num_prices = get_prices(day, h, h+3, prices, total_num_prices);
        ESP_LOGI(TAG, "return num_prices: %d", num_prices);
        if (num_prices <= 0){
            ESP_LOGE(TAG, "num_prices: %d", num_prices);
            return ESP_FAIL;
        }
        total_num_prices +=  num_prices;
        ESP_LOGI(TAG, "total num_prices: %d", total_num_prices);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    return total_num_prices;
}
static void watio_app_task(void *pvParameters)
{
    time_t now = 0;
    struct tm timeinfo = {0};
    char strftime_buf[64];
    price_t *prices = NULL;
    int total_num_prices = 0;

    ESP_LOGI(TAG, "Initializing SNTP");
    setup_sntp();
    memory_info("watio_app_task");
    for (;;)
    {
        free(prices);
        prices = NULL;
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
        // ESP_LOGI(TAG, "The current date/time  is: %s", strftime_buf);
        if (timeinfo.tm_year < (2022 - 1900))
        {
            ESP_LOGI(TAG, "Error in datetime: %s", strftime_buf);
            sntp_stop();
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            sntp_init();
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        total_num_prices = get_prices_by_day(&prices,0);
        for (int i = 0 ; i < total_num_prices ; i ++){
            ESP_LOGI(TAG, "%d.value: %d",i,(int) prices[i].value);
            ESP_LOGI(TAG, "%d.hour: %d",i, prices[i].timeinfo.tm_hour);
        }
        ESP_LOGI(TAG,"Sorted prices by value:");
        sort_prices(prices,total_num_prices);
        for (int i = 0 ; i < total_num_prices ; i ++){
            ESP_LOGI(TAG, "%d.value: %d",i,(int) prices[i].value);
            ESP_LOGI(TAG, "%d.hour: %d",i, prices[i].timeinfo.tm_hour);
        }
        memory_info("watio_app_task end getting prices.");
        vTaskDelay(30000 / portTICK_PERIOD_MS);



    }
    vTaskDelete(NULL);
}

void start_watio_app()
{
    memory_info(TAG);
    xTaskCreate(&watio_app_task, "watio_app_task", 8192, NULL, 4, NULL);
}