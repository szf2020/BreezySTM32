/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>

#include <platform.h>


#include "drv_gpio.h"

#define MODE_OFFSET 0
#define PUPD_OFFSET 2
#define OUTPUT_OFFSET 4

#define MODE_MASK ((1|2) << MODE_OFFSET)
#define PUPD_MASK ((1|2) << PUPD_OFFSET)
#define OUTPUT_MASK ((1|2) << OUTPUT_OFFSET)

//#define GPIO_Speed_10MHz GPIO_Speed_Level_1   Fast Speed:10MHz
//#define GPIO_Speed_2MHz  GPIO_Speed_Level_2   Medium Speed:2MHz
//#define GPIO_Speed_50MHz GPIO_Speed_Level_3   High Speed:50MHz

void gpioInit(GPIO_TypeDef *gpio, gpio_config_t *config)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    uint32_t pinIndex;
    for (pinIndex = 0; pinIndex < 16; pinIndex++) {
        // are we doing this pin?
        uint32_t pinMask = (0x1 << pinIndex);
        if (config->pin & pinMask) {

            GPIO_InitStructure.GPIO_Pin =  pinMask;
            GPIO_InitStructure.GPIO_Mode = (config->mode & MODE_MASK) >> MODE_OFFSET;

            GPIOSpeed_TypeDef speed = GPIO_Speed_10MHz;
            switch (config->speed) {
                case Speed_10MHz:
                    speed = GPIO_Speed_Level_1;
                    break;
                case Speed_2MHz:
                    speed = GPIO_Speed_Level_2;
                    break;
                case Speed_50MHz:
                    speed = GPIO_Speed_Level_3;
                    break;
            }

            GPIO_InitStructure.GPIO_Speed = speed;
            GPIO_InitStructure.GPIO_OType = (config->mode & OUTPUT_MASK) >> OUTPUT_OFFSET;
            GPIO_InitStructure.GPIO_PuPd = (config->mode & PUPD_MASK) >> PUPD_OFFSET;
            GPIO_Init(gpio, &GPIO_InitStructure);
        }
    }
}

void enableGPIOPowerUsageAndNoiseReductions(void)
{
    RCC_AHBPeriphClockCmd(
        RCC_AHBPeriph_GPIOA |
        RCC_AHBPeriph_GPIOB |
        RCC_AHBPeriph_GPIOC |
        RCC_AHBPeriph_GPIOD |
        RCC_AHBPeriph_GPIOE |
        RCC_AHBPeriph_GPIOF,
        ENABLE
    );

    gpio_config_t gpio;

    gpio.mode = Mode_AIN;

    gpio.pin = Pin_All & ~(Pin_13 | Pin_14 | Pin_15);  // Leave JTAG pins alone
    gpioInit(GPIOA, &gpio);

    gpio.pin = Pin_All;
    gpioInit(GPIOB, &gpio);
    gpioInit(GPIOC, &gpio);
    gpioInit(GPIOD, &gpio);
    gpioInit(GPIOE, &gpio);
    gpioInit(GPIOF, &gpio);
}


