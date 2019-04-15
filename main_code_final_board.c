//
// Main file for final project
// Jake Johnson 4/12/19
//

// Included Files
//
#include "driverlib.h"
#include "device.h"
#include <stdio.h>
#include "sci.h"

// Custom includes
#include "gui.h"

//
// Defines
//
#define EPWM8_TIMER_TBPRD  2080 // about 56kHz
#define EPWM8_MAX_CMPA     1040 // 50% duty cycle
#define EPWM8_MIN_CMPA     1040
#define EPWM8_MAX_CMPB     1040
#define EPWM8_MIN_CMPB     1040

#define EPWM_CMP_UP           1U
#define EPWM_CMP_DOWN         0U

#define REFERENCE_VDAC      0
#define REFERENCE_VREF      1
#define DACA         1
#define DACB         2

#define REFERENCE            REFERENCE_VREF
#define DAC_NUM                DACA

//
// Globals
//
typedef struct
{
    uint32_t epwmModule;
    uint16_t epwmCompADirection;
    uint16_t epwmCompBDirection;
    uint16_t epwmTimerIntCount;
    uint16_t epwmMaxCompA;
    uint16_t epwmMinCompA;
    uint16_t epwmMaxCompB;
    uint16_t epwmMinCompB;
}epwmInformation;

//
// Globals to hold the ePWM information used in this example
//
epwmInformation epwm8Info;
epwmInformation epwm8Info;


//
// Function Prototypes
//
void initEPWM8(void);
__interrupt void epwm8ISR(void);

void itoa(long unsigned int value, char* result, int base);

//
// Main
//
void main(void)
{

    // initializations
    Device_init();
    Device_initGPIO();

    // initialize interrupts
    Interrupt_initModule();
    Interrupt_initVectorTable();
    Interrupt_register(INT_EPWM8, &epwm8ISR);

    //////////////////////////////////////////////////////////////////////////////////
    // GPIO configurations

    // Configure GPIO14/15 as EPWM8A/8B pins respectively
    GPIO_setPadConfig(14, GPIO_PIN_TYPE_STD);
    GPIO_setPinConfig(GPIO_14_EPWM8A);
    GPIO_setPadConfig(15, GPIO_PIN_TYPE_STD);
    GPIO_setPinConfig(GPIO_15_EPWM8B);

    //
    // GPIO3 is the SCI Rx pin.
    //
    GPIO_setMasterCore(57, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_57_SCIRXDB);
    GPIO_setDirectionMode(57, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(57, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(57, GPIO_QUAL_ASYNC);

    //
    // GPIO2 is the SCI Tx pin.
    //
    GPIO_setMasterCore(56, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_56_SCITXDB);
    GPIO_setDirectionMode(56, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(56, GPIO_PIN_TYPE_STD);

    // turn gpio 13 high
    GPIO_setPinConfig(GPIO_13_GPIO13);
    GPIO_setPadConfig(13, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(13, GPIO_DIR_MODE_OUT);
    GPIO_writePin(13, 1);

    // turn gpio 22 high
    GPIO_setPinConfig(GPIO_22_GPIO22);
    GPIO_setPadConfig(22, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(22, GPIO_DIR_MODE_OUT);
    GPIO_writePin(22, 1);

    // turn gpio 2 high (should turn on led)
    GPIO_setPinConfig(GPIO_2_GPIO2);
    GPIO_setPadConfig(2, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(2, GPIO_DIR_MODE_OUT);
    GPIO_writePin(2, 1);

    // turn gpio 3 high (should turn on led)
    GPIO_setPinConfig(GPIO_3_GPIO3);
    GPIO_setPadConfig(3, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(3, GPIO_DIR_MODE_OUT);
    GPIO_writePin(3, 1);

    //////////////////////////////////////////////////////////////////////////////////

    //
    // Initialize SCIB and its FIFO.
    //
    SCI_performSoftwareReset(SCIB_BASE);

    //
    // Configure SCIB for echoback.
    //
    SCI_setConfig(SCIB_BASE, DEVICE_LSPCLK_FREQ, 9600, (SCI_CONFIG_WLEN_8 |
                                                        SCI_CONFIG_STOP_ONE |
                                                        SCI_CONFIG_PAR_NONE));
    SCI_resetChannels(SCIB_BASE);
    SCI_resetRxFIFO(SCIB_BASE);
    SCI_resetTxFIFO(SCIB_BASE);
    SCI_clearInterruptStatus(SCIB_BASE, SCI_INT_TXFF | SCI_INT_RXFF);
    SCI_enableFIFO(SCIB_BASE);
    SCI_enableModule(SCIB_BASE);
    SCI_performSoftwareReset(SCIB_BASE);

    #ifdef AUTOBAUD
        //
        // Perform an autobaud lock.
        // SCI expects an 'a' or 'A' to lock the baud rate.
        //
        SCI_lockAutobaud(SCIB_BASE);
    #endif

    //
    // Disable sync(Freeze clock to PWM as well)
    //
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    // init for pwm8a and b
    initEPWM8();

    //
    // Enable sync and clock to PWM
    //
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    //
    // Enable ePWM interrupts
    //
    Interrupt_enable(INT_EPWM8);

    //
    // Set up ADCs, initializing the SOCs to be triggered by software
    //
    initADCs();
    initADCSOCs();

    //
    // Enable Global Interrupt (INTM) and realtime interrupt (DBGM)
    //
    EINT;
    ERTM;

    //
    // IDLE loop. Just sit and loop forever (optional):
    //
    for(;;)
    {
        // run gui code
        run_gui();

        NOP;
    }
}

//
// EPWM8ISR - ePWM 8 ISR
//
__interrupt void epwm8ISR(void)
{
    //
    // Clear INT flag for this timer
    //
    EPWM_clearEventTriggerInterruptFlag(EPWM8_BASE);

    //
    // Acknowledge interrupt group
    //
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

//
// initEPWM8 - Configure EPWM5
//
void initEPWM8(void)
{
    //
    // Set-up TBCLK
    //
    EPWM_setTimeBasePeriod(EPWM8_BASE, EPWM8_TIMER_TBPRD);
    EPWM_setPhaseShift(EPWM8_BASE, 0U);
    EPWM_setTimeBaseCounter(EPWM8_BASE, 0U);

    //
    // Set Compare values
    //
    EPWM_setCounterCompareValue(EPWM8_BASE,
                                EPWM_COUNTER_COMPARE_A,
                                EPWM8_MIN_CMPA);
    EPWM_setCounterCompareValue(EPWM8_BASE,
                                EPWM_COUNTER_COMPARE_B,
                                EPWM8_MAX_CMPB);

    //
    // Set up counter mode
    //
    EPWM_setTimeBaseCounterMode(EPWM8_BASE, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_disablePhaseShiftLoad(EPWM8_BASE);
    EPWM_setClockPrescaler(EPWM8_BASE,
                           EPWM_CLOCK_DIVIDER_1,
                           EPWM_HSCLOCK_DIVIDER_1);

    //
    // Set up shadowing
    //
    EPWM_setCounterCompareShadowLoadMode(EPWM8_BASE,
                                         EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareShadowLoadMode(EPWM8_BASE,
                                         EPWM_COUNTER_COMPARE_B,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);

    //
    // Set actions
    //
    EPWM_setActionQualifierAction(EPWM8_BASE,
                                  EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(EPWM8_BASE,
                                  EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);
    EPWM_setActionQualifierAction(EPWM8_BASE,
                                  EPWM_AQ_OUTPUT_B,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB);
    EPWM_setActionQualifierAction(EPWM8_BASE,
                                  EPWM_AQ_OUTPUT_B,
                                  EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPB);

    //
    // Interrupt where we will change the Compare Values
    // Select INT on Time base counter zero event,
    // Enable INT, generate INT on 3rd event
    //
    EPWM_setInterruptSource(EPWM8_BASE, EPWM_INT_TBCTR_ZERO);
    EPWM_enableInterrupt(EPWM8_BASE);
    EPWM_setInterruptEventCount(EPWM8_BASE, 3U);

    //
    // Information this example uses to keep track of the direction the
    // CMPA/CMPB values are moving, the min and max allowed values and
    // a pointer to the correct ePWM registers
    //
    epwm8Info.epwmCompADirection = EPWM_CMP_UP;
    epwm8Info.epwmCompBDirection = EPWM_CMP_DOWN;
    epwm8Info.epwmTimerIntCount = 0U;
    epwm8Info.epwmModule = EPWM8_BASE;
    epwm8Info.epwmMaxCompA = EPWM8_MAX_CMPA;
    epwm8Info.epwmMinCompA = EPWM8_MIN_CMPA;
    epwm8Info.epwmMaxCompB = EPWM8_MAX_CMPB;
    epwm8Info.epwmMinCompB = EPWM8_MIN_CMPB;
}

//
// initADCs - Function to configure and power up ADCs A and B.
//
void initADCs(void)
{
    //
    // Setup VREF as internal
    //
    ADC_setVREF(ADCB_BASE, ADC_REFERENCE_EXTERNAL, ADC_REFERENCE_3_3V);
    ADC_setVREF(ADCC_BASE, ADC_REFERENCE_EXTERNAL, ADC_REFERENCE_3_3V);

    //
    // Set ADCCLK divider to /4
    //
    ADC_setPrescaler(ADCB_BASE, ADC_CLK_DIV_4_0);
    ADC_setPrescaler(ADCC_BASE, ADC_CLK_DIV_4_0);

    //
    // Set pulse positions to late
    //
    ADC_setInterruptPulseMode(ADCB_BASE, ADC_PULSE_END_OF_CONV);
    ADC_setInterruptPulseMode(ADCC_BASE, ADC_PULSE_END_OF_CONV);

    //
    // Power up the ADCs and then delay for 1 ms
    //
    ADC_enableConverter(ADCB_BASE);
    ADC_enableConverter(ADCC_BASE);

    DEVICE_DELAY_US(1000);
}

//
// initADCSOCs - Function to configure SOCs 0 and 1 of ADCs A and B.
//
void initADCSOCs(void)
{
    //
    // Configure SOCs of ADCA
    // - SOC0 will convert pin A0 with a sample window of 10 SYSCLK cycles.
    // - SOC1 will convert pin A1 with a sample window of 10 SYSCLK cycles.
    //
    ADC_setupSOC(ADCB_BASE, ADC_SOC_NUMBER2, ADC_TRIGGER_SW_ONLY,
                 ADC_CH_ADCIN2, 10);

    //
    // Set SOC1 to set the interrupt 1 flag. Enable the interrupt and make
    // sure its flag is cleared.
    //
    ADC_setInterruptSource(ADCB_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER2);
    ADC_enableInterrupt(ADCB_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADCB_BASE, ADC_INT_NUMBER1);

    //
    // Configure SOCs of ADCB
    // - SOC0 will convert pin B0 with a sample window of 10 SYSCLK cycles.
    // - SOC1 will convert pin B1 with a sample window of 10 SYSCLK cycles.
    //
    ADC_setupSOC(ADCC_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_SW_ONLY,
                 ADC_CH_ADCIN0, 10);
    ADC_setupSOC(ADCC_BASE, ADC_SOC_NUMBER6, ADC_TRIGGER_SW_ONLY,
                 ADC_CH_ADCIN6, 10);

    //
    // Set SOC1 to set the interrupt 1 flag. Enable the interrupt and make
    // sure its flag is cleared.
    //
    ADC_setInterruptSource(ADCC_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER6);
    ADC_enableInterrupt(ADCC_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADCC_BASE, ADC_INT_NUMBER1);

}
