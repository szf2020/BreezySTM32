#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <platform.h>

#include "build/build_config.h"
#include "build/debug.h"
#include "build/atomic.h"

#include "common/axis.h"
#include "common/color.h"
#include "common/maths.h"
#include "common/printf.h"
#include "common/streambuf.h"
#include "common/filter.h"
#include "common/time.h"

#include "config/parameter_group.h"
#include "config/parameter_group_ids.h"

#include "drivers/nvic.h"

#include "drivers/sensor.h"
#include "drivers/system.h"
#include "drivers/dma.h"
#include "drivers/gpio.h"
#include "drivers/light_led.h"
#include "drivers/sound_beeper.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "drivers/serial_softserial.h"
#include "drivers/serial_uart.h"
#include "drivers/accgyro.h"
#include "drivers/compass.h"
#include "drivers/pwm_mapping.h"
#include "drivers/pwm_rx.h"
#include "drivers/adc.h"
#include "drivers/bus_i2c.h"
#include "drivers/bus_spi.h"
#include "drivers/inverter.h"
#include "drivers/flash_m25p16.h"
#include "drivers/sonar_hcsr04.h"
#include "drivers/sdcard.h"
#include "drivers/usb_io.h"
#include "drivers/transponder_ir.h"
#include "drivers/gyro_sync.h"
#include "drivers/exti.h"
#include "drivers/io.h"
#include "drivers/video.h"
#include "drivers/video_textscreen.h"

#include "rx/rx.h"
#include "rx/spektrum.h"

#include "fc/rc_controls.h"
#include "fc/fc_serial.h"
#include "fc/fc_debug.h"

#include "io/serial.h"
#include "io/flashfs.h"
#include "io/gps.h"
#include "io/motors.h"
#include "io/servos.h"
#include "io/gimbal.h"
#include "io/ledstrip.h"
#include "io/display.h"
#include "io/asyncfatfs/asyncfatfs.h"
#include "io/transponder_ir.h"
#include "io/vtx.h"

#include "fc/msp_server_fc.h"
#include "msp/msp.h"
#include "msp/msp_serial.h"
#include "io/serial_cli.h"

#include "sensors/sensors.h"
#include "sensors/sonar.h"
#include "sensors/barometer.h"
#include "sensors/compass.h"
#include "sensors/acceleration.h"
#include "sensors/gyro.h"
#include "sensors/voltage.h"
#include "sensors/amperage.h"
#include "sensors/battery.h"
#include "sensors/boardalignment.h"
#include "sensors/initialisation.h"

#include "telemetry/telemetry.h"
#include "blackbox/blackbox.h"

#include "flight/pid.h"
#include "flight/imu.h"
#include "flight/mixer.h"
#include "flight/servos.h"
#include "flight/failsafe.h"
#include "flight/navigation.h"

#include "osd/osd_element.h"
#include "osd/osd.h"

#include "fc/runtime_config.h"
#include "fc/config.h"
#include "config/config_system.h"
#include "config/feature.h"

#include "fc/fc_tasks.h"
#include "scheduler/scheduler.h"

extern uint8_t motorControlEnable;


bool isUsingVTXSwitch(void);
void mixerUsePWMIOConfiguration(pwmIOConfiguration_t *pwmIOConfiguration);
void rxInit(modeActivationCondition_t *modeActivationConditions);

void navigationInit(pidProfile_t *pidProfile);
const sonarHardware_t *sonarGetHardwareConfiguration(amperageMeter_e amperageMeter);
void sonarInit(const sonarHardware_t *sonarHardware);


// from system_stm32f30x.c
void SetSysClock(void);

PG_REGISTER_WITH_RESET_TEMPLATE(systemConfig_t, systemConfig, PG_SYSTEM_CONFIG, 0);
PG_REGISTER(pwmRxConfig_t, pwmRxConfig, PG_DRIVER_PWM_RX_CONFIG, 0);

PG_RESET_TEMPLATE(systemConfig_t, systemConfig,
    .i2c_highspeed = 1,
);

typedef enum {
    SYSTEM_STATE_INITIALISING        = 0,
    SYSTEM_STATE_CONFIG_LOADED       = (1 << 0),
    SYSTEM_STATE_SENSORS_READY       = (1 << 1),
    SYSTEM_STATE_MOTORS_READY        = (1 << 2),
    SYSTEM_STATE_TRANSPONDER_ENABLED = (1 << 3),
    SYSTEM_STATE_READY               = (1 << 7)
} systemState_e;

static uint8_t systemState = SYSTEM_STATE_INITIALISING;

void flashLedsAndBeep(void)
{
    LED1_ON;
    LED0_OFF;
    for (uint8_t i = 0; i < 10; i++) {
        LED1_TOGGLE;
        LED0_TOGGLE;
        delay(25);
        BEEP_ON;
        delay(25);
        BEEP_OFF;
    }
    LED0_OFF;
    LED1_OFF;
}

void init(void)
{
    drv_pwm_config_t pwm_params;


    initEEPROM();

    ensureEEPROMContainsValidData();
    readEEPROM();

    systemState |= SYSTEM_STATE_CONFIG_LOADED;


    // start fpu
    SCB->CPACR = (0x3 << (10*2)) | (0x3 << (11*2));


    SetSysClock();

    i2cSetOverclock(systemConfig()->i2c_highspeed);

    systemInit();

    // Latch active features to be used for feature() in the remainder of init().
    latchActiveFeatures();

    // initialize IO (needed for all IO operations)
    IOInitGlobal();

    debugMode = debugConfig()->debug_mode;

    EXTIInit();


    ledInit(false);

    delay(100);

    timerInit();  // timer must be initialized before any channel is allocated

    dmaInit();

    serialInit(feature(FEATURE_SOFTSERIAL));

    if (!feature(FEATURE_ONESHOT125))
        motorControlEnable = true;

    systemState |= SYSTEM_STATE_MOTORS_READY;


    i2cInit(I2C_DEVICE);

#ifdef USE_ADC
    drv_adc_config_t adc_params;

    adc_params.channelMask = 0;

#ifdef ADC_BATTERY
    adc_params.channelMask = (feature(FEATURE_VBAT) ? ADC_CHANNEL_MASK(ADC_BATTERY) : 0);
#endif
#ifdef ADC_RSSI
    adc_params.channelMask |= (feature(FEATURE_RSSI_ADC) ? ADC_CHANNEL_MASK(ADC_RSSI) : 0);
#endif
#ifdef ADC_AMPERAGE
    adc_params.channelMask |=  (feature(FEATURE_AMPERAGE_METER) ? ADC_CHANNEL_MASK(ADC_AMPERAGE) : 0);
#endif

#ifdef ADC_POWER_12V
    adc_params.channelMask |= ADC_CHANNEL_MASK(ADC_POWER_12V);
#endif
#ifdef ADC_POWER_5V
    adc_params.channelMask |= ADC_CHANNEL_MASK(ADC_POWER_5V);
#endif
#ifdef ADC_POWER_3V
    adc_params.channelMask |= ADC_CHANNEL_MASK(ADC_POWER_3V);
#endif

#ifdef NAZE
    // optional ADC5 input on rev.5 hardware
    adc_params.channelMask |= (hardwareRevision >= NAZE32_REV5) ? ADC_CHANNEL_MASK(ADC_EXTERNAL) : 0;
#endif

    adcInit(&adc_params);
#endif

    initBoardAlignment();

#ifdef DISPLAY
    if (feature(FEATURE_DISPLAY)) {
        displayInit();
    }
#endif

   if (!sensorsAutodetect()) {
        // if gyro was not detected due to whatever reason, we give up now.
        failureMode(FAILURE_MISSING_ACC);
    }

    systemState |= SYSTEM_STATE_SENSORS_READY;

    flashLedsAndBeep();

    mspInit();
    mspSerialInit();

    const uint16_t pidPeriodUs = US_FROM_HZ(gyro.sampleFrequencyHz);
    pidSetTargetLooptime(pidPeriodUs * gyroConfig()->pid_process_denom);
    pidInitFilters(pidProfile());

    imuInit();

   failsafeInit();

    rxInit(modeActivationProfile()->modeActivationConditions);

#ifdef TRANSPONDER
    if (feature(FEATURE_TRANSPONDER)) {
        transponderInit(transponderConfig()->data);
        transponderEnable();
        transponderStartRepeating();
        systemState |= SYSTEM_STATE_TRANSPONDER_ENABLED;
    }
#endif

#ifdef USE_FLASHFS
#ifdef NAZE
    if (hardwareRevision == NAZE32_REV5) {
        m25p16_init();
    }
#elif defined(USE_FLASH_M25P16)
    m25p16_init();
#endif

    flashfsInit();
#endif

#ifdef USE_SDCARD
    bool sdcardUseDMA = false;

    sdcardInsertionDetectInit();

#ifdef SDCARD_DMA_CHANNEL_TX

#if defined(LED_STRIP) && defined(WS2811_DMA_CHANNEL)
    // Ensure the SPI Tx DMA doesn't overlap with the led strip
    sdcardUseDMA = !feature(FEATURE_LED_STRIP) || SDCARD_DMA_CHANNEL_TX != WS2811_DMA_CHANNEL;
#else
    sdcardUseDMA = true;
#endif

#endif

    sdcard_init(sdcardUseDMA);

    afatfs_init();
#endif

#ifdef BLACKBOX
    initBlackbox();
#endif

    if (mixerConfig()->mixerMode == MIXER_GIMBAL) {
        accSetCalibrationCycles(CALIBRATING_ACC_CYCLES);
    }
    gyroSetCalibrationCycles(CALIBRATING_GYRO_CYCLES);
#ifdef BARO
    baroSetCalibrationCycles(CALIBRATING_BARO_CYCLES);
#endif

#ifdef OSD
    if (feature(FEATURE_OSD)) {
        osdInit();
    }
#endif

    // start all timers
    // TODO - not implemented yet
    timerStart();

    ENABLE_STATE(SMALL_ANGLE);
    DISABLE_ARMING_FLAG(PREVENT_ARMING);

#ifdef SOFTSERIAL_LOOPBACK
    // FIXME this is a hack, perhaps add a FUNCTION_LOOPBACK to support it properly
    loopbackPort = (serialPort_t*)&(softSerialPorts[0]);
    if (!loopbackPort->vTable) {
        loopbackPort = openSoftSerial(0, NULL, 19200, SERIAL_NOT_INVERTED);
    }
    serialPrint(loopbackPort, "LOOPBACK\r\n");
#endif


    if (feature(FEATURE_VBAT)) {
        // Now that everything has powered up the voltage and cell count be determined.

        voltageMeterInit();
        batteryInit();
    }

    if (feature(FEATURE_AMPERAGE_METER)) {
        amperageMeterInit();
    }

#ifdef DISPLAY
    if (feature(FEATURE_DISPLAY)) {
#ifdef USE_OLED_GPS_DEBUG_PAGE_ONLY
        displayShowFixedPage(PAGE_GPS);
#else
        displayResetPageCycling();
        displayEnablePageCycling();
#endif
    }
#endif

#ifdef CJMCU
    LED2_ON;
#endif

    // Latch active features AGAIN since some may be modified by init().
    latchActiveFeatures();
    motorControlEnable = true;

    systemState |= SYSTEM_STATE_READY;
}

#ifdef SOFTSERIAL_LOOPBACK
void processLoopback(void) {
    if (loopbackPort) {
        uint8_t bytesWaiting;
        while ((bytesWaiting = serialRxBytesWaiting(loopbackPort))) {
            uint8_t b = serialRead(loopbackPort);
            serialWrite(loopbackPort, b);
        };
    }
}
#else
#define processLoopback()
#endif

void configureScheduler(void)
{
    schedulerInit();
    setTaskEnabled(TASK_SYSTEM, true);

    uint16_t gyroPeriodUs = US_FROM_HZ(gyro.sampleFrequencyHz);
    rescheduleTask(TASK_GYRO, gyroPeriodUs);
    setTaskEnabled(TASK_GYRO, true);

    rescheduleTask(TASK_PID, gyroPeriodUs);
    setTaskEnabled(TASK_PID, true);

    if (sensors(SENSOR_ACC)) {
        setTaskEnabled(TASK_ACCEL, true);
        rescheduleTask(TASK_ACCEL, accSamplingInterval);
    }

    setTaskEnabled(TASK_ATTITUDE, sensors(SENSOR_ACC));
    setTaskEnabled(TASK_SERIAL, true);
    setTaskEnabled(TASK_HARDWARE_WATCHDOG, true);
#ifdef BEEPER
    setTaskEnabled(TASK_BEEPER, true);
#endif
    setTaskEnabled(TASK_BATTERY, feature(FEATURE_VBAT) || feature(FEATURE_AMPERAGE_METER));
    setTaskEnabled(TASK_RX, true);
#ifdef GPS
    setTaskEnabled(TASK_GPS, feature(FEATURE_GPS));
#endif
#ifdef MAG
    setTaskEnabled(TASK_COMPASS, sensors(SENSOR_MAG));
#if defined(MPU6500_SPI_INSTANCE) && defined(USE_MAG_AK8963)
    // fixme temporary solution for AK6983 via slave I2C on MPU9250
    rescheduleTask(TASK_COMPASS, 1000000 / 40);
#endif
#endif
#ifdef BARO
    setTaskEnabled(TASK_BARO, sensors(SENSOR_BARO));
#endif
#ifdef SONAR
    setTaskEnabled(TASK_SONAR, sensors(SENSOR_SONAR));
#endif
#if defined(BARO) || defined(SONAR)
    setTaskEnabled(TASK_ALTITUDE, sensors(SENSOR_BARO) || sensors(SENSOR_SONAR));
#endif
#ifdef DISPLAY
    setTaskEnabled(TASK_DISPLAY, feature(FEATURE_DISPLAY));
#endif
#ifdef TELEMETRY
    setTaskEnabled(TASK_TELEMETRY, feature(FEATURE_TELEMETRY));
#endif
#ifdef LED_STRIP
    setTaskEnabled(TASK_LEDSTRIP, feature(FEATURE_LED_STRIP));
#endif
#ifdef TRANSPONDER
    setTaskEnabled(TASK_TRANSPONDER, feature(FEATURE_TRANSPONDER));
#endif
#ifdef OSD
    setTaskEnabled(TASK_DRAW_SCREEN, feature(FEATURE_OSD));
#endif
}

int main(void) {

    init();

    serialPort_t * uart1 = (serialPort_t *)uartOpen(USART1, NULL, 115200, MODE_RXTX, SERIAL_NOT_INVERTED);

    while (true) {
        char tmp[100];
        sprintf(tmp, "%ld\n", millis());
        for (char *p=tmp; *p; p++) {
            serialWrite(uart1, *p);
        }
        delay(10);
    }
}

void HardFault_Handler(void)
{
    // fall out of the sky
    uint8_t requiredStateForMotors = SYSTEM_STATE_CONFIG_LOADED | SYSTEM_STATE_MOTORS_READY;
    if ((systemState & requiredStateForMotors) == requiredStateForMotors) {
        stopMotors();
    }
#ifdef TRANSPONDER
    // prevent IR LEDs from burning out.
    uint8_t requiredStateForTransponder = SYSTEM_STATE_CONFIG_LOADED | SYSTEM_STATE_TRANSPONDER_ENABLED;
    if ((systemState & requiredStateForTransponder) == requiredStateForTransponder) {
        transponderIrDisable();
    }
#endif

    while (1);
}