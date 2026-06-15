

#ifndef __TIMERSCFG_H__
#define __TIMERSCFG_H__

// 微控制器至少要32位
#define TIMEVAL UNS32

// 定义定时器的最大计数值为 0xFFFF，也就是十六进制的65535。
#define TIMEVAL_MAX 0xFFFF 

// The timer is incrementing every 4 us. 
//#define MS_TO_TIMEVAL(ms) (ms * 250)   1000/4即250个计数值
//#define US_TO_TIMEVAL(us) (us>>2)   右移两位，即除4

//// The timer is incrementing every 8 us.
//#define MS_TO_TIMEVAL(ms) ((ms) * 125)
//#define US_TO_TIMEVAL(us) ((us)>>3)

// 定时器1us递增一次，1ms内有1000个计数值，1us内有1个计数值
#define MS_TO_TIMEVAL(ms) ((ms) * 1000)
#define US_TO_TIMEVAL(us) (us)

#endif


