#ifndef __ENCODER_H
#define __ENCODER_H


/**
 * @note
 * Encoder rourine
 * The main function initializes and tests an encoder, and continuously prints the encoder position and
 * speed.
 */
#include "debug.h"


/* Global define */

/* The `#define SpeedSampleTimeMs 20` line is defining a constant macro `SpeedSampleTimeMs` with a
value of 20. This constant is used in the code to specify the time interval (in milliseconds) at
which the speed of the encoder is sampled and calculated. */
#define SpeedSampleTimeMs 20
/* Global Variable */

// void TIM2_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_Encoder_Init();
void Encoder_Init();



#endif /* __ENCODER_H */