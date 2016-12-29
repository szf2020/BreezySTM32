/*
   mpu6500_poll.c : Read from MPU6500 IMU

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

#include <breezystm32.h>
#include <drivers/mpu.h>

void setup(void)
{
    i2cInit(I2CDEV_2); 

    mpu6500_init(INV_FSR_8G, INV_FSR_2000DPS);
} 

void loop(void)
{
    static int count;
    debug("%d\n", count++);
}
