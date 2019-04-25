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
#include "sci.h"

// Variables for GUI
unsigned char *freq;
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
unsigned int period = 2080; // about 56kHz
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

int periodCnt;

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
           // update duty cycle to new period
           dutyCycle = period * dutyCycleTrack;
           EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_A, dutyCycle);
           EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_B, dutyCycle);
           // update period
           EPWM_setTimeBasePeriod(EPWM8_BASE, period);

           break;
       case 50  :
           // increase frequency decrease period
           period = period - 1;
           // update duty cycle to new period
           dutyCycle = period * dutyCycleTrack;
           EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_A, dutyCycle);
           EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_B, dutyCycle);
           // update period
           EPWM_setTimeBasePeriod(EPWM8_BASE, period);

           break;
       case 51  :
           guiState = 0;
           break;
    }

    // print freq value for python GUI to read
    my_itoa((116480000/period), freq); // 56000 * 2080 / period ... we know 56000 corresponds to 2080 period count

    // print msg
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)freq, 7);

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
   period = 2119;
   EPWM_setTimeBasePeriod(EPWM8_BASE, period);

   // Beep buzzer once indicating start
   EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
   DEVICE_DELAY_US(250000);
   EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

   // sweep
   while(period > 2045){

       // read ADC current
       currADC_avg = avg_ADC_curr();

       // read ADC voltage
       voltADC_avg = avg_ADC_volt();

       // calculate voltage and current from adc values
       double doubleVoltADC_avg = (double)voltADC_avg;
       double volts = (doubleVoltADC_avg * 3300)/4096;
       double doubleCurrADC_avg = (double)currADC_avg;
       double curr = (doubleCurrADC_avg * 3300)/4096 / 4;  // 400mV per amp

       // calculate impedance and power from voltage and current
       double impedance = volts / curr;
       double power = volts * curr / 1000;
       int intPower = (int)power;
       int intImpedance = (int)impedance;

       int periodPrint = (116480000/period);

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
    double curr = (doubleCurrADC_avg * 3300)/4096 / 4;  // 400mV per amp

    // calculate impedance and power from voltage and current
    double impedance = volts / curr;
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
    my_itoa((116480000/period), freq);
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
    double curr = (doubleCurrADC_avg * 3300)/4096 / 4;  // 400mV per amp

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


