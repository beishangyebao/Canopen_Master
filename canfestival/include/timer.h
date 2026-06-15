#ifndef __timer_h__
#define __timer_h__

#include <timerscfg.h>  
#include <applicfg.h>

#define TIMER_HANDLE INTEGER16  

#include "data.h"

/* 定时器状态*/
#define TIMER_FREE 0   /* 定时器空闲 */
#define TIMER_ARMED 1	/* 定时器已设置 */
#define TIMER_TRIG 2  /* 定时器触发 */
#define TIMER_TRIG_PERIOD 3  /* 周期定时器触发 */

#define TIMER_NONE -1   /* 无定时器可用 */

typedef void (*TimerCallback_t)(CO_Data* d, UNS32 id);

struct struct_s_timer_entry {   //定时器结构体
	UNS8 state; // 定时器状态
	CO_Data* d; // CANopen 协议中代表节点状态的数据结构指针
	TimerCallback_t callback; // 回调函数
	UNS32 id; // 定时器ID
	TIMEVAL val; // 定时器值，表示定时器的剩余时间
	TIMEVAL interval;  // 定时器的周期
};

typedef struct struct_s_timer_entry s_timer_entry;

/* ---------  prototypes --------- */
/*#define SetAlarm(d, id, callback, value, period) printf("%s, %d, SetAlarm(%s, %s, %s, %s, %s)\n",__FILE__, __LINE__, #d, #id, #callback, #value, #period); _SetAlarm(d, id, callback, value, period)*/
/**
 * @ingroup timer
 * @brief Set an alarm to execute a callback function when expired.
 * @param *d Pointer to a CAN object data structure
 * @param id The alarm Id
 * @param callback A callback function
 * @param value Call the callback function at current time + value
 * @param period Call periodically the callback function
 * @return handle The timer handle
 */
//设置一个定时器，当定时器到期时执行回调函数
TIMER_HANDLE SetAlarm(CO_Data* d, UNS32 id, TimerCallback_t callback, TIMEVAL value, TIMEVAL period);
   // 一个指向CAN对象数据结构的指针，定时器ID，回调函数，定时器到期时间，周期性调用回调函数的时间间隔
   //返回值为定时器句柄
/**
 * @ingroup timer
 * @brief Delete an alarm before expiring.
 * @param handle A timer handle
 * @return The timer handle
 */
//删除定时器，参数和返回值均为定时器句柄
TIMER_HANDLE DelAlarm(TIMER_HANDLE handle);

void TimeDispatch(void);

/**
 * @ingroup timer
 * @brief Set a timerfor a given time.
 * @param value The time value.
 */
//设置一个定时器，用于在指定时间后唤醒，参数就是定时器到期时间
void setTimer(TIMEVAL value);

/**
 * @ingroup timer
 * @brief Get the time elapsed since latest timer occurence.
 * @return time elapsed since latest timer occurence
 */
//获取自上次定时器触发以来经过的时间
TIMEVAL getElapsedTime(void);

#endif /* #define __timer_h__ */
