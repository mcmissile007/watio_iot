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
#include "io.h"

#define TAG "watio_app"

#define MAX_PRICES_LEN 48
#define UPDATE_HOUR 20
#define UPDATE_MIN 30

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
static bool is_better_n_hour(int hour,int n,price_t *sorted_prices,int len){
    int better_count=0;
    for (int i=0; i < len ; i++){
        if (sorted_prices[i].hour != hour){
            better_count ++;
            if (better_count >=n) return false;
        }
        else{
            return true;
        }
    }
    return true;
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


static int get_prices(int day, int start_h, int end_h, price_t *prices, int total_num_prices)
{
    char url[180];
    char request[256];
    int num_prices = 0;
    cJSON *json_response = NULL;
    if (get_ree_url(url, day, start_h, end_h) == ESP_FAIL)
    {
        return num_prices;
    }
    get_ree_request(url, request);
    json_response = http_request(url, request);
    if (json_response == NULL)
    {
        goto exit;
    }
    num_prices = parse_json_response(json_response, prices,total_num_prices);
    cJSON_Delete(json_response);
    exit:
        cJSON_Delete(json_response);
    return num_prices;
}
static bool check_if_already_have(price_t *prices, int len, int day, int h, int step){
    int count_ok = 0;
    for (int i = h; i <= h+step-1; i++){
        ESP_LOGI(TAG, "check_if_already_have: %d", i);
        if (prices[i].hour == h && prices[i].day == day && prices[i].value != 0){
            count_ok += 1;
            ESP_LOGI(TAG, "count_ok %d", count_ok);
        }
    }
    if (count_ok == step){
        ESP_LOGI(TAG, "count_ok equal to step: %d", step);
        ESP_LOGI(TAG, "ccheck_if_already_have true");
        return true;
    }
    ESP_LOGI(TAG, "check_if_already_have false");
    return false;
}
static int get_prices_by_day(price_t *prices, int len, int day, int step){
    //day = 0 today
    //day = 1 tomorrow
    int current_num_prices = 0;
    int num_prices = 0;
    for (int h =0; h<24; h+=step){
        if (check_if_already_have(prices,len,day,h,step)){
            continue;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        num_prices = get_prices(day, h, h+step-1, prices,current_num_prices);
        ESP_LOGI(TAG, "return num_prices: %d", num_prices);
        if (num_prices <= 0){
            ESP_LOGE(TAG, "num_prices: %d", num_prices);
            return ESP_FAIL;
        }
        current_num_prices +=  num_prices;
        ESP_LOGI(TAG, "current num_prices: %d", current_num_prices);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    return current_num_prices;
}

static int load_prices(price_t *prices,int len, int day, int step){
    int num_prices = 0;
    time_t now = 0;
    struct tm timeinfo = {0};
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2022 - 1900))
    {
       return ESP_FAIL;
    }
    num_prices = get_prices_by_day(prices,len,day,step);
    if (num_prices < len){
        return ESP_FAIL;
    }
    return ESP_OK;

}
/*
void show_prices(price_t * prices, int len){
    for (int i = 0 ; i < len ; i ++){
        ESP_LOGI(TAG, "%d.value: %d",i,(int) prices[i].value);
        ESP_LOGI(TAG, "%d.hour: %d",i, prices[i].hour);
        ESP_LOGI(TAG, "%d.day: %d",i, prices[i].day);
    }
}
*/

void show_line_prices(price_t * prices, int len){
    for (int i = 0 ; i < len ; i ++){
        ESP_LOGI(TAG, "%d->value: %d hour: %d  day: %d",i,(int) prices[i].value,prices[i].hour,prices[i].day);
    }
}


int validate_prices(price_t *prices, int len,int day){
    int count_ok = 0;
    time_t now = 0;
    struct tm tm_now = {0};
    time(&now);
    now = now + (day*86400);
    localtime_r(&now, &tm_now);
    if (tm_now.tm_year < (2022 - 1900))
    {
        return ESP_FAIL;
    }
    int day_now = tm_now.tm_mday;
    for (int h = 0; h < 24; h++){
        for (int i = 0 ; i < len ; i ++){
            if (prices[i].hour == h && prices[i].day == day_now){
                count_ok += 1;
                break;
            }
        }
    }
    if (count_ok ==  24){
        return ESP_OK;
    }
    return ESP_FAIL;

}

void initialize_scheduler(scheduler_t * scheduler,int len){
    for (int i=0; i < len; i++){
        scheduler[i].state = DEFAULT_STATE;
        scheduler[i].last_execution = 0;
    }
}

void show_scheduler(scheduler_t * scheduler,int len){
    for (int i=0; i < len; i++){
        ESP_LOGI(TAG,"hour:%d, state:%d",i,scheduler[i].state);
    }
}

void update_scheduler(price_t * prices,scheduler_t * scheduler,int len, int n){
     for (int i=0; i < len; i++){
         scheduler[i].state = is_better_n_hour(i,n,prices,len);
     }

}



static void watio_app_task(void *pvParameters)
{
    time_t now = 0;
    struct tm tminfo = {0};
    char strftime_buf[64];
    price_t today_prices[LEN_PRICES] = {0};
    price_t tomorrow_prices[LEN_PRICES] = {0};
    scheduler_t scheduler[LEN_PRICES] = {0};
    int validate = 0;
    int iter = 0;
    int rand_hour=0;
    int rand_min= 0;
    int N = 12;
    initialize_scheduler(scheduler,LEN_PRICES);

    ESP_LOGI(TAG, "Initializing SNTP");
    setup_sntp();
    memory_info("watio_app_task");
    //switch_on_relay();
    for (;;)
    {
        time(&now);
        localtime_r(&now, &tminfo);
        if (tminfo.tm_year < (2022 - 1900))
        {
            ESP_LOGI(TAG, "Error in datetime:");
            sntp_stop();
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            sntp_init();
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        if (rand_hour == 0 && rand_min == 0)
        {
            srand(time(&now));
            rand_hour= rand() % 3; //0, 1,2
            rand_min= rand() % 50; //0...49
            // (49 * 3 ) - 1 =  146 slots.
        }
        strftime(strftime_buf, sizeof(strftime_buf), "%d/%m/%Y %H:%M:%S", &tminfo);
        ESP_LOGI(TAG, "The current date/time  is: %s", strftime_buf);
        validate = validate_prices(today_prices,LEN_PRICES,TODAY);
        ESP_LOGI(TAG, "validate_prices for today: %d", validate);
        if (validate != ESP_OK){
            ESP_LOGI(TAG, "must load today prices");
            int retval = load_prices(today_prices,LEN_PRICES,TODAY, STEP);
            if (retval == ESP_FAIL){
                vTaskDelay(5000 / portTICK_PERIOD_MS);
                continue;
            }
            sort_prices(today_prices,LEN_PRICES);
            update_scheduler(today_prices,scheduler,LEN_PRICES,N);
            show_scheduler(scheduler,LEN_PRICES);
        }
        if (tminfo.tm_hour == 23 && tminfo.tm_min == 59){
            ESP_LOGI(TAG, "End of day");
            ESP_LOGI(TAG, "The current date/time  is: %s", strftime_buf);
            memcpy(&today_prices,&tomorrow_prices,sizeof(tomorrow_prices));
            bzero(&tomorrow_prices,sizeof(tomorrow_prices));
            sort_prices(today_prices,LEN_PRICES);
            update_scheduler(today_prices,scheduler,LEN_PRICES,N);
            show_scheduler(scheduler,LEN_PRICES);
            rand_hour= rand() % 3; //0, 1,2
            rand_min= rand() % 50; //0...49
            //just one execution.
            vTaskDelay(65000 / portTICK_PERIOD_MS);
            continue;
        }
        int time_since_last_update = now -  scheduler[tminfo.tm_hour].last_execution;
        ESP_LOGI(TAG, "time_since_last_update: %d", time_since_last_update);
        bool state = scheduler[tminfo.tm_hour].state;
        ESP_LOGI(TAG, "output must be: %s",state?"on":"off");
        if (time_since_last_update > 60){
            switch_relay(state);
            ESP_LOGI(TAG, "switch relay: %s",state?"on":"off");
            scheduler[tminfo.tm_hour].last_execution = mktime(&tminfo);
        }

        if (tminfo.tm_hour >= (21+rand_hour) && tminfo.tm_min >= rand_min){
            ESP_LOGI(TAG, "It's time to get tomorrow prices");
            ESP_LOGI(TAG, "The current date/time  is: %s", strftime_buf);
            validate = validate_prices(tomorrow_prices,LEN_PRICES,TOMORROW);
            ESP_LOGI(TAG, "validate_prices for tomorrow: %d", validate);
            if (validate != ESP_OK){
                ESP_LOGI(TAG, "must load tomorrow prices");
                int retval = load_prices(tomorrow_prices,LEN_PRICES,TOMORROW, STEP);
                if (retval == ESP_FAIL){
                    vTaskDelay(5000 / portTICK_PERIOD_MS);
                    continue;
                }
                sort_prices(tomorrow_prices,LEN_PRICES);
            }
        }

        ESP_LOGI(TAG, "today prices:");
        show_line_prices(today_prices,LEN_PRICES);
        ESP_LOGI(TAG, "tomorrow prices:");
        show_line_prices(tomorrow_prices,LEN_PRICES);
        ESP_LOGI(TAG, "scheduler:");
        show_scheduler(scheduler,LEN_PRICES);
        iter += 1;
        ESP_LOGI(TAG, ".iter: %d, rand_hour:%d, rand_min:%d", iter,21 + rand_hour,rand_min);
        ESP_LOGI(TAG, "The current date/time  is: %s", strftime_buf);
        vTaskDelay(10000 / portTICK_PERIOD_MS);

    }
    vTaskDelete(NULL);
}

void start_watio_app()
{
    memory_info(TAG);
    xTaskCreate(&watio_app_task, "watio_app_task", 8192, NULL, 4, NULL);
}