# Master 配合 DS402 状态机改造方案

## 1. 改造背景

slave 计划把 `0x6041 Statusword` 从“类 DS402 状态字”改成真正由 DS402 PDS finite state automaton 生成的状态字。

当前 master 和 slave 的 PDO 布局已经匹配：

- slave TPDO1 `0x181`：`0x6041 Statusword` + `0x606C Actual velocity`。
- slave TPDO2 `0x281`：`0x6064 Actual position` + `0x6077 Actual torque`。
- master 在 `canfestival_can.c` 中手动解析 TPDO1/TPDO2，并把状态字缓存到 `recv_statusword`。
- master 常规运动命令已经基本按 `0x0006 -> 0x0007 -> 0x000F` 发送控制字。

因此，slave 改造成真正状态机时，master 必须同步改造成 DS402 主站控制流程。PDO 映射和 COB-ID 保持不变，但 `6040/6041` 的交互方式必须改为“写控制字、等待状态字确认、再进入下一步”。

## 2. 当前 master 的风险点

当前 `motion_ctrl.c` 中的使能流程是连续盲发：

```c
canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, 0x0006, 2);
canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, 0x0007, 2);
canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, 0x000F, 2);
```

在旧 slave 下这能工作，因为 slave 主要看 `6040` 低 4 位是否为 `0x000F`。在严格 DS402 状态机下，风险是：

- slave 上电后会先进入 `Not ready to switch on`，收到 `0x0006/0x0007/0x000F` 时可能还没有离开启动初始化窗口。
- master 不等待 `Ready to switch on` 和 `Switched on`，后续目标值可能已经发出，但 slave 还没进入 `Operation enabled`。
- fault 锁存后，仅再次发送 `0x0006/0x0007/0x000F` 无法恢复，必须先执行 fault reset 上升沿。
- `status` 命令只打印十六进制状态字，不便判断当前卡在哪个 DS402 状态。

## 3. 必须新增模块

在 master 的 `USER` 目录新增：

- `ds402_master.c`
- `ds402_master.h`

职责：

- 解析 `recv_statusword`。
- 按 mask 判断 DS402 状态。
- 封装 `shutdown/switch on/enable operation/fault reset/quick stop/disable voltage`。
- 封装等待状态的超时逻辑。

## 4. 状态字解析

DS402 状态不能用完整等值判断，因为 slave 会叠加 bit4、bit5、bit9、bit10、bit12、bit13 等公共位和模式相关位。

按 mask 判断：

```c
typedef enum
{
    DS402_MASTER_STATE_UNKNOWN = 0,
    DS402_MASTER_STATE_NOT_READY_TO_SWITCH_ON,
    DS402_MASTER_STATE_SWITCH_ON_DISABLED,
    DS402_MASTER_STATE_READY_TO_SWITCH_ON,
    DS402_MASTER_STATE_SWITCHED_ON,
    DS402_MASTER_STATE_OPERATION_ENABLED,
    DS402_MASTER_STATE_QUICK_STOP_ACTIVE,
    DS402_MASTER_STATE_FAULT_REACTION_ACTIVE,
    DS402_MASTER_STATE_FAULT
} Ds402MasterState;

static int ds402_status_match(uint16_t sw, uint16_t mask, uint16_t value)
{
    return ((sw & mask) == value);
}

Ds402MasterState ds402_master_decode_status(uint16_t sw)
{
    if (ds402_status_match(sw, 0x004Fu, 0x0000u)) return DS402_MASTER_STATE_NOT_READY_TO_SWITCH_ON;
    if (ds402_status_match(sw, 0x004Fu, 0x0040u)) return DS402_MASTER_STATE_SWITCH_ON_DISABLED;
    if (ds402_status_match(sw, 0x006Fu, 0x0021u)) return DS402_MASTER_STATE_READY_TO_SWITCH_ON;
    if (ds402_status_match(sw, 0x006Fu, 0x0023u)) return DS402_MASTER_STATE_SWITCHED_ON;
    if (ds402_status_match(sw, 0x006Fu, 0x0027u)) return DS402_MASTER_STATE_OPERATION_ENABLED;
    if (ds402_status_match(sw, 0x006Fu, 0x0007u)) return DS402_MASTER_STATE_QUICK_STOP_ACTIVE;
    if (ds402_status_match(sw, 0x004Fu, 0x000Fu)) return DS402_MASTER_STATE_FAULT_REACTION_ACTIVE;
    if (ds402_status_match(sw, 0x004Fu, 0x0008u)) return DS402_MASTER_STATE_FAULT;
    return DS402_MASTER_STATE_UNKNOWN;
}
```

同时提供字符串函数，供 `status` 命令打印：

```c
const char *ds402_master_state_name(Ds402MasterState state);
```

## 5. 等待状态 helper

master 当前的 `recv_statusword` 来自 TPDO 缓存，等待状态时直接使用该缓存。新增等待函数：

```c
int ds402_master_wait_state(Ds402MasterState expected, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;

    while (elapsed < timeout_ms) {
        if (ds402_master_decode_status(recv_statusword) == expected) {
            return 0;
        }
        delay_ms(2);
        elapsed += 2;
    }

    printf("[DS402] wait %s timeout, sw=0x%04X state=%s\r\n",
           ds402_master_state_name(expected),
           (unsigned)recv_statusword,
           ds402_master_state_name(ds402_master_decode_status(recv_statusword)));
    return -1;
}
```

注意：

- 上电初期 TPDO 可能还没到，等待前最好先发送 NMT start 并延时几毫秒。
- slave 上电初态要求为 `Not ready to switch on`；master 在发送 `6040=0x0006` 前，如果看到该状态，必须先等待 `Switch on disabled`，不能把 `0x0006/0x0007/0x000F` 直接压过去。
- TPDO 日志关闭不影响缓存，因为缓存是在 CAN 接收中断中更新。
- 标准化后的主流程以 TPDO 缓存为状态来源；若后续切换为 SDO 读 `0x6041`，必须保持相同的 mask 解析和超时语义。

## 6. 标准使能流程

把 `motion_ctrl.c` 中重复的三步使能替换为一个统一 helper：

```c
int ds402_master_enable_operation(uint8_t node)
{
    Ds402MasterState state = ds402_master_decode_status(recv_statusword);

    if (state == DS402_MASTER_STATE_FAULT) {
        if (ds402_master_fault_reset(node) != 0) {
            return -1;
        }
    } else if (state == DS402_MASTER_STATE_FAULT_REACTION_ACTIVE) {
        if (ds402_master_wait_state(DS402_MASTER_STATE_FAULT, 1000) != 0) {
            return -1;
        }
        if (ds402_master_fault_reset(node) != 0) {
            return -1;
        }
    }

    state = ds402_master_decode_status(recv_statusword);
    if (state == DS402_MASTER_STATE_NOT_READY_TO_SWITCH_ON) {
        if (ds402_master_wait_state(DS402_MASTER_STATE_SWITCH_ON_DISABLED, 1000) != 0) {
            return -1;
        }
    }

    canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, 0x0006, 2);
    if (ds402_master_wait_state(DS402_MASTER_STATE_READY_TO_SWITCH_ON, 500) != 0) {
        return -1;
    }

    canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, 0x0007, 2);
    if (ds402_master_wait_state(DS402_MASTER_STATE_SWITCHED_ON, 500) != 0) {
        return -1;
    }

    canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, 0x000F, 2);
    if (ds402_master_wait_state(DS402_MASTER_STATE_OPERATION_ENABLED, 500) != 0) {
        return -1;
    }

    return 0;
}
```

然后让这些接口统一调用它：

- `motion_pv_run()`
- `motion_pp_init_enable()`
- `motion_homing_run()`
- `pt_init_enable()`

这样 slave 严格执行状态跳转后，master 不会因为盲发时序过快而卡住。

## 7. Fault reset 流程

slave 侧应按 bit7 上升沿识别 fault reset，因此 master 也必须产生上升沿：

```c
int ds402_master_fault_reset(uint8_t node)
{
    canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, 0x0000, 2);
    delay_ms(5);
    canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, 0x0080, 2);
    delay_ms(5);
    canopen_send_sdo(node, 0x6040, 0x00, ODT_U16, 0x0000, 2);

    return ds402_master_wait_state(DS402_MASTER_STATE_SWITCH_ON_DISABLED, 1000);
}
```

在 `cmd_uart.c` 增加命令：

```text
faultreset <node>
```

用于现场恢复和验证。

## 8. Quick stop 和 disable voltage

当前 `qstop` 命令写 `6040=0x0002`，这和 DS402 quick stop 命令匹配。标准化后该命令继续存在，但发送后必须等待 `Quick stop active` 或 slave 定义的停稳后状态。

补充两个调试命令：

```text
disable <node>   写 6040=0x0000，期望 Switch on disabled
shutdown <node>  写 6040=0x0006，期望 Ready to switch on
```

`halt` 命令当前直接写 `0x010F`。标准化后，`halt` 必须先确认当前为 `Operation enabled`；若不是，命令返回错误，不得把 `halt` 当成隐式使能流程。

## 9. 运动命令改造要求

### PV 速度模式

流程：

1. `sendNMT(node, 0x01)`。
2. 写 `6060=3`。
3. 调 `ds402_master_enable_operation(node)`；如果 slave 仍在 `Not ready to switch on`，helper 内部先等待 `Switch on disabled`。
4. 下发 RPDO2 目标速度。

### PP 位置模式

流程：

1. `sendNMT(node, 0x01)`。
2. 写 `6060=1`。
3. 调 `ds402_master_enable_operation(node)`；如果 slave 仍在 `Not ready to switch on`，helper 内部先等待 `Switch on disabled`。
4. 按原逻辑用 RPDO1 做 `new set-point` 脉冲。

连续位置命令 `cp/cr` 必须先检查当前为 `Operation enabled`：

```c
if (ds402_master_decode_status(recv_statusword) != DS402_MASTER_STATE_OPERATION_ENABLED) {
    printf("[PP] not operation enabled\r\n");
    return;
}
```

### PT 转矩模式

流程：

1. `sendNMT(node, 0x01)`。
2. 写 `6060=4`。
3. 调 `ds402_master_enable_operation(node)`。
4. 用 RPDO2 下发目标转矩。

连续转矩命令 `ct` 同样必须检查 `Operation enabled`。

### Homing

slave 标准化后，回零也必须纳入统一 DS402 状态响应流程。master 使用统一 enable helper 后再启动 homing。

在标准 homing 流程下，master 必须解析 `0x6041` bit10、bit12、bit13 的 homing 含义，用于判断 homing attained、homing error 和目标完成状态。

## 10. status 命令增强

当前 `status` 只打印：

```text
状态字: 0x....
```

改为：

```text
状态字: 0x1234 (Operation enabled)
bit4 Voltage enabled: 1
bit5 Quick stop: 1
bit9 Remote: 1
bit10 Target reached: 0
bit12 Mode specific: 0
bit13 Mode specific/error: 0
```

这样调试 slave 状态机时能直接看出卡在哪一步。

## 11. 最小测试清单

1. `nmt 1 start` 后，等待 slave TPDO 更新。
2. 刚上电读取 `sdor 1 6041 0` 或 `status`，应能解析为 `Not ready to switch on`；启动初始化窗口结束后应转为 `Switch on disabled`。
3. 执行 `pv 1 100`，master 应先在必要时等待 `Switch on disabled`，再依次等待 `Ready to switch on`、`Switched on`、`Operation enabled`，然后下发速度。
4. 执行 `qstop 1`，状态应进入 `Quick stop active` 或按 slave 策略停稳后转出。
5. 制造 G4 通信超时，状态应进入 `Fault reaction active` 后到 `Fault`。
6. 故障未清除时执行 `faultreset 1`，不应恢复。
7. 故障清除后执行 `faultreset 1`，应回到 `Switch on disabled`，不回到 `Not ready to switch on`。
8. 再执行 `pp/pr/pt`，应能重新完成标准使能流程。

## 12. 落地顺序

按以下顺序落地：

1. 新增 `ds402_master.c/h`，并接入 `status` 命令打印解析结果。
2. 把 `motion_ctrl.c` 中重复的三步使能替换为 `ds402_master_enable_operation()`。
3. 增加 `faultreset/disable/shutdown` 调试命令。
4. 最后给 `cp/cr/ct` 增加 `Operation enabled` 状态检查。

完成以上步骤后，master 才算具备和标准化 slave DS402 状态机配套的控制与响应能力。
