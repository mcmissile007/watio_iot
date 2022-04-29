#include <string.h>
#include <stdlib.h>
#include <time.h>

//sdk include
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

#define TAG "watio_app"

#define MAX_PRICES_LEN 48

typedef struct price_struct {
    struct tm timeinfo;
    double value;
} price_t;



static void get_ree_request(const char * url,char * request){
    strcpy(request,"GET ");
    strncat(request,url,strlen(url));
    strcat(request," HTTP/1.0\r\n");
    strcat(request,"Host: apidatos.ree.es\r\n");
    strcat(request, "User-Agent: esp-idf/1.0 esp32\r\n");
    strcat(request,"\r\n");
    
}

static int  get_ree_url(char * url, int day,int start_h, int end_h){
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
    if (start.tm_year < (2022-1900)){
        ESP_LOGE(TAG,"Error in time no url query");
        return ESP_FAIL;

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
    strcpy(url,"https://apidatos.ree.es/es/datos/mercados/precios-mercados-tiempo-real?start_date=");
    strncat(url,start_buf,strlen(start_buf));
    strcat(url,"&end_date=");
    strncat(url,end_buf,strlen(end_buf));
    strcat(url,"&time_trunc=hour&geo_ids=8741");
    ESP_LOGI(TAG, "url: %s", url);
    return ESP_OK;
}



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
    setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);
    tzset();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "es.pool.ntp.org");

}
static int get_min_position(price_t * prices,int len, int start){
    int min = INT_MAX;
    int min_index = 0;
    for (int i=start; i<len; i++){
        if (prices[i].value < min){
            min_index = i;
            min = prices[i].value;
        }
    }
    return min_index;
}
static void sort_prices(price_t *prices, int len){
    for(int i = 0; i < len-1; i ++){
        int min_index = get_min_position(prices,len,i+1);
        if (prices[min_index].value < prices[i].value){
             price_t price_i = prices[i];
             prices[i] = prices[min_index];
             prices[min_index] = price_i;
        }
    }
}
static int parse_json_response( const cJSON *json_response,price_t *prices, int offset, int max_len){
    int num_prices = 0;
    int num_included = 0;
    const cJSON *json_data = NULL;
    const cJSON *json_errors = NULL;
    const cJSON *json_type = NULL;
    const cJSON *json_array_included = NULL;
    const cJSON *json_item_included = NULL;
    json_errors = cJSON_GetObjectItemCaseSensitive(json_response, "errors");
    if (json_errors != NULL)
    {
        ESP_LOGE(TAG,"Errors in json response");
        return 0;
    }

    json_data = cJSON_GetObjectItemCaseSensitive(json_response, "data");
    if (json_data == NULL){
        ESP_LOGE(TAG,"No data in json response");
        return 0;

    }
    json_type = cJSON_GetObjectItemCaseSensitive(json_data, "type");
    if (json_type == NULL){
        ESP_LOGE(TAG,"No type in json response");
        return 0;

    }
    if (cJSON_IsString(json_type) && (json_type->valuestring != NULL))
    {
        ESP_LOGI(TAG, "type: %s", json_type->valuestring);
    }
    json_array_included = cJSON_GetObjectItemCaseSensitive(json_response, "included");
    if (json_array_included == NULL){
        ESP_LOGE(TAG,"No json_array_included in response");
        return 0;

    }
    cJSON_ArrayForEach(json_item_included,json_array_included){

        if (num_included == 0){
            num_prices = offset;
            cJSON *json_attributes = cJSON_GetObjectItemCaseSensitive(json_item_included, "attributes");
            cJSON *json_array_values = cJSON_GetObjectItemCaseSensitive(json_attributes, "values");
            cJSON *json_item_value = NULL;
            cJSON_ArrayForEach(json_item_value,json_array_values){
                cJSON *json_value = cJSON_GetObjectItemCaseSensitive(json_item_value, "value");
                cJSON *json_datetime = cJSON_GetObjectItemCaseSensitive(json_item_value, "datetime");
                if (cJSON_IsString(json_datetime) && (json_datetime->valuestring != NULL))
                {
                    //ESP_LOGI(TAG, "datime: %s", json_datetime->valuestring);
                     strptime(json_datetime->valuestring, "%Y-%m-%dT%H:%M:%S.%f%z", &prices[num_prices].timeinfo);
                    //prices[num_prices].epoch = mktime(&prices[num_prices].timeinfo);
                }
                if (cJSON_IsNumber(json_value)){
                    double value = json_value->valuedouble;
                    prices[num_prices].value = value;
                    //ESP_LOGI(TAG, "value: %d", (int) value);
                }
                ++num_prices;
                if (num_prices >= max_len){
                    return num_prices;
                }

            }
        }
        ++num_included;

    }

    return num_prices;
}

static int get_prices(int day,int start_h, int end_h)
{
    char ree_url[180];
    char ree_request[256];
    cJSON *json_response = NULL;
    price_t  prices[MAX_PRICES_LEN];

    if (get_ree_url(ree_url,day,start_h,end_h) == ESP_FAIL){
        return ESP_FAIL;
    }
    get_ree_request(ree_url,ree_request);
    json_response = http_request(ree_url,ree_request);
    if (json_response == NULL){
        goto exit;
    }
    memset(&prices,0,sizeof(prices));
    int num_prices = parse_json_response(json_response,prices,0,MAX_PRICES_LEN);
    cJSON_Delete(json_response);
    //sort_prices(prices,num_prices);

    for (int i = 0 ; i < num_prices ; i ++){
        ESP_LOGI(TAG, "%d.value: %d",i,(int) prices[i].value);
        ESP_LOGI(TAG, "%d.hour: %d",i, prices[i].timeinfo.tm_hour);
    }
    exit:
        cJSON_Delete(json_response);
    return ESP_OK;

}

static void watio_app_task(void *pvParameters)
{
    time_t now = 0;
    struct tm timeinfo = { 0 };
    char strftime_buf[64];
   

    ESP_LOGI(TAG, "Initializing SNTP");
    setup_sntp();
    memory_info("watio_app_task");
    for(;;){
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
        //ESP_LOGI(TAG, "The current date/time  is: %s", strftime_buf);
        if (timeinfo.tm_year < (2022-1900)){
             ESP_LOGI(TAG, "Error in datetime: %s", strftime_buf);
             sntp_stop();
             vTaskDelay(10000 / portTICK_PERIOD_MS);
             sntp_init();
             vTaskDelay(5000 / portTICK_PERIOD_MS);
         }
         //better get prices day by day to consume less heap memory in querys.
         memory_info("Before get prices ");
         int retval = get_prices(0,0,3);
         memory_info("After get prices ");
         vTaskDelay(10000 / portTICK_PERIOD_MS);
         memory_info("Before get prices");
         retval = get_prices(0,4,7);
         memory_info("After get prices");
         vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    vTaskDelete( NULL );



}

void start_watio_app(){
    memory_info(TAG);
    xTaskCreate(&watio_app_task, "watio_app_task", 8192, NULL, 4, NULL);


}