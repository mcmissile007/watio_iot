
// sdk includes
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// app includes
#include "io.h"
#include "config.h"

// for GPIO2 in NodeMCU seems to work backwards
#define LOW 1
#define HIGH 0
#define LED GPIO_NUM_2
#define LED_Pin GPIO_Pin_2

void setup_gpio()
{

    gpio_config_t io_conf;
    // disable interrupt
    // PIN 5 LED OUTPUT
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    // bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = LED_Pin;
    // disable pull-down mode
    io_conf.pull_down_en = 0;
    // disable pull-up mode
    io_conf.pull_up_en = 0;
    // configure GPIO with the given settings
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // PIN 16 BUTTON INPUT
    // disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    // bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = GPIO_Pin_16;
    // enablde pull-down mode
    io_conf.pull_down_en = 1;
    // disable pull-up mode
    io_conf.pull_up_en = 0;
    // configure GPIO with the given settings
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("GPIO config end.\n");
}

void turn_on_led()
{
    gpio_set_level(LED, HIGH);
}
void turn_off_led()
{
    gpio_set_level(LED, LOW);
}

void blink(int t, int n)
{
    for (int i = 0; i < n; i++)
    {
        turn_on_led();
        vTaskDelay(t / portTICK_PERIOD_MS);
        turn_off_led();
        vTaskDelay(t / portTICK_PERIOD_MS);
    }
}

void slow_blink()
{
    blink(1000, 5);
}
void fast_blink()
{
    blink(200, 10);
}
void update_led_status(led_status_t led_status)
{
    switch (led_status)
    {
    case 0:
        turn_off_led();
        break;
    case 1:
        turn_on_led();
        break;
    case 2:
        slow_blink();
        break;
    case 3:
        fast_blink();
        break;

    default:
        break;
    }
}
