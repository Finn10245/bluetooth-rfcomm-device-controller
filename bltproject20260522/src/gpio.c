#include "gpio.h"
#include <stdio.h>
#include <unistd.h>
#include <gpiod.h>

#define GPIO_CHIP "/dev/gpiochip0"
#define GPIO_LINE 17

static struct gpiod_chip *chip = NULL;
static struct gpiod_line_request *request = NULL;

int gpio_init(void)
{
    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("GPIO: 無法開啟 chip");
        return -1;
    }

    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    struct gpiod_line_config *cfg = gpiod_line_config_new();
    unsigned int offsets[] = { GPIO_LINE };
    gpiod_line_config_add_line_settings(cfg, offsets, 1, settings);

    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "blt_hsm");

    request = gpiod_chip_request_lines(chip, req_cfg, cfg);
    if (!request) {
        perror("GPIO: 無法 request line");
        return -1;
    }

    gpiod_line_settings_free(settings);
    gpiod_line_config_free(cfg);
    gpiod_request_config_free(req_cfg);

    printf("GPIO 初始化完成，預設為 LOW\n");
    return 0;
}

void gpio_set(int value)
{
    if (!request) return;

    gpiod_line_request_set_value(request, GPIO_LINE, value);
    printf("GPIO17 設為 %d\n", value);
}

void gpio_close(void)
{
    if (request) {
        gpiod_line_request_release(request);
        request = NULL;
    }
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
    printf("GPIO 已釋放\n");
}

