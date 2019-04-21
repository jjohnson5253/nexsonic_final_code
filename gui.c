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
unsigned char *rawADCcurr;
unsigned char *rawADCvolt;
unsigned char *curr;
unsigned char *volt;
unsigned char *impedance;
unsigned char *power;
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

    // print a bunch of new lines to clear out window
    msg = "\r\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);

    switch(guiState){
    case 0:
        run_main_menu();
        break;

    case 1:
        run_duty_cycle_menu();
        break;

    case 2:
        run_frequency_menu();
        break;

    case 3:
        run_dac_menu();
        break;

    case 4:
        run_freq_sweep_menu();
        break;

    default:
        guiState = 0;
        break;
    }
}

void run_main_menu(){

    msg = "\r\n\nChoose an option: \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 23);
    msg = "\r\n 1. Change duty cycle \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n 2. Change frequency \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 24);
    msg = "\r\n 3. Change DAC \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 20);
    msg = "\r\n 4. Perform sweep \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 21);
    msg = "\r\n\nEnter number: \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 17);

    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);

    switch(receivedChar) {
       case 49  :
           // duty cycle menu
           guiState = 1;

           // Enter halt mode
           // drive gpio 40 low then high to power back on
           SysCtl_enterHaltMode();
           // reset clock
           SysCtl_setClock(DEVICE_SETCLOCK_CFG);

           break;
       case 50  :
           // freq menu
           guiState = 2;
           break;
       case 51  :
           // DAC menu
           guiState = 3;
           break;
       case 52  :
           // freq sweep menu
           guiState = 4;
           break;
       default :
           msg = "\r\nPlease choose one of the options\n\0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 36);
           break;
    }
}

void run_duty_cycle_menu(){

    msg = "\r\n 1. Increase duty cycle \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 27);
    msg = "\r\n 2. Decrease duty cycle \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 27);
    msg = "\r\n 3. Go back \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 13);
    msg = "\r\n\nEnter number: \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 17);

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
       default :
           msg = "\r\nPlease choose one of the options\n\0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 36);
    }
}

void run_frequency_menu(){

    msg = "\r\n 1. Decrease frequency \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 26);
    msg = "\r\n 2. Increase frequency \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 26);
    msg = "\r\n 3. Go back \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 13);
    msg = "\r\n\nEnter number: \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 17);

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
       default :
           msg = "\r\nPlease choose one of the options\n\0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 36);
    }
}

void run_dac_menu(){

    msg = "\r\n 1. Increase DAC \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 20);
    msg = "\r\n 2. Decrease DAC \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 20);
    msg = "\r\n 3. Go back \n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 13);
    msg = "\r\n\nEnter number: \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 17);

    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);

    switch(receivedChar) {
       case 49  :
           // Increase DAC
           dacVal = dacVal + 50;

           // set DAC
           DAC_setShadowValue(DACA_BASE, dacVal);
           DEVICE_DELAY_US(2);
           break;
       case 50  :
           // Decrease DAC
           dacVal = dacVal - 50;

           // set DAC
           DAC_setShadowValue(DACA_BASE, dacVal);
           DEVICE_DELAY_US(2);
           break;
       case 51  :
           guiState = 0;
           break;
       default :
           msg = "\r\nPlease choose one of the options\n\0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 36);
    }
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

           // print a bunch of new lines to clear out window
           msg = "\r\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
           msg = "\r\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);

           // speech bubble for sonic
           msg = "\r\n           ------------- \0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 27);
           msg = "\r\n          < Sweeping... > \0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 28);
           msg = "\r\n           ------------- \0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 27);
           msg = "\r\n              /          \0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 27);
           msg = "\r\n             /           \0";
           SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 27);

           // draw sonic with mouth open and tell user "sweeping"
           drawSonic(0);

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
               curr = "      ";

               // convert to string
               my_itoa(currValues[i], curr);

               // print msg
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)curr, 7);

               // add column spacing
               msg = "    | ";
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 7);

               // print CURRENT
               // make msg 6 characters long for curr raw adc val
               impedance = "      ";

               // convert to string
               my_itoa(impedanceValues[i], impedance);

               // print msg
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)impedance, 7);

               // add column spacing
               msg = "    | ";
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 7);

               // print CURRENT
               // make msg 6 characters long for curr raw adc val
               power = "      ";

               // convert to string
               my_itoa(powerValues[i], power);

               // print msg
               SCI_writeCharArray(SCIB_BASE, (uint16_t*)power, 7);

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


