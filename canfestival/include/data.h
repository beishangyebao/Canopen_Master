#ifndef __data_h__
#define __data_h__

#ifdef __cplusplus
extern "C" {
#endif

/* declaration of CO_Data type let us include all necessary headers
 struct struct_CO_Data can then be defined later
 */
typedef struct struct_CO_Data CO_Data;

#include "applicfg.h"
#include "def.h"
#include "can.h"
#include "objdictdef.h"
#include "objacces.h"
#include "sdo.h"
#include "pdo.h"
#include "states.h"
#include "lifegrd.h"
#include "sync.h"
#include "nmtSlave.h"
#include "nmtMaster.h"
#include "emcy.h"
#ifdef CO_ENABLE_LSS
#include "lss.h"
#endif

/**
 * @ingroup od
 * @brief This structure contains all necessary informations to define a CANOpen node 
 */
struct struct_CO_Data {
	/* Object dictionary */
	UNS8 *bDeviceNodeId;
	const indextable *objdict;
	s_PDO_status *PDO_status;
	TIMER_HANDLE *RxPDO_EventTimers;
	void (*RxPDO_EventTimers_Handler)(CO_Data*, UNS32);
	const quick_index *firstIndex;
	const quick_index *lastIndex;
	const UNS16 *ObjdictSize;
	const UNS8 *iam_a_slave;
	valueRangeTest_t valueRangeTest;
	
	/* SDO */
	s_transfer transfers[SDO_MAX_SIMULTANEOUS_TRANSFERS];
	/* s_sdo_parameter *sdo_parameters; */

	/* State machine */
	e_nodeState nodeState;
	s_state_communication CurrentCommunicationState;
	initialisation_t initialisation;
	preOperational_t preOperational;
	operational_t operational;
	stopped_t stopped;
     void (*NMT_Slave_Node_Reset_Callback)(CO_Data*);
     void (*NMT_Slave_Communications_Reset_Callback)(CO_Data*);
     
	/* NMT-heartbeat */
	UNS8 *ConsumerHeartbeatCount;
	UNS32 *ConsumerHeartbeatEntries;
	TIMER_HANDLE *ConsumerHeartBeatTimers;
	UNS16 *ProducerHeartBeatTime;
	TIMER_HANDLE ProducerHeartBeatTimer;
	heartbeatError_t heartbeatError;
	e_nodeState NMTable[NMT_MAX_NODE_ID]; 

	/* NMT-nodeguarding */
	TIMER_HANDLE GuardTimeTimer;
	TIMER_HANDLE LifeTimeTimer;
	nodeguardError_t nodeguardError;
	UNS16 *GuardTime;
	UNS8 *LifeTimeFactor;
	UNS8 nodeGuardStatus[NMT_MAX_NODE_ID];

	/* SYNC */
	TIMER_HANDLE syncTimer;
	UNS32 *COB_ID_Sync;
	UNS32 *Sync_Cycle_Period;
	/*UNS32 *Sync_window_length;;*/
	post_sync_t post_sync;
	post_TPDO_t post_TPDO;
	post_SlaveBootup_t post_SlaveBootup;
    post_SlaveStateChange_t post_SlaveStateChange;
	
	/* General */
	UNS8 toggle;
	CAN_PORT canHandle;	
	scanIndexOD_t scanIndexOD;
	storeODSubIndex_t storeODSubIndex; 
	
	/* DCF concise */
    const indextable* dcf_odentry;
	UNS8* dcf_cursor;
	UNS32 dcf_entries_count;
	UNS8 dcf_status;
    UNS32 dcf_size;
    UNS8* dcf_data;
	
	/* EMCY */
	e_errorState error_state;
	UNS8 error_history_size;
	UNS8* error_number;
	UNS32* error_first_element;
	UNS8* error_register;
    UNS32* error_cobid;
	s_errors error_data[EMCY_MAX_ERRORS];
	post_emcy_t post_emcy;
	
#ifdef CO_ENABLE_LSS
	/* LSS */
	lss_transfer_t lss_transfer;
	lss_StoreConfiguration_t lss_StoreConfiguration;
#endif	
};

#define NMTable_Initializer Unknown_state,
#define nodeGuardStatus_Initializer 0x00,

#ifdef SDO_DYNAMIC_BUFFER_ALLOCATION
#define s_transfer_Initializer {\
		0,          /* CliServNbr */\
		0,          /* wohami */\
		SDO_RESET,  /* state */\
		0,          /* toggle */\
		0,          /* abortCode */\
		0,          /* index */\
		0,          /* subIndex */\
		0,          /* count */\
		0,          /* offset */\
		{0},        /* data (static use, so that all the table is initialize at 0)*/\
    NULL,       /* dynamicData */ \
    0,          /* dynamicDataSize */ \
		0,          /* peerCRCsupport */\
		0,          /* blksize */\
		0,          /* ackseq */\
		0,          /* objsize */\
		0,          /* lastblockoffset */\
		0,          /* seqno */\
		0,          /* endfield */\
		RXSTEP_INIT,/* rxstep */\
		{0},        /* tmpData */\
		0,          /* dataType */\
		-1,         /* timer */\
		NULL        /* Callback */\
	  },
#else
#define s_transfer_Initializer {\
		0,          /* CliServNbr */\
		0,          /* wohami */\
		SDO_RESET,  /* state */\
		0,          /* toggle */\
		0,          /* abortCode */\
		0,          /* index */\
		0,          /* subIndex */\
		0,          /* count */\
		0,          /* offset */\
		{0},        /* data (static use, so that all the table is initialize at 0)*/\
		0,          /* peerCRCsupport */\
		0,          /* blksize */\
		0,          /* ackseq */\
		0,          /* objsize */\
		0,          /* lastblockoffset */\
		0,          /* seqno */\
		0,          /* endfield */\
		RXSTEP_INIT,/* rxstep */\
		{0},        /* tmpData */\
		0,          /*  */\
		-1,         /*  */\
		NULL        /*  */\
	  },
#endif //SDO_DYNAMIC_BUFFER_ALLOCATION

#define ERROR_DATA_INITIALIZER \
	{\
	0, /* errCode */\
	0, /* errRegMask */\
	0 /* active */\
	},
	
#ifdef CO_ENABLE_LSS

#ifdef CO_ENABLE_LSS_FS	
#define lss_fs_Initializer \
		,0,						/* IDNumber */\
  		128, 					/* BitChecked */\
  		0,						/* LSSSub */\
  		0, 						/* LSSNext */\
  		0, 						/* LSSPos */\
  		LSS_FS_RESET,			/* FastScan_SM */\
  		-1,						/* timerFS */\
  		{{0,0,0,0},{0,0,0,0}}   /* lss_fs_transfer */
#else
#define lss_fs_Initializer
#endif		

#define lss_Initializer {\
		LSS_RESET,  			/* state */\
		0,						/* command */\
		LSS_WAITING_MODE, 		/* mode */\
		0,						/* dat1 */\
		0,						/* dat2 */\
		0,          			/* NodeID */\
		0,          			/* addr_sel_match */\
		0,          			/* addr_ident_match */\
		"none",         		/* BaudRate */\
		0,          			/* SwitchDelay */\
		SDELAY_OFF,   			/* SwitchDelayState */\
		NULL,					/* canHandle_t */\
		-1,						/* TimerMSG */\
		-1,          			/* TimerSDELAY */\
		NULL,        			/* Callback */\
		0						/* LSSanswer */\
		lss_fs_Initializer		/*FastScan service initialization */\
	  },\
	  NULL 	/* _lss_StoreConfiguration*/
#else
#define lss_Initializer
#endif


/* 静态初始化一个CANopen节点的数据结构 */
/* 预处理器宏
把一大串值按顺序填成一个复合初始化器，用于初始化一个节点运行的时数据结构
*/
#define CANOPEN_NODE_DATA_INITIALIZER(NODE_PREFIX) {\
	/* Object dictionary/基本指针和 OD 相关*/\
	& NODE_PREFIX ## _bDeviceNodeId,     /*取地址，指向此节点的节点号变量*/\
	NODE_PREFIX ## _objdict,             /*对象字典表的数组/指针，本节点的 OD*/\
	NODE_PREFIX ## _PDO_status,          /*PDO状态结构或数组，管理PDO的传输状态*/\
	NULL,                                /*接收PDO的事件定时器数组指针 未提供*/\
	_RxPDO_EventTimers_Handler,          /*若有定时器到期，调用的处理函数指针*/\
	& NODE_PREFIX ## _firstIndex,        /*OD 索引范围与大小指针*/\
	& NODE_PREFIX ## _lastIndex,         /*同上*/\
	& NODE_PREFIX ## _ObjdictSize,       /*同上*/\
	& NODE_PREFIX ## _iam_a_slave,       /*从机*/\
	NODE_PREFIX ## _valueRangeTest,      /*值范围检查函数或表*/\
	\
	/* SDO传输数组结构初始化*/\
	{\
          REPEAT_SDO_MAX_SIMULTANEOUS_TRANSFERS_TIMES(s_transfer_Initializer)\
	},\
	\
	/*状态机（NMT/状态回调）*/\
	Unknown_state,      /*节点初始状态*/\
	/*各子状态标志字段*/\
	{\
		0,          /*Boot_Up启动状态*/\
		0,          /*SDO服务数据对象状态*/\
		0,          /*Emergency紧急消息状态*/\
		0,          /*SYNC同步消息状态*/\
		0,          /*Heartbeat心跳消息状态*/\
		0,           /*PDO过程数据对象状态*/\
		0           /*LSS层设置服务状态*/\
	},\
	_initialisation,     /*初始化状态，设备上电或复位后进入此状态，进行初始化操作*/\
	_preOperational,     /*预运行状态，设备已初始化完成，但还未进入正常运行状态，通常用于配置和参数设置*/\
	_operational,        /*运行状态，设备正常运行，可以执行其主要功能*/\
	_stopped,            /*停止状态，设备停止运行，但保持通信能力*/\
	NULL,                /*NMT节点复位回调，此处为空表示没有定义回调函数*/\
	NULL,                /*NMT通信复位回调，此处为空表示没有定义回调函数*/\
	\
	/* NMT-heartbeat  NMT心跳机制*/\
	& NODE_PREFIX ## _highestSubIndex_obj1016, /*消费者心跳计数
	                                           用于存储该节点需要监控的消费者心跳数量
											   对象字典1016定义了该节点需要监控哪些其他节点的心跳*/\
	NODE_PREFIX ## _obj1016,                   /*消费者心跳条目
	                                           1016的实际数据结构
											   存储了所有需要监控的消费者心跳配置信息
											   包括每个被监控节点的ID和期望的心跳时间*/\
	NODE_PREFIX ## _heartBeatTimers,           /*消费者心跳定时器
	                                           用于跟踪每个消费者心跳的超时时间
											   当接收到对应节点的心跳报文时，定时器会重置
											   如果定时器超时，说明对应节点可能已经离线*/\
	& NODE_PREFIX ## _obj1017,                 /*生产者心跳时间
	                                           指向对象1017的指针
											   定义了该节点作为生产者发送心跳报文的周期
											   其他节点会根据这个时间来监控本节点的状态*/\
	TIMER_NONE,                                /*生产者心跳定时器
	                                           用于控制本节点发送心跳报文的定时器
											   当节点进入运行状态后，会根据obj1017配置的时间启动定时器*/\
	_heartbeatError,           /*心跳错误处理*/\
	\
	{REPEAT_NMT_MAX_NODE_ID_TIMES(NMTable_Initializer)},\
    /*初始化NMT状态表，用于跟踪网络中所有节点的状态*/\
	\
	/* NMT-nodeguarding，节点监控*/\
	TIMER_NONE,                                /*守护时间计时器初始为无*/\
	TIMER_NONE,                                /*生命周期计时器初始为无*/\
	_nodeguardError,           /*节点监控错误处理函数*/\
	& NODE_PREFIX ## _obj100C,                 /*指向守护时间对象(100Ch)*/\
	& NODE_PREFIX ## _obj100D,                 /*指向生命周期因子对象(100Dh)*/\
	{REPEAT_NMT_MAX_NODE_ID_TIMES(nodeGuardStatus_Initializer)},\
	/*初始化节点监控状态数组，用于跟踪网络中所有节点的监控状态*/\
	\
	/*配置SYNC同步消息*/\
	TIMER_NONE,                                /*同步计时器初始为无*/\
	& NODE_PREFIX ## _obj1005,                 /*指向同步COB-ID对象(1005h)*/\
	& NODE_PREFIX ## _obj1006,                 /*指向同步周期对象(1006h)*/\
	/*& NODE_PREFIX ## _obj1007, */            /*同步窗口长度*/\
	_post_sync,                 /*同步消息后处理函数*/\
	_post_TPDO,                 /*TPDO发送后处理函数*/\
	_post_SlaveBootup,			/*从节点启动后处理函数*/\
  _post_SlaveStateChange,			/*从节点状态改变后处理函数*/\
	\
	/*通用配置*/\
	0,                                         /* 切换标志，初始为0*/\
	NULL,                   /* CAN发送函数指针*/\
	NODE_PREFIX ## _scanIndexOD,                /*对象字典扫描函数*/\
	_storeODSubIndex,                /* 对象字典子索引存储函数*/\
	\
    /* DCF配置 */\
    NULL,       /*DCF对象字典条目*/\
	NULL,		/*DCF游标*/\
	1,		/*DCF条目计数*/\
	0,		/*DCF状态*/\
    0,      /*DCF大小*/\
    NULL,   /*DCF数据*/\
	\
	/* EMCY 配置紧急情况处理*/\
	Error_free,                      /* 错误状态初始为无错误*/\
	sizeof(NODE_PREFIX ## _obj1003) / sizeof(NODE_PREFIX ## _obj1003[0]),/*错误历史记录大小*/\
	& NODE_PREFIX ## _highestSubIndex_obj1003,    /*错误数量指针*/\
	& NODE_PREFIX ## _obj1003[0],    /*第一个错误元素指针*/\
	& NODE_PREFIX ## _obj1001,       /*错误寄存器指针*/\
    & NODE_PREFIX ## _obj1014,       /*紧急COB-ID指针*/\
	/* error_data: structure s_errors */\
	{\
	REPEAT_EMCY_MAX_ERRORS_TIMES(ERROR_DATA_INITIALIZER)\
	},\
	/*错误数据初始化 */\
	_post_emcy,              /* 紧急消息后处理函数*/\
	/* LSS初始化配置*/\
	lss_Initializer\
}

#ifdef __cplusplus
};
#endif

#endif /* __data_h__ */


