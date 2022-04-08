#ifndef IO_H
#define IO_H
typedef enum
{
    LED_OFF = 0,
    LED_ON,
    LED_SLOW_BLINK,
    LED_FAST_BLINK

} led_status_t;
void setup_gpio();
void turn_on_led();
void turn_off_led();
void blink(int t, int n);
void update_led_status(led_status_t led_status);
#endif