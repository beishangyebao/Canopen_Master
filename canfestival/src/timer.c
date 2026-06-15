
/* #define DEBUG_WAR_CONSOLE_ON */
/* #define DEBUG_ERR_CONSOLE_ON */

#include <applicfg.h>
#include "timer.h"

/*  ---------  The timer table --------- */
s_timer_entry timers[MAX_NB_TIMER] = {{TIMER_FREE, NULL, NULL, 0, 0, 0},};

TIMEVAL total_sleep_time = TIMEVAL_MAX;
TIMER_HANDLE last_timer_raw = -1;

#define min_val(a,b) ((a<b)?a:b)

/*!
** -------  Use this to declare a new alarm ------
**
** @param d
** @param id
** @param callback
** @param value
** @param period
**
** @return
**/
TIMER_HANDLE SetAlarm(CO_Data* d, UNS32 id, TimerCallback_t callback, TIMEVAL value, TIMEVAL period)
{
	TIMER_HANDLE row_number;
	s_timer_entry *row;

	/* in order to decide new timer setting we have to run over all timer rows */
	for(row_number=0, row=timers; row_number <= last_timer_raw + 1 && row_number < MAX_NB_TIMER; row_number++, row++)
	{
		if (callback && 	/* if something to store */
		   row->state == TIMER_FREE) /* and empty row */
		{	/* just store */
			TIMEVAL real_timer_value;
			TIMEVAL elapsed_time;

			if (row_number == last_timer_raw + 1) last_timer_raw++;

			elapsed_time = getElapsedTime();
			/* set next wakeup alarm if new entry is sooner than others, or if it is alone */
			real_timer_value = value;
			real_timer_value = min_val(real_timer_value, TIMEVAL_MAX);

			if (total_sleep_time > elapsed_time && total_sleep_time - elapsed_time > real_timer_value)
			{
				total_sleep_time = elapsed_time + real_timer_value;
				setTimer(real_timer_value);
			}
			row->callback = callback;
			row->d = d;
			row->id = id;
			row->val = value + elapsed_time;
			row->interval = period;
			row->state = TIMER_ARMED;
			return row_number;
		}
	}

	return TIMER_NONE;
}

/*!
**  -----  Use this to remove an alarm ----
**
** @param handle
**
** @return
**/
TIMER_HANDLE DelAlarm(TIMER_HANDLE handle)
{
	/* Quick and dirty. system timer will continue to be trigged, but no action will be preformed. */
	MSG_WAR(0x3320, "DelAlarm. handle = ", handle);
	if(handle != TIMER_NONE)
	{
		if(handle == last_timer_raw)
			last_timer_raw--;
		timers[handle].state = TIMER_FREE;
	}
	return TIMER_NONE;
}

/*!
** ------  TimeDispatch is called on each timer expiration ----
**
**/
int tdcount=0;


/*1.遍历所有定时器
  2.每个定时器：若是需触发就触发
               不要就减去此次调用耗费的时间
  3.记录最早需要下一次唤醒的剩余时间val并将其记录在next_wakeup
    设置定时器setTimer(next_wakeup)为最近一次需要触发的时间
*/
void TimeDispatch(void)   
{
    TIMER_HANDLE i;  //循环计数器，用于遍历所有定时器
    TIMEVAL next_wakeup = TIMEVAL_MAX; 
	/*将唤醒时间设为最大值，便于后边与定时器剩余时间比较
	即找出下次最早需要唤醒的定时器*/

	/*计算从上次setTimer调用到当前时刻的时间间隔即overrun
	即系统睡过头的时间
	当前时间-上次setTimer调用的时间
	*/
    UNS32 overrun = (UNS32)getElapsedTime();

    //没有进入中断的时间成为睡眠时间，即计划睡眠时间
	//进入中断的时间为非睡眠时间
	//真正的睡眠时间=计划睡眠时间+系统睡过头的时间
    TIMEVAL real_total_sleep_time = total_sleep_time + overrun;

	//创建一个指针，用于遍历定时器数组
    s_timer_entry *row;

    // timers是软件定时器数组，包含了所有软件定时器
	//遍历所有定时器条目
    for(i=0, row = timers; i <= last_timer_raw; i++, row++)
    {
        // 如果定时器处于激活状态
        if (row->state & TIMER_ARMED)
        {
            // 如果定时器到期（需要触发）  row->val定时器剩余时间
            if (row->val <= real_total_sleep_time)
            {
                // 如果不是周期定时器（只触发一次）   row->interval定时器周期
                if (!row->interval)
                {
                    row->state = TIMER_TRIG; // 标记为触发
                }
                else // 周期定时器，到期后重新设置
                {
					row->state = TIMER_TRIG_PERIOD; // 标记为周期触发

					// 重新设置定时器值，考虑溢出修正
					/*(overrun % (UNS32)row->interval)是要计算得出晚掉的的时间
					overrun了205秒，interval是100秒，205%100=5，即晚掉了5秒
					从周期中再减去晚掉的时间，就是下次定时器唤醒的时间
					将定时器对齐，避免了累积误差*/
                    row->val = row->interval - (overrun % (UNS32)row->interval);
                   
                    //寻找最近需要唤醒的定时器并将其设置为下次唤醒时间
                    if(row->val < next_wakeup)
                        next_wakeup = row->val;
                }
            }
            else
            {
                // 定时器还没到期，递减剩余时间
                row->val -= real_total_sleep_time;// a=a-b/a-=b

                //寻找最近需要唤醒的定时器并将其设置为下次唤醒时间
                if(row->val < next_wakeup)
                    next_wakeup = row->val;
            }
        }
    }

    // 记录下次需要休眠的时间
    total_sleep_time = next_wakeup;

    // 设置定时器为最近一次需要唤醒的时间
    setTimer(next_wakeup);

    // 再次遍历所有定时器条目，触发需要触发的定时器
    for(i=0, row = timers; i<=last_timer_raw; i++, row++)
    {
        if (row->state & TIMER_TRIG)
		//&是按位与运算符，判断定时器状态是否为触发状态,都为1才为1
        {
            row->state &= ~TIMER_TRIG; // 清除触发标志（如果不是周期定时器则变为FREE）
			//取反后按位与赋值，即 row->state = row->state & ~TIMER_TRIG;
            if(row->callback)
                (*row->callback)(row->d, row->id); // 执行回调函数
        }
    }
}