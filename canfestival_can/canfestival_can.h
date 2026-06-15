/**
	***********************************
	* 文件名:	CANOpen_can.h
	* 作者:		stone
	* 版本:		V0.1
	* 日期:		2018-3-29
	* 描述:		CANOPEN协议底层总线接口
	***********************************
	*/
#ifndef __canfestival_can_H_
#define __canfestival_can_H_

#include "stm32f10x.h"

/* 功能:	can总线配置
	 参数:	无
	 返回值:无
 */

u8 canfestival_can_init(u8 tsjw,u8 tbs2,u8 tbs1,u16 brp,u8 mode);

#endif

