#ifndef _CONFIG_H_
#define _CONFIG_H_


//#ifdef  __IAR_SYSTEMS_ICC__
//#include <ioavr.h>
//#include <intrinsics.h>
//#include "iar.h"
//#else	// GCC
//#include <inttypes.h>
//#include <avr/io.h>
//#include <avr/interrupt.h>
//#include <avr/pgmspace.h>
//#include <avr/sleep.h>
//#include <avr/wdt.h>
//#endif	// GCC

////#define WD_SLEEP
//// Needed defines by Atmel lib
//#define FOSC           8000        // 8 MHz External cristal
//#ifndef F_CPU
//#define F_CPU          (1000UL*FOSC) // Need for AVR GCC
//#endif
//#define CAN_BAUDRATE    125

// Needed defines by Canfestival lib
#define MAX_CAN_BUS_ID 1  // 最大CAN总线ID
#define SDO_MAX_LENGTH_TRANSFER 32  // SDO最大传输长度
#define SDO_BLOCK_SIZE 16  //
#define SDO_MAX_SIMULTANEOUS_TRANSFERS 1  //同时发送SDO的最大数量
#define NMT_MAX_NODE_ID 128  // NMT最大节点ID
#define SDO_TIMEOUT_MS 3000U    // SDO超时时间
#define MAX_NB_TIMER 8 // 最大定时器数量

// CANOPEN_BIG_ENDIAN is not defined
#define CANOPEN_LITTLE_ENDIAN 1

#define US_TO_TIMEVAL_FACTOR 8

#define REPEAT_SDO_MAX_SIMULTANEOUS_TRANSFERS_TIMES(repeat)\
repeat
#define REPEAT_NMT_MAX_NODE_ID_TIMES(repeat)\
repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat repeat

#define EMCY_MAX_ERRORS 8
#define REPEAT_EMCY_MAX_ERRORS_TIMES(repeat)\
repeat repeat repeat repeat repeat repeat repeat repeat


#endif /* _CONFIG_H_ */
