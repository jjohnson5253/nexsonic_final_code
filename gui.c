/*
 * gui.c
 *
 *  Created on: Apr 13, 2019
 *      Author: jake
 *
 *  NOTE: when transferring this to custom board, replace all SCIA_BASE with SCIA_BASE
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
unsigned char *lengthofSweepStr;
unsigned char *msg;
uint16_t receivedChar;
uint16_t dutyCycle = 1040;
double dutyCycleTrack = 0.5;

double adcCntPerMv = 0.245*4096/3300; // counts per millivolt (0.304)
double mvPerAdcCnt = 3.28947; // 1/(0.304) counts per millivolt
double adcCntPerMa = 0.24824*2;
double maPerAdcCnt = 1/(0.24824*2);

uint16_t period = 861; // about 56kHz
uint16_t hrPeriodPrint;
uint16_t guiState = 0;
double voltSensorGain = 0.245 * 4096 / 3.3;

uint16_t adcBResult2;
uint16_t adcCResult0;
uint16_t adcCResult6;

uint16_t startSweepFreq;
uint16_t stopSweepFreq;

uint16_t dacVal = 600;

uint16_t currADC_avg; // ADC B2
uint16_t voltADC_avg; // ADC C0

uint16_t currADCValues[100];
uint16_t voltADCValues[100];
uint16_t voltValues[100];
uint16_t currValues[100];
uint16_t powerValues[100];
uint16_t impedanceValues[100];
uint16_t freqValues[100];

//double tenThou;
//double thou;
//double hun;
//double ten;
//double one; /// TODO: WHY DO I GET WRONG INPUT VALUE ( 134 ) WHEN I add this multiply by 1?????

uint16_t periodCnt;

//
// Globals
//
uint16_t UpdateFine, PeriodFine, status;
uint16_t MEP_ScaleFactor; // Global variable used by the SFO library
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
    uint16_t i;
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
    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);

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

        case 97: // a
            turnOffPwms();
            break;

        case 98: // b
            turnOnPwms();
            break;

        case 99: // c
            readAndSetStartSweepFreq();
            break;

        case 100: // d
            readAndSetStopSweepFreq();
            break;
    }
}

void run_duty_cycle_menu(){

    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);

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

    // pruint16_t freq value for python GUI to read
    my_itoa((uint16_t)dutyCyclePercent, dutyString);

    // pruint16_t msg
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)dutyString, 7);

    // pruint16_t a new line (need for python GUI to know when to stop reading)
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);
}

void run_frequency_menu(){

    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);

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

    // calculate period with hr resolution:
    // 1. 55975 * 861 / period ... we know 55975 corresponds to 861 period count
    // 2. subtract 13108, bc that is the basically 0 Hz (baseline), then divide by 793 bc that is about 1 Hz
    hrPeriodPrint = (48194475/period) - ((PeriodFine - 13108)/793);

    // pruint16_t freq value for python GUI to read
    my_itoa(hrPeriodPrint, freq);

    // pruint16_t msg
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)freq, 7);

    // pruint16_t a new line (need for python GUI to know when to stop reading)
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);
}

void run_hr_freq_menu(){

    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);

    uint16_t i = 0;


    switch(receivedChar) {
       case 49  : // decrease HR Freq (increase PeriodFine)
           if(PeriodFine < 0xFDE8){
               PeriodFine = PeriodFine + 793; // dec about 1Hz

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
           if(PeriodFine > 0x364C){ // 13900
               PeriodFine = PeriodFine - 793; // inc about 1Hz

               for(i=1; i<PWM_CH; i++)
               {
                   (*ePWM[i]).TBPRDHR = PeriodFine; //In Q16 format
               }
           }
           else{
               // decrement sysclk period (increase freq)
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

    // calculate period with hr resolution:
    // 1. 55975 * 861 / period ... we know 55975 corresponds to 861 period count
    // 2. subtract 13108, bc that is the basically 0 Hz (baseline), then divide by 793 bc that is about 1 Hz
    hrPeriodPrint = (48194475/period) - ((PeriodFine - 13108)/793);

    // print freq value for python GUI to read
    my_itoa(hrPeriodPrint, freq);

    // print msg
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)freq, 7);

    // print a new line (need for python GUI to know when to stop reading)
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

    // print hr freq value for python GUI to read
    my_itoa(PeriodFine, hrfreq);

    // print msg
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)hrfreq, 7);

    // print a new line (need for python GUI to know when to stop reading)
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

}

void run_dac_menu(){
    // Read a character from the FIFO.
    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);

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

    // note: all values in equation must be doubles not uint16_ts to prevent overflow
    double dacInVolts = ((double)dacVal * 3300)/4096;

    // pruint16_t dac value for python GUI to read
    my_itoa((uint16_t)dacInVolts, dac);
    // pruint16_t msg
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)dac, 7);

    // pruint16_t a new line (need for python GUI to know when to stop reading)
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);
}


void run_freq_sweep_menu(){

   // turn on led1 to indicate sweeping
   GPIO_writePin(5, 1);

   periodCnt = 0;
   uint16_t i = 0;

   // set period... 48194475 = 55975 * 861 (we know 55975 corresponds to 861 period counts)
   period = 48194475/ startSweepFreq;

   // reset PeriodFine to a little more than lowest value
   PeriodFine = 0x3334;

   int k;
   for(k=1; k<PWM_CH; k++)
   {
       (*ePWM[k]).TBPRDHR = PeriodFine; //In Q16 format
   }

   // add HR adjustment
   int freqWithoutHR = (48194475/period);
   int diffFromSetFreq = freqWithoutHR - startSweepFreq;

   if(diffFromSetFreq > 0){ // decrement by difference
       int i;
       int j;
       for(i=0;i<diffFromSetFreq;i++){
           if(PeriodFine < 0xFDE8){
               PeriodFine = PeriodFine + 793; // dec about 1Hz

               for(j=1; j<PWM_CH; j++)
               {
                   (*ePWM[j]).TBPRDHR = PeriodFine; //In Q16 format
               }
           }
           else{
               // increment sysclk period
               period = period + 1;
               configHRPWM(period);

               // reset PeriodFine to a little more than lowest value
               PeriodFine = 0x3334;

               for(j=1; j<PWM_CH; j++)
               {
                   (*ePWM[j]).TBPRDHR = PeriodFine; //In Q16 format
               }
           }

           status = SFO(); // in background, MEP calibration module
                           // continuously updates MEP_ScaleFactor
       }
   }
   else if(diffFromSetFreq < 0){ // increment by difference
       int i;
       int j;
       for(i=0;i<(diffFromSetFreq * (-1));i++){
           if(PeriodFine > 0x364C){ // 13900
               PeriodFine = PeriodFine - 793; // inc about 1Hz

               for(j=1; j<PWM_CH; j++)
               {
                   (*ePWM[j]).TBPRDHR = PeriodFine; //In Q16 format
               }
           }
           else{
               // decrement sysclk period (increase freq)
               period = period - 1;
               configHRPWM(period);

               // reset PeriodFine to a little less than highest value
               PeriodFine = 0xFFBE;

               for(j=1; j<PWM_CH; j++)
               {
                   (*ePWM[j]).TBPRDHR = PeriodFine; //In Q16 format
               }
           }

           status = SFO(); // in background, MEP calibration module
                           // continuously updates MEP_ScaleFactor
       }
   }

   // set start freq
   configHRPWM(period);

   // Beep buzzer once indicating start
   EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
   DEVICE_DELAY_US(250000);
   EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

   // send length of sweep to GUI
   uint16_t lengthOfSweep = stopSweepFreq - startSweepFreq;
   lengthofSweepStr = "       ";
   my_itoa(lengthOfSweep, lengthofSweepStr);
   SCI_writeCharArray(SCIA_BASE, (uint16_t*)lengthofSweepStr, 8);
   msg = "\n";
   SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

   // TODO: check why the freq doesn't increment a couple times when sending to GUI at 56040
   // sweep
   uint16_t p;
   for(p=0;p<lengthOfSweep;p++){ // set end freq



       // increase by 1 Hz
       if(PeriodFine > 0x364C){ // 13900
          PeriodFine = PeriodFine - 793; // inc about 1Hz

          for(i=1; i<PWM_CH; i++)
          {
              (*ePWM[i]).TBPRDHR = PeriodFine; //In Q16 format
          }
       }
       else{
          // decrement sysclk period (increase freq)
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

//       // read ADC current
//       currADC_avg = avg_ADC_curr();
//
//       // read ADC voltage
//       voltADC_avg = avg_ADC_volt();
//
//       // calculate voltage and current from adc values
//       double doubleVoltADC_avg = (double)voltADC_avg;
//       double doubleVolts = doubleVoltADC_avg * mvPerAdcCnt;
//       uint16_t volts = (uint16_t)doubleVolts;
//       double doubleCurrADC_avg = (double)currADC_avg;
//       double doubleCurr = doubleCurrADC_avg * maPerAdcCnt;  // 400mV per amp
//       uint16_t curr = (uint16_t)doubleCurr;
//
//       // calculate impedance and power from voltage and current
//       double impedance = doubleVolts / doubleCurr * 1000;
//       double power = doubleVolts * doubleCurr / 1000;
//       uint16_t uint16_tPower = (uint16_t)power;
//       uint16_t uint16_tImpedance = (uint16_t)impedance;

       // Print values for python GUI to read: adcvolt, adccurr, volts, curr, impedance, power

        // calculate period with hr resolution:
        // 1. 55975 * 861 / period ... we know 55975 corresponds to 861 period count
        // 2. subtract 13108, bc that is the basically 0 Hz (baseline), then divide by 793 bc that is about 1 Hz
        hrPeriodPrint = (48194475/period) - ((PeriodFine - 13108)/793);

        // This is freq but trying to trick sci... I don't why it sends extra characters from this write if I use freq or
        // any other character array
        rawADCvolt = "       ";
        my_itoa(hrPeriodPrint, rawADCvolt);
        SCI_writeCharArray(SCIA_BASE, (uint16_t*)rawADCvolt, 8);
        msg = "\n";
        SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

        // read and send ADC reading
        readAndSendADC();

//        rawADCvolt = "       ";
//        my_itoa(voltADC_avg, rawADCvolt);
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)rawADCvolt, 8);
//        msg = "\n";
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);
//
//        rawADCcurr = "       ";
//        my_itoa(currADC_avg, rawADCcurr);
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)rawADCcurr, 8);
//        msg = "\n";
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);
//
//        volt = "       ";
//        my_itoa((uint16_t)volts, volt);
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)volt, 8);
//        msg = "\n";
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);
//
//        currString = "       ";
//        my_itoa((uint16_t)(curr), currString);
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)currString, 8);
//        msg = "\n";
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);
//
//        powerString = "       ";
//        my_itoa(uint16_tPower, powerString);
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)powerString, 8);
//        msg = "\n";
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);
//
//        impedanceString = "       ";
//        my_itoa(uint16_tImpedance, impedanceString);
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)impedanceString, 8);
//        msg = "\n";
//        SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

        // wait (100000 = 0.6 seconds I think)
        DEVICE_DELAY_US(200000);
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
    double volts = doubleVoltADC_avg * mvPerAdcCnt;
    double doubleCurrADC_avg = (double)currADC_avg;
    double curr = doubleCurrADC_avg * maPerAdcCnt;  // 400mV per amp

//    double doubleVoltADC_avg = (double)voltADC_avg;
//    double volts = (doubleVoltADC_avg * 3300)/4096;
//    double doubleCurrADC_avg = (double)currADC_avg;
////    double curr = ((doubleCurrADC_avg * 3300)/4096) * 1;  // 400mV per amp
//    double curr = 70;
//
    // Bug was here: when dividing volts/curr, if value is two places, then multiplying by 1000 is too high for string to convert
    // calculate impedance and power from voltage and current
    double impedance = volts / curr * 1000;
    double power = volts * curr / 1000;
    uint16_t uint16_tPower = (uint16_t)power;
    uint16_t uint16_tImpedance = (uint16_t)impedance;

    //
    // old code that works and doesn't mess up... somehow using decimals is messing up the sci
    //
//    // calculate voltage and current from adc values
//    double doubleVoltADC_avg = (double)voltADC_avg;
//    double volts = (doubleVoltADC_avg * 3300)/4096;
//    double doubleCurrADC_avg = (double)currADC_avg;
//    double curr = ((doubleCurrADC_avg * 3300)/4096 ) *  2;  // 400mV per amp
//
//    // calculate impedance and power from voltage and current
//    double impedance = volts / curr * 1000;
//    double power = volts * curr / 1000;
//    uint16_t uint16_tPower = (uint16_t)power;
//    uint16_t uint16_tImpedance = (uint16_t)impedance;


    // print values for python GUI to read: adcvolt, adccurr, volts, curr, impedance, power
    rawADCvolt = "       ";
    my_itoa(voltADC_avg, rawADCvolt);
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)rawADCvolt, 8);
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

    rawADCcurr = "       ";
    my_itoa(currADC_avg, rawADCcurr);
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)rawADCcurr, 8);
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

    volt = "       ";
    my_itoa((uint16_t)volts, volt);
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)volt, 8);
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

    currString = "       ";
    my_itoa((uint16_t)(curr), currString);
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)currString, 8);
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

    impedanceString = "       ";
    my_itoa(uint16_tImpedance, impedanceString);
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)impedanceString, 8);
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

    powerString = "       ";
    my_itoa(uint16_tPower, powerString);
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)powerString, 8);
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

}

void sendSettingsVals(){

    // find dac
    double dacInVolts = ((double)dacVal * 3300)/4096;

    // calculate period with hr resolution:
    // 1. 55975 * 861 / period ... we know 55975 corresponds to 861 period count
    // 2. subtract 13108, bc that is the basically 0 Hz (baseline), then divide by 793 bc that is about 1 Hz
    hrPeriodPrint = (48194475/period) - ((PeriodFine - 13108)/793);

    // pruint16_t freq value for python GUI to read
    my_itoa(hrPeriodPrint, freq);
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

    // pruint16_t DAC
    my_itoa((uint16_t)dacInVolts, dac);
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)dac, 7);
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

}

void powerTrackAndSend(){
//
//    uint16_t prevPower3 = 0;
//    uint16_t prevPower2 = 0;
//    uint16_t prevPower1 = 0;
//    uint16_t currPower = 0;
//    uint16_t nextPower1 = 0;
//    uint16_t nextPower2 = 0;
//    uint16_t nextPower3 = 0;
//
//    uint16_t prevPowTotal = 0;
//    uint16_t nextPowTotal = 0;
//    uint16_t currPowTotal = 0;
//
//    uint16_t prev3Period = 0;
//    uint16_t prev2Period = 0;
//    uint16_t prev1Period = 0;
//    uint16_t currPeriod = 0;
//    uint16_t next1Period = 0;
//    uint16_t next2Period = 0;
//    uint16_t next3Period = 0;
//
//    if(PeriodFine < 0xFDE8){
//        PeriodFine = PeriodFine + 793; // dec about 1Hz
//
//        for(i=1; i<PWM_CH; i++)
//        {
//            (*ePWM[i]).TBPRDHR = PeriodFine; //In Q16 format
//        }
//    }
//    else{
//        // increment sysclk period
//        period = period + 1;
//        configHRPWM(period);
//
//        // reset PeriodFine to a little more than lowest value
//        PeriodFine = 0x3334;
//
//        for(i=1; i<PWM_CH; i++)
//        {
//            (*ePWM[i]).TBPRDHR = PeriodFine; //In Q16 format
//        }
//    }
//
//
//    // turn on LEDs to indicate start of power tracking
//    GPIO_writePin(2, 1);
//    GPIO_writePin(3, 1);
//    GPIO_writePin(5, 1);
//    GPIO_writePin(6, 1);
//
//    // Beep buzzer three times (1st one long) indicating start of power tracking
//    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
//    DEVICE_DELAY_US(400000);
//    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
//    DEVICE_DELAY_US(250000);
//    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
//    DEVICE_DELAY_US(250000);
//    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
//    DEVICE_DELAY_US(250000);
//    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
//    DEVICE_DELAY_US(250000);
//    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
//
//    uint16_t i = 0;
//    for(i=0;i<200;i++){
//
//        prevPower3 = powerAtPeriod(prev3Period);
//        prevPower2 = powerAtPeriod(prev2Period);
//        prevPower1 = powerAtPeriod(prev1Period);
//        currPower = powerAtPeriod(currPeriod);
//        nextPower1 = powerAtPeriod(next1Period);
//        nextPower2 = powerAtPeriod(next2Period);
//        nextPower3 = powerAtPeriod(next3Period);
//
//        prevPowTotal = prevPower3 + prevPower2 + prevPower1;
//        nextPowTotal = nextPower3 + nextPower2 + nextPower1;
//        currPowTotal = currPower * 3;
//
//        // check if previous power or next power are greater than current power
//        if(prevPowTotal > currPowTotal || nextPowTotal > currPowTotal){
//            if(prevPowTotal > nextPowTotal){
//                // set currPeriod to prevPeriod since it has more power than next and curr, and adjust other period vars
//                currPeriod = prev1Period;
//                prev3Period = currPeriod - 3;
//                prev2Period = currPeriod - 2;
//                prev1Period = currPeriod - 1;
//                next1Period = currPeriod + 1;
//                next2Period = currPeriod + 2;
//                next3Period = currPeriod + 3;
//            }
//            else{
//                // set currPeriod to nextPeriod since it has more power than prev and curr, and adjust other period vars
//                currPeriod = next1Period;
//                prev3Period = currPeriod - 3;
//                prev2Period = currPeriod - 2;
//                prev1Period = currPeriod - 1;
//                next1Period = currPeriod + 1;
//                next2Period = currPeriod + 2;
//                next3Period = currPeriod + 3;
//            }
//        }
//
//        // set period to current period and update
//        period = currPeriod - 1;
//        updatePeriod();
//
//        // send settings and adc data to gui for current period
//        sendSettingsVals();
//        readAndSendADC();
//
//        // wait 0.06 seconds (I think)
////        DEVICE_DELAY_US(100000);
//    }
//
//    // turn off LEDs to indicate end of power tracking
//    GPIO_writePin(2, 0);
//    GPIO_writePin(3, 0);
//    GPIO_writePin(5, 0);
//    GPIO_writePin(6, 0);
//
//    // Beep buzzer twice indicating end of power tracking
//    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
//    DEVICE_DELAY_US(250000);
//    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
//    DEVICE_DELAY_US(250000);
//    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
//    DEVICE_DELAY_US(250000);
//    EPWM_setActionQualifierAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
//
}

uint16_t powerAtPeriod(uint16_t testPeriod){

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
    uint16_t uint16_tPower = (uint16_t)power;

    return uint16_tPower;
}

void updatePeriod(){

    // update duty cycle to new period
    dutyCycle = period * dutyCycleTrack;
    EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_A, dutyCycle);
    EPWM_setCounterCompareValue(EPWM8_BASE, EPWM_COUNTER_COMPARE_B, dutyCycle);

    // update period
    EPWM_setTimeBasePeriod(EPWM8_BASE, period);
}

uint16_t avg_ADC_curr(){

    double avgB2 = 0;

    uint16_t i;
    for(i=0;i<1000;i++){
        read_ADC();
        avgB2 = avgB2 + adcBResult2;
    }
    avgB2 = avgB2 / 1000;
    return (uint16_t)avgB2;
}

uint16_t avg_ADC_volt(){

    double avgC0 = 0;

    uint16_t i;
    for(i=0;i<1000;i++){
        read_ADC();
        avgC0 = avgC0 + adcCResult0;
    }

    avgC0 = avgC0 / 1000;
    return (uint16_t)avgC0;
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
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    msg = "\r\n |\\__-- /\\       _-   \0";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    msg = "\r\n |/    __      -        \0";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    msg = "\r\n //\\  /  \\    /__     \0";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    msg = "\r\n |  o|  0|__     --_    \0";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    // change sonic's mouth
    if(smile == 0){
        msg = "\r\n \\\\____-- __ \\   ___-\0";
        SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
        msg = "\r\n (@@    []   / /_       \0";
        SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    }
    else{
        msg = "\r\n \\\\____-- __ \\   ___-\0";
        SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
        msg = "\r\n (@@    __/  / /_       \0";
        SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    }
    msg = "\r\n  -_____---   --_       \0";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    msg = "\r\n   //  \\ \\\\   ___-   \0";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    msg = "\r\n //|\\__/  \\\\  \\     \0";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    msg = "\r\n \\_-\\_____/  \\-\\      \0";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    msg = "\r\n       // \\\\--\\|           \0";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    msg = "\r\n  ____//  ||_          \0";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
    msg = "\r\n /_____\\ /___\\        \0";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 25);
}

// Implementation of itoa()
void my_itoa(uint16_t value, char* result){

    uint16_t temp = 0;
    uint16_t hunThou = 0;
    uint16_t tenThou = 0;
    uint16_t thou = 0;
    uint16_t huns = 0;
    uint16_t tens = 0;
    uint16_t ones = 0;

    if(value > 99999){
        temp = (uint16_t)value/100000;
        hunThou = (uint16_t)temp;
        value = value - hunThou*100000;
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
    else if(value > 9999){
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
    if(hunThou==0 && tenThou==0 && thou==0 && huns==0 && tens==0){
        result[0] = 32;
        result[1] = 32;
        result[2] = 32;
        result[3] = 32;
        result[4] = 32;
        result[5] = (char)ones + 48;
    } // 2 digits
    else if(hunThou==0 && tenThou==0 && thou==0 && huns==0){
        result[0] = 32;
        result[1] = 32;
        result[2] = 32;
        result[3] = 32;
        result[4] = (char)tens + 48;
        result[5] = (char)ones + 48;
    } // 3 digits
    else if(hunThou==0 && tenThou==0 && thou==0){
        result[0] = 32;
        result[1] = 32;
        result[2] = 32;
        result[3] = (char)huns + 48;
        result[4] = (char)tens + 48;
        result[5] = (char)ones + 48;
    } // 4 digits
    else if(hunThou==0 && tenThou==0){
        result[0] = 32;
        result[1] = 32;
        result[2] = (char)thou + 48;
        result[3] = (char)huns + 48;
        result[4] = (char)tens + 48;
        result[5] = (char)ones + 48;
    } // 5 digits
    else if(hunThou == 0){
        result[0] = 32;
        result[1] = (char)tenThou + 48;
        result[2] = (char)thou + 48;
        result[3] = (char)huns + 48;
        result[4] = (char)tens + 48;
        result[5] = (char)ones + 48;
    } // 6 digits
    else{
        result[0] = (char)hunThou + 48;
        result[1] = (char)tenThou + 48;
        result[2] = (char)thou + 48;
        result[3] = (char)huns + 48;
        result[4] = (char)tens + 48;
        result[5] = (char)ones + 48;
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
        (*ePWM[j]).AQCTLB.bit.CBU = AQ_CLEAR;             // PWM toggle high/low
        (*ePWM[j]).AQCTLB.bit.CBD = AQ_SET;

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
    // Disable uint16_ternal pull-up for the selected output pins
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


uint16_t my_str_to_int(unsigned char* input){

    uint16_t input4 = (uint16_t)input[4] - 48;
    uint16_t input3 = (uint16_t)input[3] - 48;
    uint16_t input2 = (uint16_t)input[2] - 48;
    uint16_t input1 = (uint16_t)input[1] - 48;
    uint16_t input0 = (uint16_t)input[0] - 48;

    uint16_t tenThou = input0 * 10000;
    uint16_t thou = input1 * 1000;
    uint16_t hun = input2 * 100;
    uint16_t ten = input3 * 10;
    uint16_t one = input4;

    uint16_t sum = tenThou + thou + hun + ten + one;

    uint16_t intResult = (uint16_t)sum;

    return intResult;
}


void readAndSetFreq(){

    unsigned char *readFreq;
    readFreq = "     ";

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    uint16_t receivedCharuint16_t = (uint16_t)receivedChar;
//    char receivedCharuint16_tString = (char)receivedCharuint16_t;
    readFreq[0] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
//    receivedCharuint16_tString = (char)receivedCharuint16_t;
    readFreq[1] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
//    receivedCharuint16_tString = (char)receivedCharuint16_t;
    readFreq[2] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
//    receivedCharuint16_tString = (char)receivedCharuint16_t;
    readFreq[3] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
//    receivedCharuint16_tString = (char)receivedCharuint16_t;
    readFreq[4] = (char)receivedCharuint16_t;

    // convert char to string
    uint16_t convertedStrToIntFreq = my_str_to_int(readFreq);

    // set period... 48194475 = 55975 * 861 (we know 55975 corresponds to 861 period counts)
    period = 48194475/ convertedStrToIntFreq;

    configHRPWM(period);

    // reset PeriodFine to a little more than lowest value
    PeriodFine = 0x3334;

    int k;
    for(k=1; k<PWM_CH; k++)
    {
        (*ePWM[k]).TBPRDHR = PeriodFine; //In Q16 format
    }

    int freqWithoutHR = (48194475/period);
    int diffFromSetFreq = freqWithoutHR - convertedStrToIntFreq;
    // update freq with HR considered
    if(diffFromSetFreq > 0){ // decrement by difference
        int i;
        int j;
        for(i=0;i<diffFromSetFreq;i++){
            if(PeriodFine < 0xFDE8){
                PeriodFine = PeriodFine + 793; // dec about 1Hz

                for(j=1; j<PWM_CH; j++)
                {
                    (*ePWM[j]).TBPRDHR = PeriodFine; //In Q16 format
                }
            }
            else{
                // increment sysclk period
                period = period + 1;
                configHRPWM(period);

                // reset PeriodFine to a little more than lowest value
                PeriodFine = 0x3334;

                for(j=1; j<PWM_CH; j++)
                {
                    (*ePWM[j]).TBPRDHR = PeriodFine; //In Q16 format
                }
            }

            status = SFO(); // in background, MEP calibration module
                            // continuously updates MEP_ScaleFactor
        }
    }
    else if(diffFromSetFreq < 0){ // increment by difference
        int i;
        int j;
        for(i=0;i<(diffFromSetFreq * (-1));i++){
            if(PeriodFine > 0x364C){ // 13900
                PeriodFine = PeriodFine - 793; // inc about 1Hz

                for(j=1; j<PWM_CH; j++)
                {
                    (*ePWM[j]).TBPRDHR = PeriodFine; //In Q16 format
                }
            }
            else{
                // decrement sysclk period (increase freq)
                period = period - 1;
                configHRPWM(period);

                // reset PeriodFine to a little less than highest value
                PeriodFine = 0xFFBE;

                for(j=1; j<PWM_CH; j++)
                {
                    (*ePWM[j]).TBPRDHR = PeriodFine; //In Q16 format
                }
            }

            status = SFO(); // in background, MEP calibration module
                            // continuously updates MEP_ScaleFactor
        }
    }

    // calculate period with hr resolution:
    // 1. 55975 * 861 / period ... we know 55975 corresponds to 861 period count
    // 2. subtract 13108, bc that is the basically 0 Hz (baseline), then divide by 793 bc that is about 1 Hz
    hrPeriodPrint = (48194475/period) - ((PeriodFine - 13108)/793);

    // pruint16_t freq value for python GUI to read
    my_itoa(hrPeriodPrint, freq);

    // pruint16_t msg
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)freq, 7);

    // pruint16_t a new line (need for python GUI to know when to stop reading)
    msg = "\n";
    SCI_writeCharArray(SCIA_BASE, (uint16_t*)msg, 3);

}

void readAndSetStartSweepFreq(){

    unsigned char *readStartFreq;
    readStartFreq = "     ";

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    uint16_t receivedCharuint16_t = (uint16_t)receivedChar;
    readStartFreq[0] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
    readStartFreq[1] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
    readStartFreq[2] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
    readStartFreq[3] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
    readStartFreq[4] = (char)receivedCharuint16_t;

    // convert char to string
    uint16_t convertedStrToIntStartFreq = my_str_to_int(readStartFreq);

    // set sweep start freq
    startSweepFreq = convertedStrToIntStartFreq;
}

void readAndSetStopSweepFreq(){

    unsigned char *readStopFreq;
    readStopFreq = "     ";

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    uint16_t receivedCharuint16_t = (uint16_t)receivedChar;
    readStopFreq[0] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
    readStopFreq[1] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
    readStopFreq[2] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
    readStopFreq[3] = (char)receivedCharuint16_t;

    receivedChar = SCI_readCharBlockingFIFO(SCIA_BASE);
    receivedCharuint16_t = (uint16_t)receivedChar;
    readStopFreq[4] = (char)receivedCharuint16_t;

    // convert char to string
    uint16_t convertedStrToIntStopFreq = my_str_to_int(readStopFreq);

    // set sweep stop freq
    stopSweepFreq = convertedStrToIntStopFreq;

}

// https://e2e.ti.com/support/microcontrollers/c2000/f/171/t/741374?CCS-LAUNCHXL-F28379D-Epwm-Enable-Disable
void turnOffPwms(){

    EALLOW;
    EPwm8Regs.TZFRC.bit.OST = 1;
    EPwm8Regs.TZCTL.bit.TZA = 0x02; // fOR FORCE low
    EPwm8Regs.TZCTL.bit.TZB = 0x02; //fOR fORCE loW
    EDIS;
}

void turnOnPwms(){

    EALLOW;
    EPwm8Regs.TZCLR.bit.OST = 0x01; // fOR FORCE low
    EPwm8Regs.TZCLR.bit.OST = 0x01; //fOR fORCE loW
    EDIS;
}

