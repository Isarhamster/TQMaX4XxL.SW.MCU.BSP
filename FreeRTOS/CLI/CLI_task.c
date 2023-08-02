/*
 *  Copyright (C) 2021 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <emmc_cmd.h>
#include <string.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include "portmacro.h"
#include "projdefs.h"
#include "FreeRTOS_CLI.h"
#include "Config/FreeRTOSConfig.h"

/* commands */
#include "gpio_led_blink_cmd.h"
#include "i2c_temperature_cmd.h"
#include "i2c_scan_cmd.h"
#include "i2c_rtc_cmd.h"
#include "qspi_nor_flash_cmd.h"
#include "eeprom_cmd.h"
#include "mcan_cmd.h"
#include "eth_cmd.h"
#include "lpddr4_cmd.h"
#include "gpio_dig_cmd.h"
#include "rs485_cmd.h"
#include "adc_cmd.h"
#include "afe_cmd.h"

#define APP_UART_RECEIVE_BUFSIZE (1)
#define MAX_INPUT_LENGTH         (50)

uint8_t uartReceiveBuffer[APP_UART_RECEIVE_BUFSIZE];
static SemaphoreP_Object uartReadDoneSem;

void cliTask( void* pvParameters )
{
    /* The buffers and welcome message are declared static to keep them off the stack. */
    static char welcomeMessage[]                     = "TQMaX4XxL MCU-BSP.\r\n\r\n"
                                                       "Type help to view a list of registered commands.\r\n\r\n";
    char*            outputString                    = FreeRTOS_CLIGetOutputBuffer();
    static char      inputString[MAX_INPUT_LENGTH]   = {0};
    char             rxedChar                        = 0;
    uint8_t          inputIndex                      = 0;
    BaseType_t       moreDataToFollow                = pdFALSE;
    UART_Transaction trans                           = {0};
    int32_t          status                          = 0;

    status = SemaphoreP_constructBinary(&uartReadDoneSem, 0);
    DebugP_assert(SystemP_SUCCESS == status);

    UART_Transaction_init(&trans);

    Drivers_uartOpen();

    /* send welcome message to user */
    trans.buf   = welcomeMessage;
    trans.count = strlen(welcomeMessage);
    UART_write(gUartHandle[CONFIG_USART0], &trans);

    /* register all commands */
    FreeRTOS_CLIRegisterCommand(&ledBlinkCommandDef);
    FreeRTOS_CLIRegisterCommand(&i2cTempCommandDef);
    FreeRTOS_CLIRegisterCommand(&i2cBusScanCommandDef);
    FreeRTOS_CLIRegisterCommand(&i2cRtcCommandDef);
    FreeRTOS_CLIRegisterCommand(&emmcCommandDef);
    FreeRTOS_CLIRegisterCommand(&qspiNorFlashCommandDef);
    FreeRTOS_CLIRegisterCommand(&eepromCommandDef);
    FreeRTOS_CLIRegisterCommand(&mcanCommandDef);
    FreeRTOS_CLIRegisterCommand(&ethCommandDef);
    FreeRTOS_CLIRegisterCommand(&lpddr4CommandDef);
    FreeRTOS_CLIRegisterCommand(&gpioDigCommandDef);
    FreeRTOS_CLIRegisterCommand(&rs485CommandDef);
    FreeRTOS_CLIRegisterCommand(&adcCommandDef);
    FreeRTOS_CLIRegisterCommand(&afeCommandDef);

    while(1)
    {
        /* Read one character user input. */
        trans.buf   = &rxedChar;
        trans.count = APP_UART_RECEIVE_BUFSIZE;
        UART_read(gUartHandle[CONFIG_USART0], &trans);

        /* Wait for read completion */
        SemaphoreP_pend(&uartReadDoneSem, SystemP_WAIT_FOREVER);

        if(rxedChar == '\n')
        {
            /* The command interpreter is called repeatedly until it returns pdFALSE. */
            do
            {
                /* Send the command string to the command interpreter. Any output generated by the command interpreter will be placed in the pcOutputString buffer. */
                moreDataToFollow = FreeRTOS_CLIProcessCommand(inputString, outputString, configCOMMAND_INT_MAX_OUTPUT_SIZE);

                /* send output string to user */
                trans.buf   = outputString;
                trans.count = strlen(outputString);
                UART_write(gUartHandle[CONFIG_USART0], &trans);
            } while(moreDataToFollow != pdFALSE);

            /* All the strings generated by the input command have been sent. Processing of the command is complete. Clear the input string ready to receive the next command. */
            inputIndex = 0;
            memset(inputString, 0x00, MAX_INPUT_LENGTH);
        }
        else
        {
            /* The if() clause performs the processing after a newline character is received. This else clause performs the processing if any other character is received. */
            if( rxedChar == '\r' )
            {
                /* Ignore carriage returns. */
            }
            else if( rxedChar == '\b' )
            {
                /* Backspace was pressed. Erase the last character in the input buffer - if there are any. */
                if( inputIndex > 0 )
                {
                    inputIndex--;
                    inputString[inputIndex] = '\0';
                }
            }
            else
            {
                /*
                 * A character was entered. It was not a new line, backspace or carriage return, so it is accepted as part of the input and placed into the input buffer.
                 * When a \n is entered the complete string will be passed to the command interpreter.
                 */
                if(inputIndex < MAX_INPUT_LENGTH)
                {
                    inputString[inputIndex] = rxedChar;
                    inputIndex++;
                }
            }
        }
    }
}

/**
 * @brief UART read callback function
 *
 * @param handle UART handler
 * @param transaction UART data structure
 */
void uartCallback(UART_Handle handle, UART_Transaction* transaction)
{
    SemaphoreP_post(&uartReadDoneSem);
}
