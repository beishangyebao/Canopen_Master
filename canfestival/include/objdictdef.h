#ifndef __objdictdef_h__
#define __objdictdef_h__

/************************* CONSTANTES **********************************/
/** this are static defined datatypes taken fCODE the canopen standard. They
 *  are located at index 0x0001 to 0x001B. As described in the standard, they
 *  are in the object dictionary for definition purpose only. a device does not
 *  to support all of this datatypes.
 */
#define boolean         0x01
#define int8            0x02
#define int16           0x03
#define int32           0x04
#define uint8           0x05
#define uint16          0x06
#define uint32          0x07
#define real32          0x08
#define visible_string  0x09
#define octet_string    0x0A
#define unicode_string  0x0B
#define time_of_day     0x0C
#define time_difference 0x0D

#define domain          0x0F
#define int24           0x10
#define real64          0x11
#define int40           0x12
#define int48           0x13
#define int56           0x14
#define int64           0x15
#define uint24          0x16

#define uint40          0x18
#define uint48          0x19
#define uint56          0x1A
#define uint64          0x1B

#define pdo_communication_parameter 0x20
#define pdo_mapping                 0x21
#define sdo_parameter               0x22
#define identity                    0x23

/* CanFestival is using 0x24 to 0xFF to define some types containing a 
 value range (See how it works in objdict.c)
 */


/** Each entry of the object dictionary can be READONLY (RO), READ/WRITE (RW),
 *  WRITE-ONLY (WO)
 */
#define RW     0x00  
#define WO     0x01
#define RO     0x02

#define TO_BE_SAVE  0x04
#define DCF_TO_SEND 0x08

/************************ STRUCTURES ****************************/
/** This are some structs which are neccessary for creating the entries
 *  of the object dictionary.
 */
typedef struct td_indextable indextable;

typedef UNS32 (*ODCallback_t)(CO_Data* d, const indextable *, UNS8 bSubindex);

typedef struct td_subindex
{
    UNS8                    bAccessType;  //读写类型，是否可读可写
    UNS8                    bDataType; //数据类型
    UNS32                   size;      //数据值
    void*                   pObject;   //指针
	ODCallback_t            callback;  //回调函数
} subindex;

/* 用于在通信配置文件中创建项目字典的结构体
   该结构体包含了一个指向subindex的指针、子索引的数量和索引值。
   该结构体用于描述一个索引表，其中包含了多个子索引。
   该结构体的目的是为了在对象字典中定义一个索引表，
   该索引表可以包含多个子索引，每个子索引都有自己的访问类型、数据类型、大小和指针。    
 */
struct td_indextable
{
    subindex*   pSubindex;   //指向subindex的指针
    UNS8   bSubCount;   /* 此子索引的有效条目计数
                           这里的计数定义了已经使用了多少内存分配
                           这个内存不需要使用
                         */
    UNS16   index;
};

typedef struct s_quick_index{
	UNS16 SDO_SVR;
	UNS16 SDO_CLT;
	UNS16 PDO_RCV;
	UNS16 PDO_RCV_MAP;
	UNS16 PDO_TRS;
	UNS16 PDO_TRS_MAP;
}quick_index;


typedef const indextable * (*scanIndexOD_t)(CO_Data* d, UNS16 wIndex, UNS32 * errorCode);

/************************** MACROS *********************************/

/* CANopen usefull helpers */
#define GET_NODE_ID(m)         (UNS16_LE(m.cob_id) & 0x7f)
#define GET_FUNCTION_CODE(m)   (UNS16_LE(m.cob_id) >> 7)

#endif /* __objdictdef_h__ */
