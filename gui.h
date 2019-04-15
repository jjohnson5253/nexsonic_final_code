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

extern uint16_t receivedChar;
extern unsigned char *msg;

extern int dutyCycle;
extern double dutyCycleTrack;
extern unsigned int period;
extern int guiState;


#endif /* GUI_H_ */
