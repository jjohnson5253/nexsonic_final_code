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
unsigned char *msg;
uint16_t receivedChar;
int dutyCycle = 1040;
double dutyCycleTrack = 0.5;
unsigned int period = 2080; // about 56kHz
int guiState = 0;

uint16_t adcBResult2;
uint16_t adcCResult0;
uint16_t adcCResult6;

uint16_t dacVal = 2048;

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
            break;

        case 53: // 5
            readAndSendADC();
            break;

        case 54: // 6
            sendSettingsVals();
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

    // 55khz = 2119 counts, 57kHz = 2045 counts
    msg = "\r\n 1. Sweep from 55kHz to 57kHz \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 33);
    msg = "\r\n 2. Go back \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 13);
    msg = "\r\n\nEnter number: \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 17);

    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);

    switch(receivedChar) {
       case 49  :

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

               // read ADC current and store in array
               currADC_avg = avg_ADC_curr();
               currADCValues[periodCnt] = currADC_avg;

               // read ADC voltage and store in array
               voltADC_avg = avg_ADC_volt();
               voltADCValues[periodCnt] = voltADC_avg;

               // TODO: change all this double casting by using uint32_t values instead of doubles for the ADC_avg values
               // calculate actual voltage and current value and store (3300mV / 4095 ADC val)
               double doubleVoltADC_avg = (double)voltADC_avg;
               double volts = (doubleVoltADC_avg * 3300)/4096;
               double doubleCurrADC_avg = (double)currADC_avg;
               double curr = (doubleCurrADC_avg * 3300)/4096 / 4;  // 400mV per amp

               voltValues[periodCnt] = (int)volts;
               currValues[periodCnt] = (int)curr;

               // maybe add more or less resolution to these
               double impedance = volts / curr;
               double power = volts * curr;

               // calculate power and impedance value and store
               powerValues[periodCnt] = (int)power / 1000;
               impedanceValues[periodCnt] = (int)impedance;

               // calculate current frequency and store
               freqValues[periodCnt] = (116480000/period); // 56000 * 2080 / period ... we know 56000 corresponds to 2080 period count

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

           // print a bunch of new lines to clear out window
           msg = "\r\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);

           // speech bubble for sonic
           msg = "\r\n           ------------- \0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 27);
           msg = "\r\n          <    Done!    > \0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 28);
           msg = "\r\n           ------------- \0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 27);
           msg = "\r\n              /          \0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 27);
           msg = "\r\n             /           \0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 27);

           // draw sonic smiling and tell user "sweeping"
           drawSonic(1);

           // write column headers
           msg = "\r\n\nFreq (Hz) | ADC (V) | ADC (I) | Volt (mV) | Curr (mA) | Z (Ohms)  | Power (mW)\n\0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 84);

           // print out values
           int i;
           for(i = 0; i<periodCnt; i++){

               // print a new line
               msg = "\r\n\0";
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

               // print FREQ
               // make msg 6 characters long for freq value
               freq = "      ";

               // convert testAvg to string
               my_itoa(freqValues[i], freq);

               // print msg
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)freq, 7);

               // add column spacing
               msg = "    | ";
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 7);

               // print RAWVOLTADC
               // make msg 6 characters long for curr raw adc val
               rawADCvolt = "      ";

               // convert testAvg to string
               my_itoa(voltADCValues[i], rawADCvolt);

               // print msg
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)rawADCvolt, 7);

               // add column spacing
               msg = "  | ";
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 5);

               // print RAWCURRADC
               // make msg 6 characters long for curr raw adc val
               rawADCcurr = "      ";

               // convert to string
               my_itoa(currADCValues[i], rawADCcurr);

               // print msg
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)rawADCcurr, 7);

               // add column spacing
               msg = "  | ";
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 5);

               // print VOLTAGE
               // make msg 6 characters long for curr raw adc val
               volt = "      ";

               // convert to string
               my_itoa(voltValues[i], volt);

               // print msg
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)volt, 7);

               // add column spacing
               msg = "    | ";
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 7);

               // print CURRENT
               // make msg 6 characters long for curr raw adc val
               currString = "      ";

               // convert to string
               my_itoa(currValues[i], currString);

               // print msg
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)currString, 7);

               // add column spacing
               msg = "    | ";
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 7);

               // print impedance
               // make msg 6 characters long for curr raw adc val
               impedanceString = "      ";

               // convert to string
               my_itoa(impedanceValues[i], impedanceString);

               // print msg
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)impedanceString, 7);

               // add column spacing
               msg = "    | ";
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 7);

               // print power
               // make msg 6 characters long for curr raw adc val
               powerString = "      ";

               // convert to string
               my_itoa(powerValues[i], powerString);

               // print msg
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)powerString, 7);

           }
           break;
       case 50  :
           guiState = 0;
           break;
       default :
           msg = "\r\nPlease choose one of the options\n\0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 36);
    }
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
    double power = volts * curr;
    int intPower = (int)power / 1000;
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

    // find dac and duty cycle
    double dutyCyclePercent = (double)dutyCycle/(double)period * 100;
    double dacInVolts = ((double)dacVal * 3300)/4096;

    // print freq
    my_itoa((116480000/period), freq);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)freq, 7);
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

    // print duty cycle
    my_itoa((int)dutyCyclePercent, dutyString);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)dutyString, 7);
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

    // print DAC
    my_itoa((int)dacInVolts, dac);
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)dac, 7);
    msg = "\n";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 3);

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


