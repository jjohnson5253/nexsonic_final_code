/*
 * gui.c
 *
 *  Created on: Apr 13, 2019
 *      Author: jake
 *
 *  NOTE: when transferring this to custom board, replace all SCIB_BASE with SCIB_BASE
 */

// Includes
#include "gui.h"
#include "driverlib.h"
#include "device.h"
#include "F28x_Project.h"
#include "SFO_V8.h"
#include "sci.h"

//
// Defines
//
#define PWM_CH            2        // # of PWM channels - 1
#define STATUS_SUCCESS    1
#define STATUS_FAIL       0

// Variables for GUI
unsigned char *freq;
unsigned char *hrfreq;
unsigned char *dutyString;
unsigned char *rawADCcurr;
unsigned char *rawADCvolt;
unsigned char *currString;
unsigned char *volt;
unsigned char *dac;
unsigned char *impedanceString;
unsigned char *powerString;
unsigned char *endMsg;
unsigned char *msg;
uint16_t receivedChar;
int dutyCycle = 1040;
double dutyCycleTrack = 0.5;
unsigned int period = 800; // about 56kHz
int guiState = 0;

uint16_t adcBResult2;
uint16_t adcCResult0;
uint16_t adcCResult6;

uint16_t dacVal = 600;

int currADC_avg; // ADC B2
int voltADC_avg; // ADC C0

int currADCValues[100];
int voltADCValues[100];
int voltValues[100];
int currValues[100];
int powerValues[100];
int impedanceValues[100];
int freqValues[100];

double tenThou;
double thou;
double hun;
double ten;
double one; /// TODO: WHY DO I GET WRONG INPUT VALUE ( 134 ) WHEN I add this multiply by 1?????

int periodCnt;

//
// Globals
//
uint16_t UpdateFine, PeriodFine, status;
int MEP_ScaleFactor; // Global variable used by the SFO library
                     // Result can be used for all HRPWM channels
                     // This variable is also copied to HRMSTEP
                     // register by SFO(0) function.

// Used by SFO library (ePWM[0] is a dummy value that isn't used)
volatile struct EPWM_REGS *ePWM[PWM_CH] = {&EPwm8Regs, &EPwm8Regs};

//
// Function Prototypes
//
void initHRPWM8GPIO(void);
void configHRPWM(uint16_t period);
void error(void);

void setupHrPwmVars(){
    //
    // Setup example variables
    //
    UpdateFine = 1;
    PeriodFine = 0x3334;
    status = SFO_INCOMPLETE;
}

void otherHrPwmSetup(){
    //
    // ePWM and HRPWM register initialization
    //
    int i;
    for(i=1; i<PWM_CH; i++)
    {
        // Change clock divider to /1
        // (PWM clock needs to be > 60MHz)
        (*ePWM[i]).TBCTL.bit.HSPCLKDIV = 0;
    }

    //
    // Calling SFO() updates the HRMSTEP register with calibrated MEP_ScaleFactor.
    // HRMSTEP must be populated with a scale factor value prior to enabling
    // high resolution period control.
    //
    while(status == SFO_INCOMPLETE)
    {
        status = SFO();
        if (status == SFO_ERROR)
        {
            error();    // SFO function returns 2 if an error occurs & # of MEP
        }               // steps/coarse step exceeds maximum of 255.
    }

}

void run_gui(){

    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);

    switch(receivedChar){
        case 49: // 1
            run_duty_cycle_menu();
            break;

        case 50: // 2
            run_frequency_menu();
            readAndSendADC();
            break;

        case 51: // 3
            run_dac_menu();
            readAndSendADC();
            break;

        case 52: // 4
            run_freq_sweep_menu();

//            // send stop sweep message so python knows to stop reading
//            endMsg = "ENDSWP";
//            SCI_writeCharArray(SCIB_BASE, (uint16_t*)freq, 7);
//            msg = "\n";
//            SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

            break;

        case 53: // 5
            readAndSendADC();
            break;

        case 54: // 6
            sendSettingsVals();
            readAndSendADC();
            break;

        case 55: // 7
            powerTrackAndSend();
            break;

        case 56: // 8
            readAndSetFreq();
            break;

        case 57: // 9
            run_hr_freq_menu();
            readAndSendADC();
            break;
    }
}

void run_duty_cycle_menu(){

    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);

    switch(receivedChar) {
       case 49  :
           // decrease duty cycle
           if(dutyCycleTrack < .90){
               dutyCycleTrack = dutyCycleTrack + 0.005;
           }
           dutyCycle = period * dutyCycleTrack;
           EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_A, dutyCycle);
           EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_B, dutyCycle);
           break;
       case 50  :
           // increase duty cycle
           if(dutyCycleTrack > .001){
               dutyCycleTrack = dutyCycleTrack - 0.005;
           }
           dutyCycle = period * dutyCycleTrack;
           EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_A, dutyCycle);
           EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_B, dutyCycle);
           break;
       case 51  :
           // return to home
           guiState = 0;
           break;
    }

    double dutyCyclePercent = (double)dutyCycle/(double)period * 100;

    // print freq value for python GUI to read
    my_itoa((int)dutyCyclePercent, dutyString);

    // print msg
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)dutyString, 7);

    // print a new line (need for python GUI to know when to stop reading)
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);
}

void run_frequency_menu(){

    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);

    switch(receivedChar) {
       case 49  :
           // decrease frequency increase period
           period = period + 1;

           // update period (should update duty cycle as well)
           configHRPWM(period);

           break;
       case 50  :
           // increase frequency decrease period
           period = period - 1;

           // update period (should update duty cycle as well)
           configHRPWM(period);

           break;
       case 51  :
           guiState = 0;
           break;
    }

    // print freq value for python GUI to read
    my_itoa((47980859/period), freq); // 56000 * 2080 / period ... we know 56000 corresponds to 2080 period count

    // print msg
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)freq, 7);

    // print a new line (need for python GUI to know when to stop reading)
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);
}

void run_hr_freq_menu(){

    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);

    uint16_t i = 0;


    switch(receivedChar) {
       case 49  : // decrease HR Freq (increase PeriodFine)
           if(PeriodFine < 0xFDE8){
               PeriodFine = PeriodFine + 100;

               for(i=1; i<PWM_CH; i++)
               {
                   (*ePWM[i]).TBPRDHR = PeriodFine; //In Q16 format
               }
           }
           else{
               // increment sysclk period
               period = period + 1;
               configHRPWM(period);

               // reset PeriodFine to a little more than lowest value
               PeriodFine = 0x3334;

               for(i=1; i<PWM_CH; i++)
               {
                   (*ePWM[i]).TBPRDHR = PeriodFine; //In Q16 format
               }
           }

           status = SFO(); // in background, MEP calibration module
                           // continuously updates MEP_ScaleFactor

           break;
       case 50  : // increase HR Freq (decrease PeriodFine)
           if(PeriodFine > 0x3333){
               PeriodFine = PeriodFine - 100;

               for(i=1; i<PWM_CH; i++)
               {
                   (*ePWM[i]).TBPRDHR = PeriodFine; //In Q16 format
               }
           }
           else{
               // decrement sysclk period
               period = period - 1;
               configHRPWM(period);

               // reset PeriodFine to a little less than highest value
               PeriodFine = 0xFFBE;

               for(i=1; i<PWM_CH; i++)
               {
                   (*ePWM[i]).TBPRDHR = PeriodFine; //In Q16 format
               }
           }

           status = SFO(); // in background, MEP calibration module
                           // continuously updates MEP_ScaleFactor
           break;
       case 51  :
           guiState = 0;
           break;
    }

    // print freq value for python GUI to read
    my_itoa((47980859/period), freq); // 56000 * 2080 / period ... we know 56000 corresponds to 2080 period count

    // print msg
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)freq, 7);

    // print a new line (need for python GUI to know when to stop reading)
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

    // print hr freq value for python GUI to read
    my_itoa(PeriodFine, hrfreq);

    // print msg
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)hrfreq, 7);

    // print a new line (need for python GUI to know when to stop reading)
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

}

void run_dac_menu(){
    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);

    switch(receivedChar) {
       case 49  :
           // decrease DAC
           dacVal = dacVal - 50;

           // set DAC
           DAC_setShadowValue(DACA_BASE, dacVal);
           DEVICE_DELAY_US(2);
           break;
       case 50  :
           // increase DAC
           dacVal = dacVal + 50;

           // set DAC
           DAC_setShadowValue(DACA_BASE, dacVal);
           DEVICE_DELAY_US(2);
           break;
       case 51  :
           guiState = 0;
           break;
    }

    // note: all values in equation must be doubles not ints to prevent overflow
    double dacInVolts = ((double)dacVal * 3300)/4096;

    // print dac value for python GUI to read
    my_itoa((int)dacInVolts, dac);
    // print msg
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)dac, 7);

    // print a new line (need for python GUI to know when to stop reading)
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);
}


void run_freq_sweep_menu(){

   // turn on led1 to indicate sweeping
   GPIO_writePin(5, 1);

   periodCnt = 0;

   // set frequency at 55kHz
   period = 870;
   EPWM_setTimeBasePeriod(EPWM8_BASE, period);

   // Beep buzzer once indicating start
   EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
   DEVICE_DELAY_US(250000);
   EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

   // sweep
   while(period > 830){

       // read ADC current
       currADC_avg = avg_ADC_curr();

       // read ADC voltage
       voltADC_avg = avg_ADC_volt();

       // calculate voltage and current from adc values
       double doubleVoltADC_avg = (double)voltADC_avg;
       double volts = (doubleVoltADC_avg * 3300)/4096;
       double doubleCurrADC_avg = (double)currADC_avg;
       double curr = ((doubleCurrADC_avg * 3300)/4096 ) *  2;  // 400mV per amp

       // calculate impedance and power from voltage and current
       double impedance = volts / curr * 1000;
       double power = volts * curr / 1000;
       int intPower = (int)power;
       int intImpedance = (int)impedance;

       int periodPrint = (47980859/period);

       // print values for python GUI to read: adcvolt, adccurr, volts, curr, impedance, power

       // This is freq but trying to trick sci... I don't why it sends extra characters from this write if I use freq or
       // any other character array
       rawADCvolt = "      ";
       my_itoa(periodPrint, rawADCvolt);
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)rawADCvolt, 7);
       msg = "\n";
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

       rawADCvolt = "      ";
       my_itoa(voltADC_avg, rawADCvolt);
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)rawADCvolt, 7);
       msg = "\n";
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

       rawADCcurr = "      ";
       my_itoa(currADC_avg, rawADCcurr);
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)rawADCcurr, 7);
       msg = "\n";
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

       volt = "      ";
       my_itoa((int)volts, volt);
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)volt, 7);
       msg = "\n";
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

       currString = "      ";
       my_itoa((int)(curr), currString);
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)currString, 7);
       msg = "\n";
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

       impedanceString = "      ";
       my_itoa(intImpedance, impedanceString);
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)impedanceString, 7);
       msg = "\n";
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

       powerString = "      ";
       my_itoa(intPower, powerString);
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)powerString, 8);
       msg = "\n";
       SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

       // increase frequency
       period = period - 1;

       // update duty cycle to new period
       dutyCycle = period * dutyCycleTrack;
       EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_A, dutyCycle);
       EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_B, dutyCycle);

       // update period
       EPWM_setTimeBasePeriod(EPWM8_BASE, period);

       // wait 0.06 seconds (I think)
       DEVICE_DELAY_US(100000);

       // increment period counter (times the period has incremented and how many frequencies have been covered)
       periodCnt++;
   }

   // Beep buzzer twice indicating end
   EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
   DEVICE_DELAY_US(250000);
   EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
   DEVICE_DELAY_US(250000);
   EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
   DEVICE_DELAY_US(250000);
   EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

   // turn off led1 to indicate end of sweep
   GPIO_writePin(5, 0);

}

void readAndSendADC(){

    // read ADC current and store in array
    currADC_avg = avg_ADC_curr();

    // read ADC voltage and store in array
    voltADC_avg = avg_ADC_volt();

    // calculate voltage and current from adc values
    double doubleVoltADC_avg = (double)voltADC_avg;
    double volts = (doubleVoltADC_avg * 3300)/4096;
    double doubleCurrADC_avg = (double)currADC_avg;
    double curr = ((doubleCurrADC_avg * 3300)/4096 ) *  2;  // 400mV per amp

    // calculate impedance and power from voltage and current
    double impedance = volts / curr * 1000;
    double power = volts * curr / 1000;
    int intPower = (int)power;
    int intImpedance = (int)impedance;

    // print values for python GUI to read: adcvolt, adccurr, volts, curr, impedance, power
    rawADCvolt = "      ";
    my_itoa(voltADC_avg, rawADCvolt);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)rawADCvolt, 7);
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

    rawADCcurr = "      ";
    my_itoa(currADC_avg, rawADCcurr);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)rawADCcurr, 7);
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

    volt = "      ";
    my_itoa((int)volts, volt);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)volt, 7);
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

    currString = "      ";
    my_itoa((int)(curr), currString);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)currString, 7);
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

    impedanceString = "      ";
    my_itoa(intImpedance, impedanceString);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)impedanceString, 7);
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

    powerString = "      ";
    my_itoa(intPower, powerString);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)powerString, 7);
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);
}

void sendSettingsVals(){

    // find dac
    double dacInVolts = ((double)dacVal * 3300)/4096;

    // print freq
    my_itoa((47980859/period), freq);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)freq, 7);
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

    // print DAC
    my_itoa((int)dacInVolts, dac);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)dac, 7);
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

}

void powerTrackAndSend(){

    int prevPower3 = 0;
    int prevPower2 = 0;
    int prevPower1 = 0;
    int currPower = 0;
    int nextPower1 = 0;
    int nextPower2 = 0;
    int nextPower3 = 0;

    int prevPowTotal = 0;
    int nextPowTotal = 0;
    int currPowTotal = 0;

    int prev3Period = period - 3;
    int prev2Period = period - 2;
    int prev1Period = period - 1;
    int currPeriod = period;
    int next1Period = period + 1;
    int next2Period = period + 2;
    int next3Period = period + 3;

    // turn on LEDs to indicate start of power tracking
    GPIO_writePin(2, 1);
    GPIO_writePin(3, 1);
    GPIO_writePin(5, 1);
    GPIO_writePin(6, 1);

    // Beep buzzer three times (1st one long) indicating start of power tracking
    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DEVICE_DELAY_US(400000);
    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DEVICE_DELAY_US(250000);
    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DEVICE_DELAY_US(250000);
    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DEVICE_DELAY_US(250000);
    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DEVICE_DELAY_US(250000);
    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

    int i = 0;
    for(i=0;i<200;i++){

        prevPower3 = powerAtPeriod(prev3Period);
        prevPower2 = powerAtPeriod(prev2Period);
        prevPower1 = powerAtPeriod(prev1Period);
        currPower = powerAtPeriod(currPeriod);
        nextPower1 = powerAtPeriod(next1Period);
        nextPower2 = powerAtPeriod(next2Period);
        nextPower3 = powerAtPeriod(next3Period);

        prevPowTotal = prevPower3 + prevPower2 + prevPower1;
        nextPowTotal = nextPower3 + nextPower2 + nextPower1;
        currPowTotal = currPower * 3;

        // check if previous power or next power are greater than current power
        if(prevPowTotal > currPowTotal || nextPowTotal > currPowTotal){
            if(prevPowTotal > nextPowTotal){
                // set currPeriod to prevPeriod since it has more power than next and curr, and adjust other period vars
                currPeriod = prev1Period;
                prev3Period = currPeriod - 3;
                prev2Period = currPeriod - 2;
                prev1Period = currPeriod - 1;
                next1Period = currPeriod + 1;
                next2Period = currPeriod + 2;
                next3Period = currPeriod + 3;
            }
            else{
                // set currPeriod to nextPeriod since it has more power than prev and curr, and adjust other period vars
                currPeriod = next1Period;
                prev3Period = currPeriod - 3;
                prev2Period = currPeriod - 2;
                prev1Period = currPeriod - 1;
                next1Period = currPeriod + 1;
                next2Period = currPeriod + 2;
                next3Period = currPeriod + 3;
            }
        }

        // set period to current period and update
        period = currPeriod - 1;
        updatePeriod();

        // send settings and adc data to gui for current period
        sendSettingsVals();
        readAndSendADC();

        // wait 0.06 seconds (I think)
//        DEVICE_DELAY_US(100000);
    }

    // turn off LEDs to indicate end of power tracking
    GPIO_writePin(2, 0);
    GPIO_writePin(3, 0);
    GPIO_writePin(5, 0);
    GPIO_writePin(6, 0);

    // Beep buzzer twice indicating end of power tracking
    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DEVICE_DELAY_US(250000);
    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DEVICE_DELAY_US(250000);
    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    DEVICE_DELAY_US(250000);
    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

}

int powerAtPeriod(int testPeriod){

    // update to new period
    period = testPeriod;
    updatePeriod();

    // read ADC current and voltage at new period
    currADC_avg = avg_ADC_curr();
    voltADC_avg = avg_ADC_volt();

    // calculate voltage and current from adc values
    double doubleVoltADC_avg = (double)voltADC_avg;
    double volts = (doubleVoltADC_avg * 3300)/4096;
    double doubleCurrADC_avg = (double)currADC_avg;
    double curr = ((doubleCurrADC_avg * 3300)/4096 ) *  2;  // 400mV per amp

    // calculate power from voltage and current
    double power = volts * curr / 1000;
    int intPower = (int)power;

    return intPower;
}

void updatePeriod(){

    // update duty cycle to new period
    dutyCycle = period * dutyCycleTrack;
    EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_A, dutyCycle);
    EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_B, dutyCycle);

    // update period
    EPWM_setTimeBasePeriod(EPWM8_BASE, period);
}

int avg_ADC_curr(){

    double avgB2 = 0;

    int i;
    for(i=0;i<1000;i++){
        read_ADC();
        avgB2 = avgB2 + adcBResult2;
    }
    avgB2 = avgB2 / 1000;
    return (int)avgB2;
}

int avg_ADC_volt(){

    double avgC0 = 0;

    int i;
    for(i=0;i<1000;i++){
        read_ADC();
        avgC0 = avgC0 + adcCResult0;
    }

    avgC0 = avgC0 / 1000;
    return (int)avgC0;
}

void read_ADC(){

    //
    // Convert, wait for completion, and store results
    //
    ADC_forceSOC(ADCB_BASE, ADC_SOC_NUMBER2);
    ADC_forceSOC(ADCC_BASE, ADC_SOC_NUMBER0);
    ADC_forceSOC(ADCC_BASE, ADC_SOC_NUMBER6);

    //
    // Wait for ADCB to complete, then acknowledge flag
    //
    while(ADC_getInterruptStatus(ADCB_BASE, ADC_INT_NUMBER1) == false)
    {
    }
    ADC_clearInterruptStatus(ADCB_BASE, ADC_INT_NUMBER1);

    //
    // Wait for ADCC to complete, then acknowledge flag
    //
    while(ADC_getInterruptStatus(ADCC_BASE, ADC_INT_NUMBER1) == false)
    {
    }
    ADC_clearInterruptStatus(ADCC_BASE, ADC_INT_NUMBER1);

    //
    // Store results
    //
    adcBResult2 = ADC_readResult(ADCBRESULT_BASE, ADC_SOC_NUMBER2);
    adcCResult0 = ADC_readResult(ADCCRESULT_BASE, ADC_SOC_NUMBER0);
    adcCResult6 = ADC_readResult(ADCCRESULT_BASE, ADC_SOC_NUMBER6);
}

void drawSonic(uint16_t smile){

    // https://www.asciiart.eu/video-games/sonic-the-hedgehog

    // draw sonic
    msg = "\r\n     ___------__        \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n |\\__-- /\\       _-   \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n |/    __      -        \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n //\\  /  \\    /__     \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n |  o|  0|__     --_    \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    // change sonic's mouth
    if(smile == 0){
        msg = "\r\n \\\\____-- __ \\   ___-\0";
        SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
        msg = "\r\n (@@    []   / /_       \0";
        SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    }
    else{
        msg = "\r\n \\\\____-- __ \\   ___-\0";
        SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
        msg = "\r\n (@@    __/  / /_       \0";
        SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    }
    msg = "\r\n  -_____---   --_       \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n   //  \\ \\\\   ___-   \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n //|\\__/  \\\\  \\     \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n \\_-\\_____/  \\-\\      \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n       // \\\\--\\|           \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n  ____//  ||_          \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n /_____\\ /___\\        \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
}

// Implementation of itoa()
void my_itoa(unsigned int value, char* result){

    uint16_t temp = 0;
    uint16_t tenThou = 0;
    uint16_t thou = 0;
    uint16_t huns = 0;
    uint16_t tens = 0;
    uint16_t ones = 0;

    if(value > 9999){
        temp = (uint16_t)value/10000;
        tenThou = (uint16_t)temp;
        value = value - tenThou*10000;
        temp = (uint16_t)value/1000;
        thou = (uint16_t)temp;
        value = value - thou*1000;
        temp = (uint16_t)value/100;
        huns = (uint16_t)temp;
        value = value - huns*100;
        temp = (uint16_t)value/10;
        tens = (uint16_t)temp;
        value = value - tens*10;
        ones = (uint16_t)value;
    }
    else if(value > 999){
        temp = (uint16_t)value/1000;
        thou = (uint16_t)temp;
        value = value - thou*1000;
        temp = (uint16_t)value/100;
        huns = (uint16_t)temp;
        value = value - huns*100;
        temp = (uint16_t)value/10;
        tens = (uint16_t)temp;
        value = value - tens*10;
        ones = (uint16_t)value;
    }
    else if(value > 99){
        temp = (uint16_t)value/100;
        huns = (uint16_t)temp;
        value = value - huns*100;
        temp = (uint16_t)value/10;
        tens = (uint16_t)temp;
        value = value - tens*10;
        ones = (uint16_t)value;
    }
    else if (value > 9){
        temp = (uint16_t)value/10;
        tens = (uint16_t)temp;
        value = value - tens*10;
        ones = (uint16_t)value;
    }
    else{
        ones = (uint16_t)value;
    }

    // only 1 digit
    if(tenThou==0 && thou==0 && huns==0 && tens==0){
        result[0] = 32;
        result[1] = 32;
        result[2] = 32;
        result[3] = 32;
        result[4] = (char)ones + 48;
    } // 2 digits
    else if(tenThou==0 && thou==0 && huns==0){
        result[0] = 32;
        result[1] = 32;
        result[2] = 32;
        result[3] = (char)tens + 48;
        result[4] = (char)ones + 48;
    } // 3 digits
    else if(tenThou==0 && thou==0){
        result[0] = 32;
        result[1] = 32;
        result[2] = (char)huns + 48;
        result[3] = (char)tens + 48;
        result[4] = (char)ones + 48;
    } // 4 digits
    else if(tenThou==0){
        result[0] = 32;
        result[1] = (char)thou + 48;
        result[2] = (char)huns + 48;
        result[3] = (char)tens + 48;
        result[4] = (char)ones + 48;
    } // 5 digits
    else{
        result[0] = (char)tenThou + 48;
        result[1] = (char)thou + 48;
        result[2] = (char)huns + 48;
        result[3] = (char)tens + 48;
        result[4] = (char)ones + 48;
    }
}


//
// configHRPWM - Configures all ePWM channels and sets up HRPWM
//                on ePWMxA channels &  ePWMxB channels
//
void configHRPWM(uint16_t period)
{
    uint16_t j;

    //
    // ePWM channel register configuration with HRPWM
    // ePWMxA toggle low/high with MEP control on Rising edge
    //
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 0;   // Disable TBCLK within the EPWM
    EDIS;

    for(j=1; j<PWM_CH; j++)
    {
        (*ePWM[j]).TBCTL.bit.PRDLD = TB_SHADOW;  // set Shadow load
        (*ePWM[j]).TBPRD = period;               // PWM frequency = 1/(2*TBPRD)
        (*ePWM[j]).CMPA.bit.CMPA = period / 2;   // set duty 50% initially
        (*ePWM[j]).CMPA.bit.CMPAHR = (1 << 8);   // initialize HRPWM extension
        (*ePWM[j]).CMPB.bit.CMPB = period / 2;   // set duty 50% initially
        (*ePWM[j]).CMPB.all |= 1;
        (*ePWM[j]).TBPHS.all = 0;
        (*ePWM[j]).TBCTR = 0;

        (*ePWM[j]).TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // Select up-down
                                                        // count mode
        (*ePWM[j]).TBCTL.bit.SYNCOSEL = TB_SYNC_DISABLE;
        (*ePWM[j]).TBCTL.bit.HSPCLKDIV = TB_DIV1;
        (*ePWM[j]).TBCTL.bit.CLKDIV = TB_DIV1;          // TBCLK = SYSCLKOUT
        (*ePWM[j]).TBCTL.bit.FREE_SOFT = 11;

        (*ePWM[j]).CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;  // LOAD CMPA on CTR = 0
        (*ePWM[j]).CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;
        (*ePWM[j]).CMPCTL.bit.SHDWAMODE = CC_SHADOW;
        (*ePWM[j]).CMPCTL.bit.SHDWBMODE = CC_SHADOW;

        (*ePWM[j]).AQCTLA.bit.CAU = AQ_SET;             // PWM toggle high/low
        (*ePWM[j]).AQCTLA.bit.CAD = AQ_CLEAR;
        (*ePWM[j]).AQCTLB.bit.CBU = AQ_SET;             // PWM toggle high/low
        (*ePWM[j]).AQCTLB.bit.CBD = AQ_CLEAR;

        EALLOW;
        (*ePWM[j]).HRCNFG.all = 0x0;
        (*ePWM[j]).HRCNFG.bit.EDGMODE = HR_BEP;          // MEP control on
                                                         // both edges.
        (*ePWM[j]).HRCNFG.bit.CTLMODE = HR_CMP;          // CMPAHR and TBPRDHR
                                                         // HR control.
        (*ePWM[j]).HRCNFG.bit.HRLOAD = HR_CTR_ZERO_PRD;  // load on CTR = 0
                                                         // and CTR = TBPRD
        (*ePWM[j]).HRCNFG.bit.EDGMODEB = HR_BEP;         // MEP control on
                                                         // both edges
        (*ePWM[j]).HRCNFG.bit.CTLMODEB = HR_CMP;         // CMPBHR and TBPRDHR
                                                         // HR control
        (*ePWM[j]).HRCNFG.bit.HRLOADB = HR_CTR_ZERO_PRD; // load on CTR = 0
                                                         // and CTR = TBPRD
        (*ePWM[j]).HRCNFG.bit.AUTOCONV = 1;        // Enable autoconversion for
                                                   // HR period

        (*ePWM[j]).HRPCTL.bit.TBPHSHRLOADE = 1;    // Enable TBPHSHR sync
                                                   // (required for updwn
                                                   //  count HR control)
        (*ePWM[j]).HRPCTL.bit.HRPE = 1;            // Turn on high-resolution
                                                   // period control.

        CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 1;      // Enable TBCLK within
                                                   // the EPWM
        (*ePWM[j]).TBCTL.bit.SWFSYNC = 1;          // Synchronize high
                                                   // resolution phase to
                                                   // start HR period
        EDIS;
    }
}

//
// initHRPWM1GPIO - Initialize HRPWM1 GPIOs
//
void initHRPWM8GPIO(void)
{
    EALLOW;

    //
    // Disable internal pull-up for the selected output pins
    // for reduced power consumption
    // Pull-ups can be enabled or disabled by the user.
    //
    GpioCtrlRegs.GPAPUD.bit.GPIO14 = 1;    // Disable pull-up on GPIO0 (EPWM1A)
    GpioCtrlRegs.GPAPUD.bit.GPIO15 = 1;    // Disable pull-up on GPIO1 (EPWM1B)

    //
    // Configure EPWM-1 pins using GPIO regs
    // This specifies which of the possible GPIO pins will be EPWM1 functional
    // pins.
    //
    GpioCtrlRegs.GPAMUX1.bit.GPIO14 = 1;   // Configure GPIO0 as EPWM1A
    GpioCtrlRegs.GPAMUX1.bit.GPIO15 = 1;   // Configure GPIO1 as EPWM1B

    EDIS;
}


void my_str_to_int(char* input, int intResult){

//    int input4 = (int)input[4] - 48;
//    int input3 = (int)input[3] - 48;
//    int input2 = (int)input[2] - 48;
//    int input1 = (int)input[1] - 48;
//    int input0 = (int)input[0] - 48;

//    double doubleInput4 = (double)input4;
//    double doubleInput3 = (double)input3;
//    double doubleInput2 = (double)input2;
//    double doubleInput1 = (double)input1;
//    double doubleInput0 = (double)input0;

    tenThou = (int)input[4] - 48 * 10000;
    thou = (int)input[3] - 48 * 1000;
    hun = (int)input[2] - 48 * 100;
    ten = (int)input[1] - 48 * 10;
    one = (int)input[0] - 48; /// TODO: WHY DO I GET WRONG INPUT VALUE ( 134 ) WHEN I add this multiply by 1?????

    double sum = tenThou + thou + hun + ten + one;

    intResult = (int)sum;
}

//void performStrToIntCalc(double input[4], double input[3], double){
//
//}

void readAndSetFreq(){

    unsigned char *readFreq;
    readFreq = "     ";

    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);
    int receivedCharInt = (int)receivedChar;
//    char receivedCharIntString = (char)receivedCharInt;
    readFreq[0] = (char)receivedCharInt;

    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);
    receivedCharInt = (int)receivedChar;
//    receivedCharIntString = (char)receivedCharInt;
    readFreq[1] = (char)receivedCharInt;

    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);
    receivedCharInt = (int)receivedChar;
//    receivedCharIntString = (char)receivedCharInt;
    readFreq[2] = (char)receivedCharInt;

    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);
    receivedCharInt = (int)receivedChar;
//    receivedCharIntString = (char)receivedCharInt;
    readFreq[3] = (char)receivedCharInt;

    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);
    receivedCharInt = (int)receivedChar;
//    receivedCharIntString = (char)receivedCharInt;
    readFreq[4] = (char)receivedCharInt;

    int convertedStrToIntFreq = 0;

    my_str_to_int(readFreq, convertedStrToIntFreq);

    period = 116480000/ convertedStrToIntFreq;

}




