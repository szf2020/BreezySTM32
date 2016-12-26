/*
   blink.c : blink the LED

   Copyright (C) 2016 Simon D. Levy 

   This file is part of BreezySTM32.

   BreezySTM32 is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   BreezySTM32 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with BreezySTM32.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "platform.h"
#include "system.h"
#include "gpio.h"
#include "io.h"
#include "serial.h"
#include "timer.h"
#include "serial_usb_vcp.h"
#include "exti.h"
#include "ioserial.h"
#include "system.h"
#include "light_led.h"

void setup(void)
{
} 

void loop(void)
{
    LED0_TOGGLE;
    delay(500);
}