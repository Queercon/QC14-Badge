/*
 * qc14.h
 *
 *  Created on: Jun 7, 2017
 *      Author: glouthan
 */

#ifndef STARTUP_QC14_H_
#define STARTUP_QC14_H_

#include "board.h"

#define LED_MP_RATE_MIN 16777215
#define LED_MP_RATE_BEST 48000

// TLC LED DRIVER CONFIGURATION
#define LED_MP_RATE LED_MP_RATE_BEST
#define LED_BRIGHTNESS_INTERVAL 12500

// Switch signals
#define SW_SIGNAL_NONE 0
#define SW_SIGNAL_OPEN 0b1000
#define SW_SIGNAL_L 0b01
#define SW_SIGNAL_R 0b10
#define SW_SIGNAL_DIR_MASK 0x11
#define SW_SIGNAL_C 0b100

// UI constant configuration
#define UI_CLOCK_MS 10
#define UI_SEL_TIMEOUT_SEC 40
#define UI_MAIN_TIMEOUT_SEC 600

// UART Protocol states:
extern uint8_t uart_proto_state[4];

#endif /* STARTUP_QC14_H_ */
