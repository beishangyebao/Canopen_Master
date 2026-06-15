#ifndef __canfestival_timer_H
#define __canfestival_timer_H
#include "sys.h"
#include "stdlib.h"
#include "stm32f10x.h"
#include "canfestival.h"

 



void TIM3_Int_Init(u16 arr,u16 psc);
void setTimer(TIMEVAL value);
TIMEVAL getElapsedTime(void);

#endif
