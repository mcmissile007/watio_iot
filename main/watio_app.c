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



static void get_ree_request(const char * url,char * request){
    strcpy(request,"GET ");
    strncat(request,url,strlen(url));
    strcat(request," HTTP/1.0\r\n");
    strcat(request,"Host: apidatos.ree.es\r\n");
    strcat(request, "User-Agent: esp-idf/1.0 esp32\r\n");
    strcat(request,"\r\n");
    
}


static int  get_ree_url(char * url){
    time_t now;
    time_t tomorrow;
    struct tm start;
    struct tm end;
    char strftime_buf[32];
    char start_buf[32];
    char end_buf[32];
    time(&now);
    localtime_r(&now, &start);
    if (start.tm_year < (2022-1900)){
        ESP_LOGE(TAG,"Error in time no url query");
        return ESP_FAIL;

    }
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H:%M", &start);
    ESP_LOGI(TAG, "The current date/time  is: %s", strftime_buf);
    tomorrow = now + 86400;
    localtime_r(&tomorrow, &end);
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H:%M", &end);
    ESP_LOGI(TAG, "Tomorrow  is: %s", strftime_buf);
    end.tm_hour = 23;
    end.tm_min = 59;
    start.tm_hour = 0;
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

static int get_prices()
{
    char ree_url[180];
    char ree_request[256];
    cJSON *json_response = NULL;

    if (get_ree_url(ree_url) == ESP_FAIL){
        return ESP_FAIL;
    }
    get_ree_request(ree_url,ree_request);
    json_response = http_request(ree_url,ree_request);
    if (json_response == NULL){
        goto exit;
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
         memory_info("Before get prices");
         int retval = get_prices();
         memory_info("after get prices");
         vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    vTaskDelete( NULL );



}

void start_watio_app(){
    memory_info(TAG);
    xTaskCreate(&watio_app_task, "watio_app_task", 8192, NULL, 4, NULL);


}