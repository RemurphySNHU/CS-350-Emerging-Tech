/*
 * Copyright (c) 2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== uart2echo.c ========
 */
#include <stdint.h>
#include <stddef.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>

/* Driver configuration */
#include "ti_drivers_config.h"

/*
 *  ======== mainThread ========
 */

/* Robert Murphy
 * 5/28/2024
 * SNHU CS 350 Milestone 2
 * */


void *mainThread(void *arg0)
{
    char input;
    const char echoPrompt[] = "Echoing characters:\r\n";
    UART2_Handle uart;
    UART2_Params uartParams;
    size_t bytesRead;
    size_t bytesWritten = 0;
    // uint32_t status = UART2_STATUS_SUCCESS;

    /* Call driver GPIO init functions */
    GPIO_init();

    /* Configure the LED pin */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Create a UART where the default read and write mode is BLOCKING
        Defaults values are: readMode = UART2_Mode_BLOCKING;
        writeMode = UART2_Mode_BLOCKING;
        readCallback = NULL;
        writeCallback = NULL;
        readReturnMode = UART2_ReadReturnMode_FULL;
        baudRate = 115200;
        dataLength = UART2_DataLen_8;
        stopBits = UART2_StopBits_1;
        parityType = UART2_Parity_NONE;
        userArg = NULL;
    */
    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;

    /* open the UART */
    uart = UART2_open(CONFIG_UART2_0, &uartParams);

    if (uart == NULL)
    {
        /* UART2_open() failed */
        while (1) {}
    }

    /* Turn on user LED to indicate successful initialization */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);

    /* display echo message */
    UART2_write(uart, echoPrompt, sizeof(echoPrompt), &bytesWritten);

    // Enumerate all machine states
    enum machineState {
        STATE_RESET,
        STATE_O, 
        STATE_ON, 
        STATE_OF, 
        STATE_OFF
}state;
    
    // initialize state to RESET
    state = STATE_RESET;
    

    /* Loop forever echoing. */
         
        while(1) {


            // Read input
            UART2_read(uart, &input, 1, &bytesRead);

            switch(state) {
                case STATE_RESET:
                    if (input == 'O') {
                        state = STATE_O;
                    }
                    else{
                        state = STATE_RESET;
                    }
                    break;

                case STATE_O:
                    if (input == 'N') {
                        state = STATE_ON;
                        GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
                        state = STATE_RESET;
                    }
                    else if (input == 'F') {
                        state = STATE_OF;

                    }else{
                        state = STATE_RESET;
                    }
                    break;

                case STATE_OF:
                    if (input == 'F') {
                        state = STATE_OFF;
                        GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
                        state = STATE_RESET;

                    } else {
                        state = STATE_RESET;
                    }
                    break;


            }


            // Echo input with each loop
            UART2_write(uart, &input, 1, &bytesWritten);

 }
}
