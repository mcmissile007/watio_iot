
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "info.h"
#include "config.h"
#include "network.h"

#define TAG "main.c"


void common_init(){
    //use for many components.
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
}


void app_main()
{

    common_init();
    system_info();
    memory_info(TAG);
    if (is_wifi_configured()){

    }
    else{
        start_softApWebServer();

    }
}
