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
#include <stdlib.h>
#include <string.h>

#include <platform.h>


#include "nvic.h"

#include "system.h"
#include "gpio.h"
#include "drivers/io.h"
#include "drivers/exti.h"
#include "bus_i2c.h"
#include "gyro_sync.h"

#include "sensor.h"
#include "accgyro.h"
#include "accgyro_mpu6050.h"
#include "accgyro_mpu.h"

//#define DEBUG_MPU_DATA_READY_INTERRUPT

static bool mpuReadRegisterI2C(uint8_t reg, uint8_t length, uint8_t* data);
static bool mpuWriteRegisterI2C(uint8_t reg, uint8_t data);

static void mpu6050FindRevision(void);

mpuDetectionResult_t mpuDetectionResult;


mpuConfiguration_t mpuConfiguration;
static const extiConfig_t *mpuIntExtiConfig = NULL;

// interrupt is triggered when internal gyro registers are updated at 8KHz, regardless of the desired sample frequency.
uint8_t mpuIntDenominator;

#define MPU_ADDRESS             0x68

mpuDetectionResult_t *detectMpu(const extiConfig_t *configToUse)
{
    memset(&mpuDetectionResult, 0, sizeof(mpuDetectionResult));
    memset(&mpuConfiguration, 0, sizeof(mpuConfiguration));

    mpuIntExtiConfig = configToUse;

    bool ack;
    uint8_t sig;

    // MPU datasheet specifies 30ms.
    delay(35);

    ack = mpuReadRegisterI2C(MPU_RA_WHO_AM_I, 1, &sig);
    if (ack) {
        mpuConfiguration.read = mpuReadRegisterI2C;
        mpuConfiguration.write = mpuWriteRegisterI2C;
    } else {

        return &mpuDetectionResult;
    }

    mpuConfiguration.gyroReadXRegister = MPU_RA_GYRO_XOUT_H;

    sig &= MPU_INQUIRY_MASK;

    if (sig == MPUx0x0_WHO_AM_I_CONST) {

        mpuDetectionResult.sensor = MPU_60x0;

        mpu6050FindRevision();
    } else if (sig == MPU6500_WHO_AM_I_CONST) {
        mpuDetectionResult.sensor = MPU_65xx_I2C;
    }

    return &mpuDetectionResult;
}

static void mpu6050FindRevision(void)
{
    bool ack;
    UNUSED(ack);

    uint8_t readBuffer[6];
    uint8_t revision;
    uint8_t productId;

    // There is a map of revision contained in the android source tree which is quite comprehensive and may help to understand this code
    // See https://android.googlesource.com/kernel/msm.git/+/eaf36994a3992b8f918c18e4f7411e8b2320a35f/drivers/misc/mpu6050/mldl_cfg.c

    // determine product ID and accel revision
    ack = mpuConfiguration.read(MPU_RA_XA_OFFS_H, 6, readBuffer);
    revision = ((readBuffer[5] & 0x01) << 2) | ((readBuffer[3] & 0x01) << 1) | (readBuffer[1] & 0x01);
    if (revision) {
        /* Congrats, these parts are better. */
        if (revision == 1) {
            mpuDetectionResult.resolution = MPU_HALF_RESOLUTION;
        } else if (revision == 2) {
            mpuDetectionResult.resolution = MPU_FULL_RESOLUTION;
        } else if ((revision == 3) || (revision == 7)) {
            mpuDetectionResult.resolution = MPU_FULL_RESOLUTION;
        } else {
            failureMode(FAILURE_ACC_INCOMPATIBLE);
        }
    } else {
        ack = mpuConfiguration.read(MPU_RA_PRODUCT_ID, 1, &productId);
        revision = productId & 0x0F;
        if (!revision) {
            failureMode(FAILURE_ACC_INCOMPATIBLE);
        } else if (revision == 4) {
            mpuDetectionResult.resolution = MPU_HALF_RESOLUTION;
        } else {
            mpuDetectionResult.resolution = MPU_FULL_RESOLUTION;
        }
    }
}

extiCallbackRec_t mpuIntCallbackRec;

void mpuIntExtiHandler(extiCallbackRec_t *cb)
{
    UNUSED(cb);

    static uint8_t counter = 0;

    if (++counter < mpuIntDenominator) {
        return;
    }

    counter = 0;
    gyroSyncIntHandler();
}

void configureMPUDataReadyInterruptHandling(void)
{
#ifdef USE_MPU_DATA_READY_SIGNAL

    IO_t mpuIntIO = IOGetByTag(mpuIntExtiConfig->io);

#ifdef ENSURE_MPU_DATA_READY_IS_LOW
    uint8_t status = GPIO_ReadInputDataBit(mpuIntExtiConfig->gpioPort, mpuIntExtiConfig->gpioPin);
    if (status) {
        return;
    }
#endif
    EXTIHandlerInit(&mpuIntCallbackRec, mpuIntExtiHandler);
    EXTIConfig(mpuIntIO, &mpuIntCallbackRec, NVIC_PRIO_MPU_INT_EXTI, EXTI_Trigger_Rising);
    EXTIEnable(mpuIntIO, true);
#endif
}

void mpuIntExtiInit(void)
{
    gpio_config_t gpio;

    static bool mpuExtiInitDone = false;

    if (mpuExtiInitDone || !mpuIntExtiConfig) {
        return;
    }

#ifdef STM32F303
        if (mpuIntExtiConfig->gpioAHBPeripherals) {
            RCC_AHBPeriphClockCmd(mpuIntExtiConfig->gpioAHBPeripherals, ENABLE);
        }
#endif
#ifdef STM32F10X
        if (mpuIntExtiConfig->gpioAPB2Peripherals) {
            RCC_APB2PeriphClockCmd(mpuIntExtiConfig->gpioAPB2Peripherals, ENABLE);
        }
#endif

    gpio.pin = mpuIntExtiConfig->gpioPin;
    gpio.speed = Speed_2MHz;
    gpio.mode = Mode_IN_FLOATING;
    gpioInit(mpuIntExtiConfig->gpioPort, &gpio);

    configureMPUDataReadyInterruptHandling();

    mpuExtiInitDone = true;
}

static bool mpuReadRegisterI2C(uint8_t reg, uint8_t length, uint8_t* data)
{
    bool ack = i2cRead(MPU_ADDRESS, reg, length, data);
    return ack;
}

static bool mpuWriteRegisterI2C(uint8_t reg, uint8_t data)
{
    bool ack = i2cWrite(MPU_ADDRESS, reg, data);
    return ack;
}

bool mpuAccRead(int16_t *accData)
{
    uint8_t data[6];

    bool ack = mpuConfiguration.read(MPU_RA_ACCEL_XOUT_H, 6, data);
    if (!ack) {
        return false;
    }

    accData[0] = (int16_t)((data[0] << 8) | data[1]);
    accData[1] = (int16_t)((data[2] << 8) | data[3]);
    accData[2] = (int16_t)((data[4] << 8) | data[5]);

    return true;
}

bool mpuGyroRead(int16_t *gyroADC)
{
    uint8_t data[6];

    bool ack = mpuConfiguration.read(mpuConfiguration.gyroReadXRegister, 6, data);
    if (!ack) {
        return false;
    }

    gyroADC[0] = (int16_t)((data[0] << 8) | data[1]);
    gyroADC[1] = (int16_t)((data[2] << 8) | data[3]);
    gyroADC[2] = (int16_t)((data[4] << 8) | data[5]);

    return true;
}
