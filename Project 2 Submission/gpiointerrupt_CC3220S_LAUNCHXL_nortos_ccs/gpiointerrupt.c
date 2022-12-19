/*
 *  ======== gpiointerrupt.c ========
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/Timer.h>
#include <ti/drivers/I2C.h>

/* Driver configuration */
#include "ti_drivers_config.h"

typedef struct task {
    int state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);
} task;

/*
 *  ======== UART Driver Stuff ========
 */
#define DISPLAY(x) UART_write(uart, &output, x);

// UART Global Variables
char output[64];
int bytesToSend;

// Driver Handles - Global variables
UART_Handle uart;

void initUART(void)
{
    UART_Params uartParams;
    // Init the driver
    UART_init();
    // Configure the driver
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.baudRate = 115200;

    // Open the driver
    uart = UART_open(CONFIG_UART_0, &uartParams);

    if (uart == NULL) {
        /* UART_open() failed */
        while (1);
    }
}
// ------------------------------- UART END -----------------------------



/*
 *  ======== I2C Driver Stuff ========
 */
// I2C Global Variables
static const struct {
    uint8_t address;
    uint8_t resultReg;
    char *id;
}

sensors[3] = {
    { 0x48, 0x0000, "11X" },
    { 0x49, 0x0000, "116" },
    { 0x41, 0x0001, "006" }
};

uint8_t txBuffer[1];
uint8_t rxBuffer[2];
I2C_Transaction i2cTransaction;

// Driver Handles - Global variables
I2C_Handle i2c;

// Make sure you call initUART() before calling this function.
void initI2C(void)
{
    int8_t i, found;
    I2C_Params i2cParams;
    DISPLAY(snprintf(output, 64, "Initializing I2C Driver - "));

    // Init the driver
    I2C_init();
    // Configure the driver
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    // Open the driver
    i2c = I2C_open(CONFIG_I2C_0, &i2cParams);

    if (i2c == NULL)
    {
        DISPLAY(snprintf(output, 64, "Failed\n\r"));
        while (1);
    }

    DISPLAY(snprintf(output, 32, "Passed\n\r"));

    // Boards were shipped with different sensors.
    // Welcome to the world of embedded systems.
    // Try to determine which sensor we have.
    // Scan through the possible sensor addresses
    /* Common I2C transaction setup */
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = rxBuffer;
    i2cTransaction.readCount = 0;

    found = false;
    for (i=0; i<3; ++i)
    {
        i2cTransaction.slaveAddress = sensors[i].address;
        txBuffer[0] = sensors[i].resultReg;
        DISPLAY(snprintf(output, 64, "Is this %s? ", sensors[i].id));
        if (I2C_transfer(i2c, &i2cTransaction))
        {
            DISPLAY(snprintf(output, 64, "Found\n\r"));
            found = true;
            break;
        }
        DISPLAY(snprintf(output, 64, "No\n\r"));
    }

    if(found)
    {
        DISPLAY(snprintf(output, 64, "Detected TMP%s I2C address: %x\n\r", sensors[i].id, i2cTransaction.slaveAddress));
    }
    else
    {
        DISPLAY(snprintf(output, 64, "Temperature sensor not found, contact professor\n\r"));
    }
}

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
        temperature = (rxBuffer[0] << 8) | (rxBuffer[1]);
        temperature *= 0.0078125;
        /*
         * If the MSB is set '1', then we have a 2's complement
         * negative value which needs to be sign extended
         */
        if (rxBuffer[0] & 0x80)
        {
            temperature |= 0xF000;
        }
    }
    else
    {
        DISPLAY(snprintf(output, 64, "Error reading temperature sensor %d\n\r", i2cTransaction.status));
        DISPLAY(snprintf(output, 64, "Please power cycle your board by unplugging USB and plugging back in.\n\r"));
    }

    return temperature;
}
// ---------------------------------- I2C END ------------------------------------------




/*
 *  ======== Timer Driver Stuff ========
 */
// Driver Handles - Global variables
Timer_Handle timer0;
volatile unsigned char TimerFlag = 0;

void timerCallback(Timer_Handle myHandle, int_fast16_t status)
{
    TimerFlag = 1;
}

void initTimer(void)
{
    Timer_Params params;
    // Init the driver
    Timer_init();
    // Configure the driver
    Timer_Params_init(&params);
    //Every 100 milliseconds
    params.period = 100000;
    params.periodUnits = Timer_PERIOD_US;
    params.timerMode = Timer_CONTINUOUS_CALLBACK;
    params.timerCallback = timerCallback;

    // Open the driver
    timer0 = Timer_open(CONFIG_TIMER_0, &params);
    if (timer0 == NULL) {
        /* Failed to initialized timer */
        while (1) {}
    }
    if (Timer_start(timer0) == Timer_STATUS_ERROR) {
        /* Failed to start timer */
        while (1) {}
    }
}
// ---------------------------------- Timer End ------------------------------------------
int buttonPress = 0;
int temperature = 0;
int setpoint = 25;
int heat = 0;
int seconds = 0;



    /*
     *  ======== GPIO Interrupt Stuff ========
     */
    /* TODO: Add rest of gpiointerrupt.c file default structure, including
       gpioButtonFxn0(), gpioButtonFxn1(), mainThread(), etc., including
       final scaffolding from the PDF for the mainThread(), duplicated below:
*/
void gpioButtonFxn0(uint_least8_t index)
{
    setpoint++;



}
void gpioButtonFxn1(uint_least8_t index)
{
    setpoint--;



}

task tasks[3];
const unsigned short tasksNum = 3;
const unsigned long tasksPeriodGCD = 100000;
enum BUTN_States { BUTN_Init, BUTN_Pressed, BUTN_NotPressed };

enum TEMP_States { TEMP_Init, TEMP_TIME };

enum PRINT_States { PRINT_Init, PRINT_TIME  };

void TimerISR() {
  unsigned char i;
  for (i = 0; i < tasksNum; ++i) { // Heart of the scheduler code
     if ( tasks[i].elapsedTime >= tasks[i].period ) { // Ready
        tasks[i].state = tasks[i].TickFct(tasks[i].state);
        tasks[i].elapsedTime = 0;
     }
     tasks[i].elapsedTime += tasksPeriodGCD;
  }
}
int TickFct_ReadTemp(int state) {
  switch(state) { // Transitions
  case TEMP_Init:
      DISPLAY(snprintf(output, 64, "Initializing temp state... \n\r"));
      state = TEMP_TIME;
      break;

     case TEMP_TIME:
        state = TEMP_TIME;
        break;
     default:
        state = TEMP_Init;
   } // Transitions

  switch(state) { // State actions
     case TEMP_TIME:
         temperature = readTemp();
         break;
     default:
         state = TEMP_Init;

        break;
  } // State actions
  return state;
}

int TickFct_CheckButn(int state) {
  switch(state) { // Transitions
     case BUTN_Init: // Initial transition
        state = BUTN_Pressed;
        break;

     case BUTN_Pressed:
        state = BUTN_Pressed;
        break;
     default:
        state = BUTN_Init;
   } // Transitions

  switch(state) { // State actions
     case BUTN_NotPressed:
        break;
     case BUTN_Pressed:
        break;
     default:
        break;
  } // State actions
  return state;
}

int TickFct_Print(int state) {
  switch(state) { // Transitions
     case PRINT_Init: // Initial transition
        state = PRINT_TIME;
        break;
     case PRINT_TIME:
        state = PRINT_TIME;
        break;
     default:
        state = PRINT_Init;
   } // Transitions

  switch(state) { // State actions

     case PRINT_TIME:
         seconds++;
         DISPLAY( snprintf(output, 64, "<%02d, %02d, %d, %04d>\n\r", temperature, setpoint, heat, seconds ))
        break;
     default:
        break;
  } // State actions
  return state;
}



void *mainThread(void *arg0)
{

    /* Call driver init functions */
    GPIO_init();
    initUART(); // The UART must be initialized before calling initI2C()
    initI2C();
    initTimer();


    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    unsigned char i =0;
            tasks[i].state = BUTN_Init;
            tasks[i].period = 100000;
            tasks[i].elapsedTime = tasks[i].period;
            tasks[i].TickFct = &TickFct_CheckButn;
            i++;
            tasks[i].state = TEMP_Init;
            tasks[i].period = 500000;
            tasks[i].elapsedTime = tasks[i].period;
            tasks[i].TickFct = &TickFct_ReadTemp;

            i++;
            tasks[i].state = PRINT_Init;
            tasks[i].period = 1000000;
            tasks[i].elapsedTime = tasks[i].period;
            tasks[i].TickFct = &TickFct_Print;




    // Loop Forever
        // The student should add flags (similar to the timer flag) to the button handlers
        /*  output to the server (via UART) should be formatted as
         *  AA    = ASCII decimal value of room temperature (00 - 99) degrees Celsius
         *  BB    = ASCII decimal value of set-point temperature (00-99) degrees Celsius
         *  S     = 0 if heat is off, 1 if heat is on
         *  CCCC  = decimal count of seconds since board has been reset
         *  <%02d,%02d,%d,%04d> = temperature, set-point, heat, seconds
         */




        while(1){


            /* Install Button callback */
        GPIO_setCallback(CONFIG_GPIO_BUTTON_0, gpioButtonFxn0);


        /* Enable interrupts */
        GPIO_enableInt(CONFIG_GPIO_BUTTON_0);

        if (CONFIG_GPIO_BUTTON_0 != CONFIG_GPIO_BUTTON_1) {
                /* Configure BUTTON1 pin */
                GPIO_setConfig(CONFIG_GPIO_BUTTON_1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

                /* Install Button callback */
                GPIO_setCallback(CONFIG_GPIO_BUTTON_1, gpioButtonFxn1);
                GPIO_enableInt(CONFIG_GPIO_BUTTON_1);
            }



          // Every 200ms check the button flags
          // Every 500ms read the temperature and update the LED
          // Every second output the following to the UART
          // <%02d,%02d,%d,%04d> = temperature, setpoint, heat, seconds


          // DISPLAY( snprintf(output, 64, "<%02d, %02d, %d, %04d>\n\r", temperature, setpoint_F, heat, seconds ))

          // Refer to zyBooks - "Converting different-period tasks to C"
          // Remember to configure the timer period




          while (!TimerFlag){} // Wait for the timer period
          TimerFlag = 0;       // Lower flag raised by timer
          ++timer0;
          TimerISR();
          if(setpoint > temperature){
                  heat = 1;
                  GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);                  }
              else{
                  heat = 0;
                  GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
              }





        }
}

