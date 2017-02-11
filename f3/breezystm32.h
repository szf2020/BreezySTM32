#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <platform.h>

#include "parameter_group.h"
#include "parameter_group_ids.h"

#include "drivers/system.h"
#include "drivers/dma.h"
#include "drivers/gpio.h"
#include "drivers/timer.h"
#include "drivers/pwm_mapping.h"
#include "drivers/pwm_output.h"
#include "drivers/pwm_rx.h"
#include "drivers/io.h"
#include "drivers/serial.h"
#include "drivers/serial_usb_vcp.h"
#include "drivers/serial_uart.h"
#include "drivers/exti.h"
#include "drivers/bus_i2c.h"

#include "io/motors.h"

#include "config_system.h"
#include "profile.h"

void debug(const char * fmt, ...);

#define printf ERROR
