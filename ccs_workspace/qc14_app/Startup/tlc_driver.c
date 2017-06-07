#include <ti/drivers/timer/GPTimerCC26XX.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/spi/SPICC26XXDMA.h>
#include <ti/drivers/dma/UDMACC26XX.h>
#include <ti/drivers/PWM.h>
#include <ti/drivers/pwm/PWMTimerCC26XX.h>

#include "qc14.h"
#include "board.h"

// General convention:
//  Things that deal with the LED system driver as a whole: led_
//  Things that deal with the multiplexing only: mp_
//  Things that deal with the LED driver IC only: tlc_

#define TLC_THISISGS    0x00
#define TLC_THISISFUN   0x01

// LED systemwide declarations:
static PIN_State led_pin_state;
PIN_Handle led_pin_h;
PIN_Config led_pin_table[] = {
    // LED controller:
    LED_LE         | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    // LED Multiplexer (bit-banged)
    MP_CTR_CLK      | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    MP0_OUT         | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    MP0_CLR         | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    MP1_CLR         | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    MP1_OUT         | PIN_INPUT_EN | PIN_NOPULL,
    PIN_TERMINATE
};

// The screen buffer:
uint8_t led_buf[11][7][3] = {
    {{0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}},
    {{0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}},
    {{0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}, {0xff, 0x00, 0x00}},
    {{0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}},
    {{0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x00}},
    {{0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}},
    {{0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}, {0x00, 0x00, 0xff}},
    {{0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0,0,0}},
    {{0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0,0,0}},
    {{0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0,0,0}},
    {{0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0xff, 0xff, 0xff}, {0,0,0}},
};

// Mapping of scan lines and channels onto row,column,color:
const uint8_t led_map[15][15][3] = {
    { {0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {1, 0, 0}, {1, 0, 1}, {1, 0, 2}, {2, 0, 0}, {2, 0, 1}, {2, 0, 2}, {3, 0, 0}, {3, 0, 1}, {3, 0, 2}, {4, 0, 0}, {4, 0, 1}, {4, 0, 2}},
    { {0, 1, 0}, {0, 1, 1}, {0, 1, 2}, {1, 1, 0}, {1, 1, 1}, {1, 1, 2}, {2, 1, 0}, {2, 1, 1}, {2, 1, 2}, {3, 1, 0}, {3, 1, 1}, {3, 1, 2}, {4, 1, 0}, {4, 1, 1}, {4, 1, 2}},
    { {0, 2, 0}, {0, 2, 1}, {0, 2, 2}, {1, 2, 0}, {1, 2, 1}, {1, 2, 2}, {2, 2, 0}, {2, 2, 1}, {2, 2, 2}, {3, 2, 0}, {3, 2, 1}, {3, 2, 2}, {4, 2, 0}, {4, 2, 1}, {4, 2, 2}},
    { {0, 3, 0}, {0, 3, 1}, {0, 3, 2}, {1, 3, 0}, {1, 3, 1}, {1, 3, 2}, {2, 3, 0}, {2, 3, 1}, {2, 3, 2}, {3, 3, 0}, {3, 3, 1}, {3, 3, 2}, {4, 3, 0}, {4, 3, 1}, {4, 3, 2}},
    { {0, 4, 0}, {0, 4, 1}, {0, 4, 2}, {1, 4, 0}, {1, 4, 1}, {1, 4, 2}, {2, 4, 0}, {2, 4, 1}, {2, 4, 2}, {3, 4, 0}, {3, 4, 1}, {3, 4, 2}, {4, 4, 0}, {4, 4, 1}, {4, 4, 2}},
    { {0, 5, 0}, {0, 5, 1}, {0, 5, 2}, {1, 5, 0}, {1, 5, 1}, {1, 5, 2}, {2, 5, 0}, {2, 5, 1}, {2, 5, 2}, {3, 5, 0}, {3, 5, 1}, {3, 5, 2}, {4, 5, 0}, {4, 5, 1}, {4, 5, 2}},
    { {0, 6, 0}, {0, 6, 1}, {0, 6, 2}, {1, 6, 0}, {1, 6, 1}, {1, 6, 2}, {2, 6, 0}, {2, 6, 1}, {2, 6, 2}, {3, 6, 0}, {3, 6, 1}, {3, 6, 2}, {4, 6, 0}, {4, 6, 1}, {4, 6, 2}},
    { {5, 1, 0}, {5, 1, 1}, {5, 1, 2}, {5, 2, 0}, {5, 2, 1}, {5, 2, 2}, {5, 3, 0}, {5, 3, 1}, {5, 3, 2}, {5, 4, 0}, {5, 4, 1}, {5, 4, 2}, {5, 5, 0}, {5, 5, 1}, {5, 5, 2}},
    { {6, 1, 0}, {6, 1, 1}, {6, 1, 2}, {6, 2, 0}, {6, 2, 1}, {6, 2, 2}, {6, 3, 0}, {6, 3, 1}, {6, 3, 2}, {6, 4, 0}, {6, 4, 1}, {6, 4, 2}, {6, 5, 0}, {6, 5, 1}, {6, 5, 2}},
    { {5, 0, 0}, {5, 0, 1}, {5, 0, 2}, {6, 0, 0}, {6, 0, 1}, {6, 0, 2}, {5, 6, 0}, {5, 6, 1}, {5, 6, 2}, {6, 6, 0}, {6, 6, 1}, {6, 6, 2}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    { {7, 1, 0}, {7, 1, 1}, {7, 1, 2}, {7, 2, 0}, {7, 2, 1}, {7, 2, 2}, {7, 3, 0}, {7, 3, 1}, {7, 3, 2}, {7, 4, 0}, {7, 4, 1}, {7, 4, 2}, {7, 5, 0}, {7, 5, 1}, {7, 5, 2}},
    { {10, 5, 0}, {10, 5, 1}, {10, 5, 2}, {10, 2, 0}, {10, 2, 1}, {10, 2, 2}, {10, 1, 0}, {10, 1, 1}, {10, 1, 2}, {10, 0, 0}, {10, 0, 1}, {10, 0, 2}, {10, 3, 0}, {10, 3, 1}, {10, 3, 2}},
    { {9, 5, 0}, {9, 5, 1}, {9, 5, 2}, {9, 2, 0}, {9, 2, 1}, {9, 2, 2}, {9, 1, 0}, {9, 1, 1}, {9, 1, 2}, {9, 4, 0}, {9, 4, 1}, {9, 4, 2}, {9, 3, 0}, {9, 3, 1}, {9, 3, 2}},
    { {8, 3, 0}, {8, 3, 1}, {8, 3, 2}, {8, 4, 0}, {8, 4, 1}, {8, 4, 2}, {8, 2, 0}, {8, 2, 1}, {8, 2, 2}, {8, 1, 0}, {8, 1, 1}, {8, 1, 2}, {8, 5, 0}, {8, 5, 1}, {8, 5, 2}},
    { {7, 0, 0}, {7, 0, 1}, {7, 0, 2}, {10, 4, 0}, {10, 4, 1}, {10, 4, 2}, {9, 0, 0}, {9, 0, 1}, {9, 0, 2}, {8, 0, 0}, {8, 0, 1}, {8, 0, 2}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
};

// Multiplex/scan line declarations:
volatile uint16_t mp_curr_scan_line = 0;

// TLC specific declarations:
GPTimerCC26XX_Handle tlc_gclk_timer_h;
PWM_Handle tlc_gclk_pwm_h;
SPI_Transaction tlc_gs_spi_transaction;
SPI_Handle tlc_spi;

uint8_t tlc_msg_fun_base[] = {
        TLC_THISISFUN, // This is a command.
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // ...reserved...
        // B135 / PSM(D1)       0
        // B134 / PSM(D0)       0
        // B133 / OLDENA        1
        // B132 / IDMCUR(D1)    0
        // B131 / IDMCUR(D0)    0
        // B130 / IDMRPT(D0)    0
        // B129 / IDMENA        0
        // B128 / LATTMG(D1)    1:
        0b00100001,
        // B127 / LATTMG(D0)    1
        // B126 / LSDVLT(D1)    0
        // B125 / LSDVLT(D0)    0
        // B124 / LODVLT(D1)    0
        // B123 / LODVLT(D0)    0
        // B122 / ESPWM         1
        // B121 / TMGRST        1
        // B120 / DSPRPT        1:
        0b10000111,
        // B119 / BLANK
        // and 7 bits of global brightness correction:
//        0xff, // blank on.
        0x00, // after this there are 16 7-tets of dot correction. // blank off
        //We'll send it as 14 octets.
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

uint8_t tlc_msg_all_on[] = {
        TLC_THISISGS, // This is a command.
        0x00, 0x00,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
};

uint8_t tlc_msg_gs_buf[] = {
        TLC_THISISGS, // This is a command.
        0x00, 0x00,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
        0xff, 0xff,
};

inline void mp_shift() {
    mp_curr_scan_line++;
    mp_curr_scan_line %= 15;
    PIN_setOutputValue(led_pin_h, MP0_OUT, mp_curr_scan_line != 0);
    PIN_setOutputValue(led_pin_h, MP_CTR_CLK, 1); // pulse clock
    PIN_setOutputValue(led_pin_h, MP_CTR_CLK, 0);
    if (mp_curr_scan_line == 7) {
        PIN_setOutputValue(led_pin_h, MP_CTR_CLK, 1); // pulse clock
        PIN_setOutputValue(led_pin_h, MP_CTR_CLK, 0);
    }
}

void mp_tick(GPTimerCC26XX_Handle handle, GPTimerCC26XX_IntMask interruptMask) {
    for (uint8_t i=0; i<15; i++) {
        tlc_msg_gs_buf[2 + i*2] = 0xff; // LSB
        tlc_msg_gs_buf[2 + i*2 + 1] = 0xff; // MSB
//        tlc_gs_buf[2 + i*2 + 1] = led_buf[tlc_led_map[scan_line][i][0]][led_map[scan_line][i][1]][led_map[scan_line][i][2]];
    }

    tlc_gs_spi_transaction.txBuf = tlc_msg_gs_buf;
    tlc_gs_spi_transaction.rxBuf = NULL;
    tlc_gs_spi_transaction.count = sizeof tlc_msg_all_on;
    SPI_transfer(tlc_spi, &tlc_gs_spi_transaction);
}

void tlc_spi_cb(SPI_Handle handle, SPI_Transaction *transaction) {
    // We only have the one SPI, so we can pretty well ignore these parameters.
    // TODO: We may want to check whether transaction.status == SPI_TRANSFER_COMPLETED???
    // LATCH!
    static uint16_t hwiKey;
    // Disable hardware interrupts.
    hwiKey  = (uint16_t) Hwi_disable();

    PIN_setOutputValue(led_pin_h, LED_LE, 1);
    if (((uint8_t *)transaction->txBuf)[0] == TLC_THISISGS) mp_shift();
    PIN_setOutputValue(led_pin_h, LED_LE, 0);

    Hwi_restore(hwiKey);
}

// Initialization functions:

void tlc_gclk_init() {
    // First, initiate the GCLK and kick it off. Max speed supported by the
    // TLC5948A is 32 MHz, and the fastest 50% duty cycle signal we can send
    // is 24 MHz, so let's do _that_.

    PWM_Params pwm_params;

    PWM_Params_init(&pwm_params);
    pwm_params.idleLevel = PWM_IDLE_LOW;
    pwm_params.periodUnits = PWM_PERIOD_HZ;
    pwm_params.periodValue = 24000000; // 24 MHz (half our fastest clock)
    pwm_params.dutyUnits = PWM_DUTY_FRACTION;
    pwm_params.dutyValue = PWM_DUTY_FRACTION_MAX/2; // 50% duty cycle

    // Open and start:
    tlc_gclk_pwm_h = PWM_open(QC14BOARD_PWM_GSCLK, &pwm_params);
    PWM_start(tlc_gclk_pwm_h);
}

void tlc_spi_init() {
    // Config up our SPI.
    SPI_Params tlc_spi_params;

    SPI_Params_init(&tlc_spi_params);
    tlc_spi_params.transferMode = SPI_MODE_CALLBACK;
    tlc_spi_params.mode = SPI_MASTER;
    tlc_spi_params.transferCallbackFxn = tlc_spi_cb;
    tlc_spi_params.bitRate = 12000000;
    tlc_spi = SPI_open(QC14BOARD_TLC_SPI, &tlc_spi_params);

    SPI_Transaction fun_data_transaction;
    fun_data_transaction.txBuf = tlc_msg_fun_base;
    fun_data_transaction.rxBuf = NULL;
    fun_data_transaction.count = sizeof tlc_msg_fun_base;

    SPI_transfer(tlc_spi, &fun_data_transaction);
    while (fun_data_transaction.status != SPI_TRANSFER_COMPLETED);
}

void tlc_mp_init() {
    // TODO: Is this clear necessary?
    // Flush out and clear the shift register:
    PIN_setOutputValue(led_pin_h, MP0_CLR, 0);
    PIN_setOutputValue(led_pin_h, MP1_CLR, 0);
    PIN_setOutputValue(led_pin_h, MP0_CLR, 1);
    PIN_setOutputValue(led_pin_h, MP1_CLR, 1);

    // TODO: Do I need two of these?
    do { // Clear out & prime the shift register:
        mp_shift();
    } while (mp_curr_scan_line);

    // Initialize the timer for the scan-line multiplexing:
    GPTimerCC26XX_Params gpt_params;
    GPTimerCC26XX_Params_init(&gpt_params);
    gpt_params.mode  = GPT_MODE_PERIODIC_UP;
    gpt_params.width = GPT_CONFIG_16BIT;
    tlc_gclk_timer_h = GPTimerCC26XX_open(QC14BOARD_GPTIMER0A, &gpt_params);

    GPTimerCC26XX_setLoadValue(tlc_gclk_timer_h, LED_MP_RATE);
    GPTimerCC26XX_registerInterrupt(tlc_gclk_timer_h, mp_tick, GPT_INT_TIMEOUT);
    GPTimerCC26XX_start(tlc_gclk_timer_h);
}

// TODO: Does this need to be called from a task?
void led_init_from_task() {
    // Set up our GPIO:
    led_pin_h = PIN_open(&led_pin_state, led_pin_table);
    tlc_spi_init();
    tlc_gclk_init();
    tlc_mp_init();
}
