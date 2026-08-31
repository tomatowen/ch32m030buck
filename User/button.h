#ifndef __BUTTON_H
#define __BUTTON_H

#define BUTTON_IT_PRI   (1<<6)|(1<<7)

extern volatile uint8_t event_flag;
extern volatile uint32_t press_ticks,release_ticks;
extern volatile uint32_t systemtick;

void EXTI4_INT_INIT(void);



#endif