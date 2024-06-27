/*
 * Robert Murphy
 * 6/16/2024
 * CS 350 Final Project
 */

/*
 * This program will control a thermostat located on the TI CC3220SF
 * LAUNCH development board. This program will check the buttons
 * every 200ms, check the temperature every 500ms, and update the LED
 * and report to the server every second (via the UART). If the
 * right-side button is pushed, it will increase the temp set-point
 * by 1 degree. If the left-side button is pushed, it will decrease
 * the temp set-point by 1 degree. If the temperature is greater than
 * the set-point, the LED will turn off. If the temperature is less
 * than the set-point, the LED will turn on (the LED controls a
 * heater). You can simulate a heating or cooling room by putting
 * your finger on the temperature sensor. The output to the server
 * (via UART) will be formatted as <AA,BB,S,CCCC>.
 */

 /* Where:

   AA = ASCII decimal value of room temperature (00 - 99) degrees Celsius
   BB = ASCII decimal value of set-point temperature (00-99) degrees Celsius
   S = ‘0’ if heat is off, ‘1’ if heat is on
   CCCC = decimal count of seconds since board has been reset
   <%02d,%02d,%d,%04d> = temperature, set-point, heat, seconds
 */


#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Timer.h>
//#include <ti/drivers/UART.h>
#include <ti/display/Display.h>

/* Driver configuration */
#include "ti_drivers_config.h"

// global time constants per function
#define timer_period_gcd 100
#define timer_period_buttons 200
#define timer_period_sensor_read 500
#define timer_period_output 1000

#define num_tasks 3
/*
 *  ======== Task Type ========
 *
 *  Defines structure for the task type.
 */
typedef struct task {
    int state;                    // Current state of the task
    unsigned long period;         // Rate at which the task should tick
    unsigned long elapsedTime;    // Time since task's previous tick
    int (*tickFunction)(int);     // Function to call for task's tick
} task;



/*
 *  ======== Driver Handles ========
 */
I2C_Handle i2c;         // I2C driver handle
Timer_Handle timer0;    // Timer driver handle
Display_Handle display;       // Display driver handle
static void i2cErrorHandler(I2C_Transaction *transaction, Display_Handle display);

/*
 *  ======== Global Variables ========
 */
// UART global variables
//char output[64];
//int bytesToSend;

// I2C global variables
static const struct
{
    uint8_t address;
    uint8_t resultReg;
    char *id;
}
sensors[3] = {
    { 0x48, 0x0000, "11X" },
    { 0x49, 0x0000, "116" },
    { 0x41, 0x0000, "006" }
};
uint8_t txBuffer[1];
uint8_t rxBuffer[2];
I2C_Transaction i2cTransaction;
uint8_t targetAddress;

// Timer global variables
volatile unsigned char TimerFlag = 0;

// Thermostat global variables
enum BUTTON_STATES {INCREASE_SETPOINT, DECREASE_SETPOINT, BUTTONS_INIT} BUTTON_STATE;   // States for setting which button was pressed.
enum SENSOR_STATES {READ_SENSOR, SENSOR_INIT};                                          // States for the temperature sensor.
enum HEAT_STATES {HEAT_OFF, HEAT_ON, HEAT_INIT};                                        // States for the heating (heat/led off or on).
int16_t amb_temp = 0;      // Initialize temperature to 0 (will be updated by sensor reading).
int16_t user_temp_setpoint = 20;               // Initialize set-point for thermostat at 20°C (68°F).
int seconds = 0;                     // Initialize seconds to 0 (will be updated by timer).

/*
 *  ======== Callback ========
 */
// GPIO button callback function to increase the thermostat set-point.
void button_raise_setpoint(uint_least8_t index)
{
    BUTTON_STATE = INCREASE_SETPOINT;
}

// GPIO button callback function to decrease the thermostat set-point.
void button_lower_setpoint(uint_least8_t index)
{
    BUTTON_STATE = DECREASE_SETPOINT;
}

// Timer callback
void timerCallback(Timer_Handle myHandle, int_fast16_t status)
{
    TimerFlag = 1;  // Set flag to 1 to indicate timer is running.
}

/*
 *  ======== Initializations ========
 */
// initiialize the display UART (server)
/* Open the UART display for output */
void init_Display(){

    Display_init();
    display = Display_open(Display_Type_UART, NULL);
    if (display == NULL)
    {
        while (1) {}
    }
}

// Initialize I2C
// initiialize I2C
void init_I2C(void){

    I2C_init();
    I2C_Params i2cParams;
    Display_printf(display, 0, 0, "Initializing I2C Driver - \n");

    /* Create I2C for usage */
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    i2c               = I2C_open(CONFIG_I2C_0, &i2cParams);
    if (i2c == NULL)
    {
        Display_printf(display, 0, 0, "Error Initializing I2C\n");
        while (1) {}
    }
    else
    {
        Display_printf(display, 0, 0, "I2C Initialized!\n");
    }

    /* Common I2C transaction setup */
    i2cTransaction.writeBuf   = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf    = rxBuffer;
    i2cTransaction.readCount  = 0;
}

// initialize TMP116
void init_Sensor(void){

    int8_t i;
    for (i = 0; i < 3; i++)
    {
        i2cTransaction.targetAddress = sensors[i].address;
        txBuffer[0]                  = sensors[i].resultReg;

        if (I2C_transfer(i2c, &i2cTransaction))
        {
            targetAddress = sensors[i].address;
            Display_printf(display,
                           0,
                           0,
                           "Detected TMP%s sensor with target"
                           " address 0x%x",
                           sensors[i].id,
                           sensors[i].address);
        }
        else
        {
            i2cErrorHandler(&i2cTransaction, display);
        }
    }

    /* If we never assigned a target address */
    if (targetAddress == 0)
    {
        Display_printf(display, 0, 0, "Failed to detect a sensor!");
        I2C_close(i2c);
        while (1) {}
    }

    //Display_printf(display, 0, 0, "\nUsing last known sensor for samples.");
    i2cTransaction.targetAddress = targetAddress;
}



// initialize GPIO
void init_GPIO(){

    /* Call driver init functions for GPIO */
    GPIO_init();

    /* Configure the LED and button pins */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    /* Start with LED off */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);

    /* Install Button callback */
    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, button_raise_setpoint);

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
        GPIO_setCallback(CONFIG_GPIO_BUTTON_1, button_lower_setpoint);
        GPIO_enableInt(CONFIG_GPIO_BUTTON_1);
    }

    BUTTON_STATE = BUTTONS_INIT;
}

// Initialize Timer
void init_Timer(void)
{
    Timer_Params params;

    // Init the driver
    Timer_init();

    // Configure the driver
    Timer_Params_init(&params);
    params.period = 100000;                         // Set period to 1/10th of 1 second.
    params.periodUnits = Timer_PERIOD_US;           // Period specified in micro seconds
    params.timerMode = Timer_CONTINUOUS_CALLBACK;   // Timer runs continuously.
    params.timerCallback = timerCallback;           // Calls timerCallback method for timer callback.

    // Open the driver
    timer0 = Timer_open(CONFIG_TIMER_0, &params);
    if (timer0 == NULL)
    {
        /* Failed to initialized timer */
        while (1) {}
    }
    if (Timer_start(timer0) == Timer_STATUS_ERROR)
    {
        /* Failed to start timer */
        while (1) {}
    }
}

/*
 *  ======== adjust_setpoint ========
 *
 *  Check the current state of GPIO buttons
    CCS32200SF LAUNCH Board oriented with USB connector facing away from user
    left will increase
    Right will decrease
 */
int adjust_setpoint(int state)
{
    // Checks if desired temperature has been adjusted.
    switch (state)
    {
        case INCREASE_SETPOINT:
            if (user_temp_setpoint < 99)      // Ensure temperature is not set to above 99°C.
            {
                user_temp_setpoint++;
            }
            BUTTON_STATE = BUTTONS_INIT;
            break;
        case DECREASE_SETPOINT:
            if (user_temp_setpoint > 0)       // Ensure temperature is not set lower than 0°C.
            {
                user_temp_setpoint--;
            }
            BUTTON_STATE = BUTTONS_INIT;
            break;
    }
    state = BUTTON_STATE;           // Reset the state of GPIO buttons after reading.

    return state;
}

/*
 *  ======== readTemp ========
 *
 *  Read in the current temperature from the sensor and return the reading.
 */
int16_t readTemp(void)
{
    int16_t temperature = 0;
    i2cTransaction.readCount = 2;
    if (I2C_transfer(i2c, &i2cTransaction))
    {
        /*
        * Extract degrees C from the received data;
        * see TMP sensor datasheet
        */
        temperature = (rxBuffer[0] << 8) | (rxBuffer[1]); temperature *= 0.0078125;
        /*
        * If the MSB is set '1', then we have a 2's complement * negative value which needs to be sign extended
        */
        if (rxBuffer[0] & 0x80)
        {
            temperature |= 0xF000;
        }
    }
    else
    {
        Display_printf(display, 0, 0, "Error reading temperature sensor (%d)\n\r",i2cTransaction.status);
        Display_printf(display, 0, 0, "Please power cycle your board by unplugging USB and plugging back in.\n\r");
    }
    return temperature;
}

/*
 *  ======== getTemp ========
 *
 * reads sensor data for current temperature
 */
int getTemp(int state)
{
    switch (state)
    {
        case SENSOR_INIT:
            state = READ_SENSOR;
            break;

        case READ_SENSOR:
            amb_temp = readTemp();
            break;
    }

    return state;
}

/*
 *  ======== heatController ========
 *
 *  Compares the ambient temperature to the set-point.
 *  Turns on the heat (Led on) if ambient temperature is lower than the set-point.
 *  Turns off the heat (Led off) if ambient temperature is higher than the set-point.
 */
int heatController(int state)
{
    if (seconds != 0)
    {

        if (amb_temp < user_temp_setpoint)  // Turn on the heat.
        {
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
            state = HEAT_ON;
        }
        else                                // Turn off the heat.
        {
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
            state = HEAT_OFF;
        }

        // Report status to the server.
        Display_printf(display, 0, 0,
                             "<%02d,%02d,%d,%04d>\n\r",
                             amb_temp,
                             user_temp_setpoint,
                             state,
                             seconds);
    }

    seconds++;

    return state;
}



/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    // Create task list with tasks.
    task tasks[num_tasks] = {
        // Task 1 - Check button state and update set point.
        {
            .state = BUTTONS_INIT,
            .period = timer_period_buttons,
            .elapsedTime = timer_period_buttons,
            .tickFunction = &adjust_setpoint
        },
        // Task 2 - Get temperature from sensor.
        {
            .state = SENSOR_INIT,
            .period = timer_period_sensor_read,
            .elapsedTime = timer_period_sensor_read,
            .tickFunction = &getTemp
        },
        // Task 3 - Update heat mode and server.
        {
            .state = HEAT_INIT,
            .period = timer_period_output,
            .elapsedTime = timer_period_output,
            .tickFunction = &heatController
        }
    };

    // Call init functions for the drivers.
    //initUART();
    init_Display();
    init_I2C();
    init_GPIO();
    init_Sensor();
    init_Timer();

    // Loop forever.
    while (1)
    {
        unsigned int i = 0;
        for (i = 0; i < num_tasks; ++i)
        {
            if ( tasks[i].elapsedTime >= tasks[i].period )
            {
                tasks[i].state = tasks[i].tickFunction(tasks[i].state);
                tasks[i].elapsedTime = 0;
             }
             tasks[i].elapsedTime += timer_period_gcd;
        }

        // Wait for timer period.
        while(!TimerFlag){}
        // Set the timer flag variable to FALSE.
        TimerFlag = 0;
    }

    return (NULL);
}

// error handling for I2C
static void i2cErrorHandler(I2C_Transaction *transaction, Display_Handle display)
{
    switch (transaction->status)
    {
        case I2C_STATUS_TIMEOUT:
            Display_printf(display, 0, 0, "I2C transaction timed out!");
            break;
        case I2C_STATUS_CLOCK_TIMEOUT:
            Display_printf(display, 0, 0, "I2C serial clock line timed out!");
            break;
        case I2C_STATUS_ADDR_NACK:
            Display_printf(display,
                           0,
                           0,
                           "I2C extraneous target address 0x%x not"
                           " acknowledged!",
                           transaction->targetAddress);
            break;
        case I2C_STATUS_DATA_NACK:
            Display_printf(display, 0, 0, "I2C data byte not acknowledged!");
            break;
        case I2C_STATUS_ARB_LOST:
            Display_printf(display, 0, 0, "I2C arbitration to another controller!");
            break;
        case I2C_STATUS_INCOMPLETE:
            Display_printf(display, 0, 0, "I2C transaction returned before completion!");
            break;
        case I2C_STATUS_BUS_BUSY:
            Display_printf(display, 0, 0, "I2C bus is already in use!");
            break;
        case I2C_STATUS_CANCEL:
            Display_printf(display, 0, 0, "I2C transaction cancelled!");
            break;
        case I2C_STATUS_INVALID_TRANS:
            Display_printf(display, 0, 0, "I2C transaction invalid!");
            break;
        case I2C_STATUS_ERROR:
            Display_printf(display, 0, 0, "I2C generic error!");
            break;
        default:
            Display_printf(display, 0, 0, "I2C undefined error case!");
            break;
    }
}

