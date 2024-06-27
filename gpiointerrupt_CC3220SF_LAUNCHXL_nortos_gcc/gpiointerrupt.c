/*
 * Robert Murphy
 * 6/8/2024
 * CS 350 Milestone 3
 */

/*
 * This program is a morse code state machine
 * It will power up to the SOS state by default
 * After initializing, the system listens for user input
 * via the two GPIO push button inputs.
 * This will toggle the machine state from SOS, OK and from OK, to SOS
 * The program is written to ensure messages are complete
 * prior to switching states
 */

/*
 *  ======== gpiointerrupt.c ========
 */
#include <stdint.h>
#include <stddef.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/Timer.h>

/* Driver configuration */
#include "ti_drivers_config.h"

/* Morse code message states */
enum MachineStates {SOS, OK} MachineState, PushButton;

/* LED code states */
enum CodeStates {DOT, DASH, OFF} CodeState;

/* Morse messages */
enum CodeStates codeSOS[] = {

    /* S */
    DOT, OFF,
    DOT, OFF,
    DOT, OFF, OFF, OFF,

    /* O */
    DASH, DASH, DASH, OFF,
    DASH, DASH, DASH, OFF,
    DASH, DASH, DASH, OFF, OFF, OFF,

    /* S */
    DOT, OFF,
    DOT, OFF,
    DOT,
    OFF, OFF, OFF, OFF, OFF, OFF, OFF
};

enum CodeStates codeOK[] = {

    /* O */
    DASH, DASH, DASH, OFF,
    DASH, DASH, DASH, OFF,
    DASH, DASH, DASH, OFF, OFF, OFF,

    /* K */
    DASH, DASH, DASH, OFF,
    DOT, OFF,
    DASH, DASH, DASH,
    OFF, OFF, OFF, OFF, OFF, OFF, OFF
};

/* byte tracker, initialized to zero. */
unsigned int bytesRead = 0;

/*
 *  ======== updateGPIO ========
 *  Function for the LEDs to represent a dot, dash, or break.
 */
void updateGPIO() {
    switch(CodeState) {

        case DOT:  // Red on; Green off
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
            GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);
            break;

        case DASH:   // Green on; Red off
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
            GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_ON);
            break;

        case OFF:   // both off
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
            GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);
            break;
    }
}

/*
 *  ======== timerCallback ========
 *  Callback function for the timer interrupt.
 */
void timerCallback(Timer_Handle myHandle, int_fast16_t status)
{
    switch(MachineState) {

        case SOS:
            CodeState = codeSOS[bytesRead];
            updateGPIO();
            bytesRead++;

            if(bytesRead == (sizeof(codeSOS)/sizeof(codeSOS[0]))) {
                MachineState = PushButton;
                bytesRead = 0;
            }
            break;

        case OK:
            CodeState = codeOK[bytesRead];
            updateGPIO();
            bytesRead++;

            if(bytesRead == (sizeof(codeOK)/sizeof(codeOK[0]))) {
                MachineState = PushButton;
                bytesRead = 0;
            }
            break;

    }
}

/*
 *  ======== timerInit ========
 *  Initialization function for the timer interrupt on timer0.
 */
void initTimer(void)
{
    Timer_Handle timer0;
    Timer_Params params;

    Timer_init();
    Timer_Params_init(&params);
    params.period = 500000;
    params.periodUnits = Timer_PERIOD_US;
    params.timerMode = Timer_CONTINUOUS_CALLBACK;
    params.timerCallback = timerCallback;

    timer0 = Timer_open(CONFIG_TIMER_0, &params);

    if(timer0 == NULL) {
        /* Failed to initialized timer */
        while (1) {}
    }

    if(Timer_start(timer0) == Timer_STATUS_ERROR)
    {
        /* Failed to start timer */
        while (1) {}
    }
}

/*
 *  ======== pushButtonCallback ========
 *  Callback function for the GPIO interrupt
 */
void pushButtonCallback(uint_least8_t index)
{
    switch(PushButton) {
        case SOS:
            PushButton = OK;
            break;

        case OK:
            PushButton = SOS;
            break;

    }
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    /* Call driver init functions for GPIO and timer */
    GPIO_init();
    initTimer();

    /* Configure the LED and button pins */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_LED_1, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    /* Start with LEDs off */
    //GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
    //GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);

    /* Set initial states to SOS_MESSAGE */
    PushButton = SOS;
    MachineState = PushButton;

    /* Install Button callback */
    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, pushButtonCallback);

    /* Enable interrupts */
    GPIO_enableInt(CONFIG_GPIO_BUTTON_0);

    /*
     *  If more than one input pin is available for your device, interrupts
     *  will be enabled on CONFIG_GPIO_BUTTON1.
     */
    if (CONFIG_GPIO_BUTTON_0 != CONFIG_GPIO_BUTTON_1) {
        /* Configure BUTTON1 pin */
        GPIO_setConfig(CONFIG_GPIO_BUTTON_1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

        /* Install Button callback */
        GPIO_setCallback(CONFIG_GPIO_BUTTON_1, pushButtonCallback);
        GPIO_enableInt(CONFIG_GPIO_BUTTON_1);
    }

    return (NULL);
}
