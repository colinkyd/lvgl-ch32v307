/* wiring_private.h shim for WCH CH32V30x core.
 * Adafruit_ST77xx.cpp does #include "wiring_private.h" (an STM32/AVR core header)
 * but on the WCH core it uses none of its symbols — it only needs the standard
 * Arduino API. Forward to Arduino.h so the include resolves.
 * Placed in the sketch dir, which arduino-cli adds to the include path.
 */
#ifndef WIRING_PRIVATE_SHIM_H
#define WIRING_PRIVATE_SHIM_H
#include <Arduino.h>
#endif
