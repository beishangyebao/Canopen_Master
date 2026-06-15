

#include "canfestival_master.h"


/**************************************************************************/
/* Declaration of mapped variables - 接收从站反馈                          */
/**************************************************************************/
UNS16  recv_statusword = 0;          /* 从站状态字 */
INTEGER32  recv_actual_pos = 0;      /* 从站实际位置 */
INTEGER32  recv_actual_vel = 0;      /* 从站实际速度 */
INTEGER16  recv_actual_torque = 0;   /* 从站实际力矩 */




/**************************************************************************/
/* Declaration of value range types                                       */
/**************************************************************************/

#define valueRange_EMC 0x9F /* Type for index 0x1003 subindex 0x00 (only set of value 0 is possible) */
UNS32 Master_valueRangeTest (UNS8 typeValue, void * value)
{
  switch (typeValue) {
    case valueRange_EMC:
      if (*(UNS8*)value != (UNS8)0) return OD_VALUE_RANGE_EXCEEDED;
      break;
  }
  return 0;
}

/**************************************************************************/
/*                                 节点id                                 */

UNS8 Master_bDeviceNodeId = 0x00;


/**************************************************************************/
/*                     主从设备标识，0表示为主设备                          */

const UNS8 Master_iam_a_slave = 0;


//心跳定时器数组，用于存储心跳定时器的句柄
TIMER_HANDLE Master_heartBeatTimers[1];


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

                             对象字典

$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/


/* index 0x1000 :   Device Type. */
                    UNS32 Master_obj1000 = 0x0;	/* 0 */
                    subindex Master_Index1000[] = 
                     {
                       { RO, uint32, sizeof (UNS32), (void*)&Master_obj1000, NULL }
                     };

/* index 0x1001 :   Error Register. */
                    UNS8 Master_obj1001 = 0x0;	/* 0 */
                    subindex Master_Index1001[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_obj1001, NULL }
                     };

/* index 0x1003 :   Pre-defined Error Field */
                    UNS8 Master_highestSubIndex_obj1003 = 0; /* number of subindex - 1*/
                    UNS32 Master_obj1003[] = 
                    {
                      0x0	/* 0 */
                    };
                    subindex Master_Index1003[] = 
                     {
                       { RO, valueRange_EMC, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1003, NULL },
                       { RO, uint32, sizeof (UNS32), (void*)&Master_obj1003[0], NULL }
                     };

/* index 0x1005 :   SYNC COB ID. */
                    UNS32 Master_obj1005 = 0x40000080;	/* 1073741952 */
                    subindex Master_Index1005[] = 
                     {
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1005, NULL }
                     };

/* index 0x1006 :   Communication / Cycle Period. */
                    UNS32 Master_obj1006 = 0;	//不发sync帧
                    subindex Master_Index1006[] = 
                     {
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1006, NULL }
                     };

/* index 0x1007 :   Synchronous Window Length. */
                    UNS32 Master_obj1007 = 0x1388;	/* 5000 */
                     subindex Master_Index1007[] = 
                     {
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1007, NULL }
                     };
 
/* index 0x100C :   Guard Time */ 
                    UNS16 Master_obj100C = 0x0;   /* 0 */

/* index 0x100D :   Life Time Factor */ 
                    UNS8 Master_obj100D = 0x0;   /* 0 */

/* index 0x1014 :   Emergency COB ID. */
                    UNS32 Master_obj1014 = 0x80;	/* 128 */
                    subindex Master_Index1014[] = 
                     {
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1014, NULL }
                     };

/* index 0x1016 :   Consumer Heartbeat Time */
                    UNS8 Master_highestSubIndex_obj1016 = 0;
                    UNS32 Master_obj1016[]={0};

/* index 0x1017 :   Producer Heartbeat Time */ 
                    UNS16 Master_obj1017 = 0x0;   /* 0 */

/* index 0x1018 :   Identity. */
                    UNS8 Master_highestSubIndex_obj1018 = 4; /* number of subindex - 1*/
                    UNS32 Master_obj1018_Vendor_ID = 0x0;	/* 0 */
                    UNS32 Master_obj1018_Product_Code = 0x0;	/* 0 */
                    UNS32 Master_obj1018_Revision_Number = 0x0;	/* 0 */
                    UNS32 Master_obj1018_Serial_Number = 0x0;	/* 0 */
                    subindex Master_Index1018[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1018, NULL },
                       { RO, uint32, sizeof (UNS32), (void*)&Master_obj1018_Vendor_ID, NULL },
                       { RO, uint32, sizeof (UNS32), (void*)&Master_obj1018_Product_Code, NULL },
                       { RO, uint32, sizeof (UNS32), (void*)&Master_obj1018_Revision_Number, NULL },
                       { RO, uint32, sizeof (UNS32), (void*)&Master_obj1018_Serial_Number, NULL }
                     };

/* index 0x1019 :   Synchronous counter overflow value. */
                    UNS8 Master_obj1019 = 0x4;	/* 4 */
                    subindex Master_Index1019[] = 
                     {
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1019, NULL }
                     };

/* index 0x1280 :   Client SDO 1 Parameter. */
                    UNS8 Master_highestSubIndex_obj1280 = 3; /* number of subindex - 1*/
                    UNS32 Master_obj1280_COB_ID_Client_to_Server_Transmit_SDO = 0x601;	/* 1537 */
                    UNS32 Master_obj1280_COB_ID_Server_to_Client_Receive_SDO = 0x581;	/* 1409 */
                    UNS8 Master_obj1280_Node_ID_of_the_SDO_Server = 0x1;	/* 1 */
                    subindex Master_Index1280[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1280, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1280_COB_ID_Client_to_Server_Transmit_SDO, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1280_COB_ID_Server_to_Client_Receive_SDO, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1280_Node_ID_of_the_SDO_Server, NULL }
                     };

/* index 0x1281 :   Client SDO 2 Parameter. */
                    UNS8 Master_highestSubIndex_obj1281 = 3; /* number of subindex - 1*/
                    UNS32 Master_obj1281_COB_ID_Client_to_Server_Transmit_SDO = 0x602;	/* 1538 */
                    UNS32 Master_obj1281_COB_ID_Server_to_Client_Receive_SDO = 0x582;	/* 1410 */
                    UNS8 Master_obj1281_Node_ID_of_the_SDO_Server = 0x2;	/* 2 */
                    subindex Master_Index1281[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1281, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1281_COB_ID_Client_to_Server_Transmit_SDO, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1281_COB_ID_Server_to_Client_Receive_SDO, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1281_Node_ID_of_the_SDO_Server, NULL }
                     };

/* index 0x1400 :   Receive PDO 1 Parameter. 接收从站TPDO1*/
                    UNS8 Master_highestSubIndex_obj1400 = 6; /* number of subindex - 1*/
                    UNS32 Master_obj1400_COB_ID_used_by_PDO = 0x181;	
                    UNS8 Master_obj1400_Transmission_Type = 0xFF;	
                    UNS16 Master_obj1400_Inhibit_Time = 0x0;	/* 0 */
                    UNS8 Master_obj1400_Compatibility_Entry = 0x0;	/* 0 */
                    UNS16 Master_obj1400_Event_Timer = 0x0;	/* 0 */
                    UNS8 Master_obj1400_SYNC_start_value = 0x0;	/* 0 */
                    subindex Master_Index1400[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1400, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1400_COB_ID_used_by_PDO, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1400_Transmission_Type, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1400_Inhibit_Time, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1400_Compatibility_Entry, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1400_Event_Timer, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1400_SYNC_start_value, NULL }
                     };

/* index 0x1401 :   Receive PDO 2 Parameter. 接收从站TPDO2*/
                    UNS8 Master_highestSubIndex_obj1401 = 6; /* number of subindex - 1*/
                    UNS32 Master_obj1401_COB_ID_used_by_PDO = 0x281;	
                    UNS8 Master_obj1401_Transmission_Type = 0xFF;	/* 0 */
                    UNS16 Master_obj1401_Inhibit_Time = 0x0;	/* 0 */
                    UNS8 Master_obj1401_Compatibility_Entry = 0x0;	/* 0 */
                    UNS16 Master_obj1401_Event_Timer = 0x0;	/* 0 */
                    UNS8 Master_obj1401_SYNC_start_value = 0x0;	/* 0 */
                    subindex Master_Index1401[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1401, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1401_COB_ID_used_by_PDO, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1401_Transmission_Type, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1401_Inhibit_Time, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1401_Compatibility_Entry, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1401_Event_Timer, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1401_SYNC_start_value, NULL }
                     };

/* index 0x1600 :   Receive PDO 1 Mapping. 状态字(16) + 实际速度(32)*/
                    UNS8 Master_highestSubIndex_obj1600 = 2; /* number of subindex - 1*/
                    UNS32 Master_obj1600[] = 
                    {
                      0x20100010,  // 2010:00, 16位 = recv_statusword
                      0x20120020  // 2012:00, 32位 = recv_actual_vel
                    };
                    subindex Master_Index1600[] = 
                     {
                       { RW, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1600, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1600[0], NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1600[1], NULL }
                     };

/* index 0x1601 :   Receive PDO 2 Mapping. 实际位置(32) + 实际力矩(16) */
                    UNS8 Master_highestSubIndex_obj1601 = 2; /* number of subindex - 1*/
                    UNS32 Master_obj1601[] = 
                    {
                      0x20110020,   // 2011:00, 32位 = recv_actual_pos
                      0x20130010   // 2013:00, 16位 = recv_actual_torque
                    };
                    subindex Master_Index1601[] = 
                     {
                       { RW, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1601, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1601[0], NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1601[1], NULL }
                     };

/* index 0x1800 :   Transmit PDO 1 Parameter */
// 该索引用于配置PDO1传输的参数
                    //00h子索引，代表有效子索引个数
                    UNS8 Master_highestSubIndex_obj1800 = 6; 
                    //01h 代表发送的ID号
                    UNS32 Master_obj1800_COB_ID_used_by_PDO = 0x80000201;	
                    //02h 代表发送类型 异步发送
                    UNS8 Master_obj1800_Transmission_Type = 0xFE;
                    //03h 抑制时间=两次PDO发送之间的最小时间间隔（单位 100 μs）	,0x0表示不限制
                    UNS16 Master_obj1800_Inhibit_Time = 0x0;	
                    //04h 保留
                    UNS8 Master_obj1800_Compatibility_Entry = 0x0;
                    //05h 触发时间，周期模式下有效，单位ms
                    UNS16 Master_obj1800_Event_Timer = 0x0;
                    //06h 同步帧的起始个数，用于同步帧模式下多个PDO错开发送
                    UNS8 Master_obj1800_SYNC_start_value = 0x0;
                    /*把以上定义好的变量都放入到一个数组里面，数组类型为subindex
                    （子索引类型的结构体）
                    即这个数组的每个元素都是一个子索引
                    */	
                    subindex Master_Index1800[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1800, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1800_COB_ID_used_by_PDO, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1800_Transmission_Type, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1800_Inhibit_Time, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1800_Compatibility_Entry, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1800_Event_Timer, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1800_SYNC_start_value, NULL }
                     };

/* index 0x1801 :   Transmit PDO 2 Parameter. */
                    UNS8 Master_highestSubIndex_obj1801 = 6; /* number of subindex - 1*/
                    UNS32 Master_obj1801_COB_ID_used_by_PDO = 0x80000301;	/* 769 */
                    UNS8 Master_obj1801_Transmission_Type = 0x2;	/* 2 */
                    UNS16 Master_obj1801_Inhibit_Time = 0x0;	/* 0 */
                    UNS8 Master_obj1801_Compatibility_Entry = 0x0;	/* 0 */
                    UNS16 Master_obj1801_Event_Timer = 1000;	/* 0 */ //1s发送一次
                    UNS8 Master_obj1801_SYNC_start_value = 0x0;	/* 0 */
                    subindex Master_Index1801[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1801, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1801_COB_ID_used_by_PDO, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1801_Transmission_Type, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1801_Inhibit_Time, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1801_Compatibility_Entry, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1801_Event_Timer, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1801_SYNC_start_value, NULL }
                     };

/* index 0x1802 :   Transmit PDO 3 Parameter. */
                    UNS8 Master_highestSubIndex_obj1802 = 6; /* number of subindex - 1*/
                    UNS32 Master_obj1802_COB_ID_used_by_PDO = 0x80000401;	/* 1025 */
                    UNS8 Master_obj1802_Transmission_Type = 0x2;	/* 2 */
                    UNS16 Master_obj1802_Inhibit_Time = 0x0;	/* 0 */
                    UNS8 Master_obj1802_Compatibility_Entry = 0x0;	/* 0 */
                    UNS16 Master_obj1802_Event_Timer = 0x0;	/* 0 */
                    UNS8 Master_obj1802_SYNC_start_value = 0x0;	/* 0 */
                    subindex Master_Index1802[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1802, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1802_COB_ID_used_by_PDO, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1802_Transmission_Type, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1802_Inhibit_Time, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1802_Compatibility_Entry, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1802_Event_Timer, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1802_SYNC_start_value, NULL }
                     };

/* index 0x1803 :   Transmit PDO 4 Parameter. */
                    UNS8 Master_highestSubIndex_obj1803 = 6; /* number of subindex - 1*/
                    UNS32 Master_obj1803_COB_ID_used_by_PDO = 0x80000202;	/* 514 */
                    UNS8 Master_obj1803_Transmission_Type = 0x2;	/* 2 */
                    UNS16 Master_obj1803_Inhibit_Time = 0x0;	/* 0 */
                    UNS8 Master_obj1803_Compatibility_Entry = 0x0;	/* 0 */
                    UNS16 Master_obj1803_Event_Timer = 0x0;	/* 0 */
                    UNS8 Master_obj1803_SYNC_start_value = 0x0;	/* 0 */
                    subindex Master_Index1803[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1803, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1803_COB_ID_used_by_PDO, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1803_Transmission_Type, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1803_Inhibit_Time, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1803_Compatibility_Entry, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1803_Event_Timer, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1803_SYNC_start_value, NULL }
                     };

/* index 0x1804 :   Transmit PDO 5 Parameter. */
                    UNS8 Master_highestSubIndex_obj1804 = 6; /* number of subindex - 1*/
                    UNS32 Master_obj1804_COB_ID_used_by_PDO = 0x80000302;	/* 770 */
                    UNS8 Master_obj1804_Transmission_Type = 0x2;	/* 2 */
                    UNS16 Master_obj1804_Inhibit_Time = 0x0;	/* 0 */
                    UNS8 Master_obj1804_Compatibility_Entry = 0x0;	/* 0 */
                    UNS16 Master_obj1804_Event_Timer = 0x0;	/* 0 */
                    UNS8 Master_obj1804_SYNC_start_value = 0x0;	/* 0 */
                    subindex Master_Index1804[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1804, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1804_COB_ID_used_by_PDO, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1804_Transmission_Type, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1804_Inhibit_Time, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1804_Compatibility_Entry, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1804_Event_Timer, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1804_SYNC_start_value, NULL }
                     };

/* index 0x1805 :   Transmit PDO 6 Parameter. */
                    UNS8 Master_highestSubIndex_obj1805 = 6; /* number of subindex - 1*/
                    UNS32 Master_obj1805_COB_ID_used_by_PDO = 0x80000402;	/* 1026 */
                    UNS8 Master_obj1805_Transmission_Type = 0x2;	/* 2 */
                    UNS16 Master_obj1805_Inhibit_Time = 0x0;	/* 0 */
                    UNS8 Master_obj1805_Compatibility_Entry = 0x0;	/* 0 */
                    UNS16 Master_obj1805_Event_Timer = 0x0;	/* 0 */
                    UNS8 Master_obj1805_SYNC_start_value = 0x0;	/* 0 */
                    subindex Master_Index1805[] = 
                     {
                       { RO, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1805, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1805_COB_ID_used_by_PDO, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1805_Transmission_Type, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1805_Inhibit_Time, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1805_Compatibility_Entry, NULL },
                       { RW, uint16, sizeof (UNS16), (void*)&Master_obj1805_Event_Timer, NULL },
                       { RW, uint8, sizeof (UNS8), (void*)&Master_obj1805_SYNC_start_value, NULL }
                     };

/* index 0x1A00 :   Transmit PDO 1 Mapping. */
//PDO映射参数，决定了PDO映射的是速度还是未知
                    UNS8 Master_highestSubIndex_obj1A00 = 0; //发送数据的个数
                    UNS32 Master_obj1A00[] = 
                    {
                     0
                    };
                    subindex Master_Index1A00[] = 
                     {
                       { RW, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1A00, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1A00[0], NULL }
                     };

/* index 0x1A01 :   Transmit PDO 2 Mapping. */
                    UNS8 Master_highestSubIndex_obj1A01 = 1; /* number of subindex - 1*/
                    UNS32 Master_obj1A01[] = 
                    {
                      0x20010020	/* 536936480 */
                    };
                    subindex Master_Index1A01[] = 
                     {
                       { RW, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1A01, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1A01[0], NULL }
                     };

/* index 0x1A02 :   Transmit PDO 3 Mapping. */
                    UNS8 Master_highestSubIndex_obj1A02 = 1; /* number of subindex - 1*/
                    UNS32 Master_obj1A02[] = 
                    {
                      0x20020010	/* 537002000 */
                    };
                    subindex Master_Index1A02[] = 
                     {
                       { RW, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1A02, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1A02[0], NULL }
                     };

/* index 0x1A03 :   Transmit PDO 4 Mapping. */
                    UNS8 Master_highestSubIndex_obj1A03 = 1; /* number of subindex - 1*/
                    UNS32 Master_obj1A03[] = 
                    {
                      0x20030008	/* 537067528 */
                    };
                    subindex Master_Index1A03[] = 
                     {
                       { RW, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1A03, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1A03[0], NULL }
                     };

/* index 0x1A04 :   Transmit PDO 5 Mapping. */
                    UNS8 Master_highestSubIndex_obj1A04 = 1; /* number of subindex - 1*/
                    UNS32 Master_obj1A04[] = 
                    {
                      0x20040020	/* 537133088 */
                    };
                    subindex Master_Index1A04[] = 
                     {
                       { RW, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1A04, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1A04[0], NULL }
                     };

/* index 0x1A05 :   Transmit PDO 6 Mapping. */
                    UNS8 Master_highestSubIndex_obj1A05 = 1; /* number of subindex - 1*/
                    UNS32 Master_obj1A05[] = 
                    {
                      0x20050010	/* 537198608 */
                    };
                    subindex Master_Index1A05[] = 
                     {
                       { RW, uint8, sizeof (UNS8), (void*)&Master_highestSubIndex_obj1A05, NULL },
                       { RW, uint32, sizeof (UNS32), (void*)&Master_obj1A05[0], NULL }
                     };


/* index 0x2010 :   Received Statusword from Slave */
                    subindex Master_Index2010[] = 
                    {
                        { RW, uint16, sizeof (UNS16), (void*)&recv_statusword, NULL }
                    };

/* index 0x2011 :   Received Actual Position from Slave */
                    subindex Master_Index2011[] = 
                    {
                        { RW, int32, sizeof (INTEGER32), (void*)&recv_actual_pos, NULL }
                    };

/* index 0x2012 :   Received Actual Velocity from Slave */
                    subindex Master_Index2012[] = 
                    {
                        { RW, int32, sizeof (INTEGER32), (void*)&recv_actual_vel, NULL }
                    };

/* index 0x2013 :   Received Actual Torque from Slave */
                    subindex Master_Index2013[] = 
                    {
                        { RW, int16, sizeof (INTEGER16), (void*)&recv_actual_torque, NULL }
                    };




/**************************************************************************/
/* 为便于管理，把上面每一个数组都放入到一个新的数组里面
   最后为索引条目                                      */
/**************************************************************************/

const indextable Master_objdict[] = 
{
  { (subindex*)Master_Index1000,sizeof(Master_Index1000)/sizeof(Master_Index1000[0]), 0x1000},
  { (subindex*)Master_Index1001,sizeof(Master_Index1001)/sizeof(Master_Index1001[0]), 0x1001},
  { (subindex*)Master_Index1005,sizeof(Master_Index1005)/sizeof(Master_Index1005[0]), 0x1005},
  { (subindex*)Master_Index1006,sizeof(Master_Index1006)/sizeof(Master_Index1006[0]), 0x1006},
  { (subindex*)Master_Index1007,sizeof(Master_Index1007)/sizeof(Master_Index1007[0]), 0x1007},
  { (subindex*)Master_Index1014,sizeof(Master_Index1014)/sizeof(Master_Index1014[0]), 0x1014},
  { (subindex*)Master_Index1018,sizeof(Master_Index1018)/sizeof(Master_Index1018[0]), 0x1018},
  { (subindex*)Master_Index1019,sizeof(Master_Index1019)/sizeof(Master_Index1019[0]), 0x1019},
  { (subindex*)Master_Index1280,sizeof(Master_Index1280)/sizeof(Master_Index1280[0]), 0x1280},
  { (subindex*)Master_Index1281,sizeof(Master_Index1281)/sizeof(Master_Index1281[0]), 0x1281},
  { (subindex*)Master_Index1400,sizeof(Master_Index1400)/sizeof(Master_Index1400[0]), 0x1400},
  { (subindex*)Master_Index1401,sizeof(Master_Index1401)/sizeof(Master_Index1401[0]), 0x1401},
  { (subindex*)Master_Index1600,sizeof(Master_Index1600)/sizeof(Master_Index1600[0]), 0x1600},
  { (subindex*)Master_Index1601,sizeof(Master_Index1601)/sizeof(Master_Index1601[0]), 0x1601},
  { (subindex*)Master_Index1800,sizeof(Master_Index1800)/sizeof(Master_Index1800[0]), 0x1800},
  { (subindex*)Master_Index1801,sizeof(Master_Index1801)/sizeof(Master_Index1801[0]), 0x1801},
  { (subindex*)Master_Index1802,sizeof(Master_Index1802)/sizeof(Master_Index1802[0]), 0x1802},
  { (subindex*)Master_Index1803,sizeof(Master_Index1803)/sizeof(Master_Index1803[0]), 0x1803},
  { (subindex*)Master_Index1804,sizeof(Master_Index1804)/sizeof(Master_Index1804[0]), 0x1804},
  { (subindex*)Master_Index1805,sizeof(Master_Index1805)/sizeof(Master_Index1805[0]), 0x1805},
  { (subindex*)Master_Index1A00,sizeof(Master_Index1A00)/sizeof(Master_Index1A00[0]), 0x1A00},
  { (subindex*)Master_Index1A01,sizeof(Master_Index1A01)/sizeof(Master_Index1A01[0]), 0x1A01},
  { (subindex*)Master_Index1A02,sizeof(Master_Index1A02)/sizeof(Master_Index1A02[0]), 0x1A02},
  { (subindex*)Master_Index1A03,sizeof(Master_Index1A03)/sizeof(Master_Index1A03[0]), 0x1A03},
  { (subindex*)Master_Index1A04,sizeof(Master_Index1A04)/sizeof(Master_Index1A04[0]), 0x1A04},
  { (subindex*)Master_Index1A05,sizeof(Master_Index1A05)/sizeof(Master_Index1A05[0]), 0x1A05},
  { (subindex*)Master_Index2010,sizeof(Master_Index2010)/sizeof(Master_Index2010[0]), 0x2010},
  { (subindex*)Master_Index2011,sizeof(Master_Index2011)/sizeof(Master_Index2011[0]), 0x2011},
  { (subindex*)Master_Index2012,sizeof(Master_Index2012)/sizeof(Master_Index2012[0]), 0x2012},
  { (subindex*)Master_Index2013,sizeof(Master_Index2013)/sizeof(Master_Index2013[0]), 0x2013},

};

const indextable * Master_scanIndexOD (CO_Data *d, UNS16 wIndex, UNS32 * errorCode)
{
	// 遍历索引表，根据wIndex的值，返回对应的索引条目
  // wIndex是根据需要访问的对象字典条目，由CANopen协议栈或应用程序根据需要访问的对象字典条目来给定的

	(void)d; //忽略传入的d参数
  
	// 遍历索引表，根据wIndex的值，返回对应的索引条目
	int i;
	// 根据wIndex的值，将i赋值为对应的索引值

	switch(wIndex){
		case 0x1000: i = 0;  break;
		case 0x1001: i = 1;  break;
		case 0x1005: i = 2;  break;
		case 0x1006: i = 3;  break;
		case 0x1007: i = 4;  break;
		case 0x1014: i = 5;  break;
		case 0x1018: i = 6;  break;
		case 0x1019: i = 7;  break;
		case 0x1280: i = 8;  break;
		case 0x1281: i = 9;  break;
		case 0x1400: i = 10; break;
		case 0x1401: i = 11; break;
		case 0x1600: i = 12; break;
		case 0x1601: i = 13; break;
		case 0x1800: i = 14; break;
		case 0x1801: i = 15; break;
		case 0x1802: i = 16; break;
		case 0x1803: i = 17; break;
		case 0x1804: i = 18; break;
		case 0x1805: i = 19; break;
		case 0x1A00: i = 20; break;
		case 0x1A01: i = 21; break;
		case 0x1A02: i = 22; break;
		case 0x1A03: i = 23; break;
		case 0x1A04: i = 24; break;
		case 0x1A05: i = 25; break;
    case 0x2010: i = 26; break;
    case 0x2011: i = 27; break;
    case 0x2012: i = 28; break;
    case 0x2013: i = 29; break;  
		// 如果wIndex的值不在上述范围内，则返回OD_NO_SUCH_OBJECT错误码，并返回NULL
		default:
			*errorCode = OD_NO_SUCH_OBJECT;
			return NULL;
	}
	// 如果wIndex的值在上述范围内，则返回OD_SUCCESSFUL错误码，并返回对应的索引值
	*errorCode = OD_SUCCESSFUL;
	return &Master_objdict[i];
}

//创建并初始化一个用于存储PDO状态的数组
s_PDO_status Master_PDO_status[6] = {s_PDO_status_Initializer,s_PDO_status_Initializer,s_PDO_status_Initializer,s_PDO_status_Initializer,s_PDO_status_Initializer,s_PDO_status_Initializer};

/* 索引个数判断
   会用在发送pdo,sdo等数据之前,进行检测.
   防止字典中没有定义相应的pdo或sdo参数等
*/

const quick_index Master_firstIndex = {
  0, /* SDO_SVR 用于访问 SDO 服务器对象的索引值*/
  8, /* SDO_CLT 用于访问 SDO 客户端对象的索引值*/
  10, /* PDO_RCV 用于访问 PDO 接收对象的索引值*/
  12, /* PDO_RCV_MAP 用于访问 PDO 接收映射对象的索引值*/
  14, /* PDO_TRS 用于访问 PDO 传输对象的索引值*/
  20 /* PDO_TRS_MAP 用于访问 PDO 传输映射对象的索引值*/
};

const quick_index Master_lastIndex = {
  0, /* SDO_SVR */
  9, /* SDO_CLT */
  11, /* PDO_RCV */
  13, /* PDO_RCV_MAP */
  19, /* PDO_TRS */
  25 /* PDO_TRS_MAP */
};

const UNS16 Master_ObjdictSize = sizeof(Master_objdict)/sizeof(Master_objdict[0]); 
//创建masterdata并初始化（不是声明），相当于Int a = 0;
//把字典的内容全部赋值给 Master_Data
//Master_Data是一个CO_Data类型的结构体，包含了CANopen节点的数据
CO_Data Master_Data = CANOPEN_NODE_DATA_INITIALIZER(Master);


