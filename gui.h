/*
 * gui.h
 *
 *  Created on: Apr 13, 2019
 *      Author: jake
 */

#ifndef GUI_H_
#define GUI_H_

#include "driverlib.h"
#include "device.h"

// vars
extern uint16_t receivedChar;
extern unsigned char *msg;

extern uint16_t dutyCycle;
extern double dutyCycleTrack;
extern uint16_t period;
extern uint16_t guiState;

// functions
void run_main_menu();
void run_duty_cycle_menu();
void run_frequency_menu();
void run_freq_sweep_menu();
void run_dac_menu();
double avg_ADC();
void read_ADC();
void drawSonic();
void sweepingAnimation(uint16_t printCount);

#endif /* GUI_H_ */
