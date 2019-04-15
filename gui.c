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
unsigned char *msg;
uint16_t receivedChar;
int dutyCycle = 1040;
double dutyCycleTrack = 0.5;
unsigned int period = 2080; // about 56kHz
int guiState = 0;

uint16_t adcBResult2;
uint16_t adcCResult0;
uint16_t adcCResult6;

float testAvg;

void run_gui(){

    // print a bunch of new lines to clear out window
    msg = "\r\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);

    drawSonic();

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
    msg = "\r\n 3. Perform Sweep \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 21);
    msg = "\r\n\nEnter number: \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 17);

    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIB_BASE);

    switch(receivedChar) {
       case 49  :
           // duty cycle menu
           guiState = 1;
           break;
       case 50  :
           // freq menu
           guiState = 2;
           break;
       case 51  :
           // freq sweep menu
           guiState = 3;
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

uint16_t run_frequency_menu(){

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

uint16_t run_freq_sweep_menu(){

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
           // set frequency at 55kHz
           period = 2119;
           EPWM_setTimeBasePeriod(EPWM8_BASE, period);

           // sweep
           while(period > 2045){
               // read ADC
               testAvg = avg_ADC();
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

double avg_ADC(){

    double avgB2 = 0;

    int i;
    for(i=0;i<10;i++){
        read_ADC();
        avgB2 = avgB2 + adcBResult2;
    }
    avgB2 = avgB2 / 10;
    return avgB2;
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

void drawSonic(){

    // https://www.asciiart.eu/video-games/sonic-the-hedgehog

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
    msg = "\r\n \\\\____-- __ \\   ___-\0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n (@@    __/  / /_       \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n  -_____---   --_       \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n   //  \\ \\\\   ___-   \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
    msg = "\r\n //|\\__/  \\\\  \\     \0";
    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);


        // for some reason I run out of memory if I use this bottom half
//    msg = "\r\n \\_-\\_____/  \\-\\      \0";
//    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
//    msg = "\r\n \\_-\\_____/  \\-\\      \0";
//    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
//    msg = "\r\n  // \\\\--\\|           \0";
//    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
//    msg = "\r\n ____//  ||_          \0";
//    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
//    msg = "\r\n /_____\\ /___\\        \0";
//    SCI_writeCharArray(SCIB_BASE, (uint16_t*)msg, 25);
}

