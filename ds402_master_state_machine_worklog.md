# DS402 Master 状态机改造记录

## 1. 修改目的

`ds402_master_update_plan.md` 要求 master 配合 slave 的标准 DS402 PDS finite state automaton。原 master 在速度、位置、回零、转矩流程中直接连续写 `0x6040=0x0006 -> 0x0007 -> 0x000F`，没有等待 `0x6041 Statusword` 确认。slave 一旦严格按 DS402 状态机执行，就可能出现主站目标值已经下发、从站尚未进入 `Operation enabled` 的问题。

本次改造把 master 的使能流程改为：主站写 `0x6040 Controlword`，等待 TPDO1 缓存中的 `0x6041 Statusword` 确认，确认成功后再进入下一步。

## 2. 本次修改位置

### 2.1 新增文件

- `Canopen_Master/USER/ds402_master.h`
  - 新增 `Ds402MasterState` 状态枚举。
  - 声明 DS402 状态字解析、状态等待、使能、故障复位、快速停止、撤销电压、状态打印等接口。

- `Canopen_Master/USER/ds402_master.c`
  - 新增 `ds402_master_decode_status()`：按 mask/value 方式解析 `0x6041`，避免 bit4、bit5、bit9、bit10、bit12、bit13 等附加位影响主状态判断。
  - 新增 `ds402_master_wait_state()`：每 2ms 读取一次 `recv_statusword`，在超时时打印期望状态、当前状态和原始状态字。
  - 新增 `ds402_master_enable_operation()`：统一执行 Fault 处理、Not ready 等待、`shutdown -> switch on -> enable operation` 三段式使能。
  - 新增 `ds402_master_fault_reset()`：按 `0x0000 -> 0x0080 -> 0x0000` 产生 bit7 上升沿。
  - 新增 `ds402_master_disable_voltage()`、`ds402_master_shutdown()`、`ds402_master_quick_stop()`，供串口调试命令使用。
  - 新增 `ds402_master_print_status()`：打印状态字主状态和关键 bit。

### 2.2 修改文件

- `Canopen_Master/USER/motion_ctrl.c`
  - `motion_pv_run()`：先写 `6060=3`，再调用 `ds402_master_enable_operation()`，确认 `Operation enabled` 后才通过 RPDO2 下发目标速度。
  - `motion_cv_set_speed()`：连续速度命令增加 `Operation enabled` 检查，未使能时拒绝下发目标速度。
  - `motion_pp_init_enable()`：先写 `6060=1`，再调用统一 DS402 使能 helper。
  - `pp_move_trigger()`：绝对/相对位置触发前检查 `Operation enabled`。
  - `motion_homing_run()`：写 `6060=6` 和 `6098` 后，先走统一 DS402 使能，再写 `6040=0x001F` 启动回零。
  - `pt_init_enable()`：改为返回 `int`，写 `6060=4` 后调用统一 DS402 使能。
  - `motion_pt_run()`：转矩目标只在使能成功后通过 RPDO2 下发。
  - `motion_ct_set_torque()`：连续转矩命令增加 `Operation enabled` 检查。
  - 删除未被引用的旧静态 helper：`pv_set_speed_sdo()`、`helper_set_profile_speed()`、`pt_set_torque_sdo()`，消除 Keil warning。

- `Canopen_Master/USER/cmd_uart.c`
  - 新增 `faultreset <node>`：调用 `ds402_master_fault_reset()`。
  - 新增 `disable <node>`：写 `6040=0x0000`，等待 `Switch on disabled`。
  - 新增 `shutdown <node>`：写 `6040=0x0006`，等待 `Ready to switch on`。
  - 增强 `status`：打印 DS402 主状态和 bit4、bit5、bit9、bit10、bit12、bit13。
  - 修改 `halt`：必须当前为 `Operation enabled` 才能写 `6040=0x010F`。
  - 修改 `qstop`：改为调用 `ds402_master_quick_stop()`，写 `6040=0x0002` 后等待 `Quick stop active`。
  - 补充 `pp/cp/pr/cr` 参数不足时的用法提示，避免参数缺失时继续执行。

- `Canopen_Master/USER/CAN.uvprojx`
  - 在 Keil `USER` 组加入 `ds402_master.c` 和 `ds402_master.h`。

- `Canopen_Master/USER/CAN.uvoptx`
  - 同步 Keil 工程显示文件树，加入 `ds402_master.c` 和 `ds402_master.h`。

## 3. 状态机总体数据来源和去向

### 3.1 输入来源

master 状态机的输入分为三类：

1. 串口命令输入
   - 来源：上位机/调试串口输入文本命令。
   - 入口：`parse_uart_cmd()`。
   - 例子：`pv 1 100`、`pp 1 50000 1000`、`pt 1 200`、`status`、`faultreset 1`。

2. 从站状态反馈输入
   - 来源：slave 通过 TPDO 周期或事件发送状态反馈。
   - TPDO1：COB-ID `0x181`，数据 `[0..1]=0x6041 Statusword`，`[2..5]=0x606C Actual velocity`。
   - TPDO2：COB-ID `0x281`，数据 `[0..3]=0x6064 Actual position`，`[4..5]=0x6077 Actual torque`。
   - 接收位置：`canfestival_can.c` 的 CAN 接收中断。
   - 缓存去向：`recv_statusword`、`recv_actual_vel`、`recv_actual_pos`、`recv_actual_torque`。

3. 从站 SDO 应答输入
   - 来源：slave 对 master 写对象字典的 SDO 响应。
   - 入口：`canopen_send_sdo()` 内部通过 CanFestival 查询写结果。
   - 用途：确认 `6060`、`6040`、`6081`、`6098` 等对象写入是否成功。

### 3.2 输出去向

master 状态机的输出分为四类：

1. NMT 输出
   - 函数：`sendNMT(node, 0x01)`。
   - 去向：slave 节点。
   - 作用：把 slave 推到 CANopen Operational，允许 PDO/SDO 进入正常工作。

2. SDO 输出
   - 函数：`canopen_send_sdo()`。
   - 去向：slave 对象字典。
   - 典型对象：
     - `0x6060 Modes of operation`：选择速度、位置、转矩、回零模式。
     - `0x6040 Controlword`：驱动 DS402 PDS 状态机跳转。
     - `0x6081 Profile velocity`：位置模式轮廓速度。
     - `0x6098 Homing method`：回零方法。

3. RPDO 输出
   - RPDO1：COB-ID `0x201`，用于位置模式，发送控制字、模式字节和目标位置。
   - RPDO2：COB-ID `0x301`，用于速度/转矩模式，发送目标速度或目标转矩。

4. 串口日志输出
   - 去向：调试串口。
   - 作用：打印 SDO 结果、DS402 状态等待结果、实际速度/位置/力矩、状态字 bit 解析。

## 4. DS402 主状态机流程

### 4.1 状态字解析

`ds402_master_decode_status(sw)` 不用完整等值判断 `0x6041`，而是按 DS402 mask/value 判断：

| 状态 | mask | value |
| --- | --- | --- |
| Not ready to switch on | `0x004F` | `0x0000` |
| Switch on disabled | `0x004F` | `0x0040` |
| Ready to switch on | `0x006F` | `0x0021` |
| Switched on | `0x006F` | `0x0023` |
| Operation enabled | `0x006F` | `0x0027` |
| Quick stop active | `0x006F` | `0x0007` |
| Fault reaction active | `0x004F` | `0x000F` |
| Fault | `0x004F` | `0x0008` |

### 4.2 统一使能流程

`ds402_master_enable_operation(node)` 的工作流程如下：

1. 读取 `recv_statusword` 并解析当前 DS402 状态。
2. 如果当前是 `Fault`：
   - 写 `6040=0x0000`。
   - 延时 5ms。
   - 写 `6040=0x0080`，产生 fault reset 上升沿。
   - 延时 5ms。
   - 写 `6040=0x0000`。
   - 等待 `Switch on disabled`。
3. 如果当前是 `Fault reaction active`：
   - 先等待进入 `Fault`。
   - 再执行上面的 fault reset 流程。
4. 如果当前是 `Not ready to switch on`：
   - 等待从站启动初始化结束，进入 `Switch on disabled`。
5. 写 `6040=0x0006`，等待 `Ready to switch on`。
6. 写 `6040=0x0007`，等待 `Switched on`。
7. 写 `6040=0x000F`，等待 `Operation enabled`。
8. 任一步超时或 SDO 写失败，函数返回失败，上层运动命令不会继续下发目标值。

## 5. 转速模式 PV/CV

### 5.1 完整启动命令 `pv <node> <speed>`

输入来源：

- 用户从串口输入 `pv 1 100`。
- `parse_uart_cmd()` 解析出 `node=1`、`speed=100`。

master 输出流程：

1. `parse_uart_cmd()` 调用 `sendNMT(1, 0x01)`。
   - 输出到 slave：NMT Start Remote Node。
2. `motion_pv_run(1, 100)` 写 `6060=3`。
   - 输出到 slave：SDO 下载 `Modes of operation = Profile Velocity`。
3. `motion_pv_run()` 调用 `ds402_master_enable_operation(1)`。
   - 输出到 slave：按状态机写 `6040=0x0006/0x0007/0x000F`。
   - 输入来自 slave：TPDO1 的 `6041` 更新到 `recv_statusword`。
   - 判断结果：必须等到 `Operation enabled`。
4. 使能成功后调用 `rpdo2_send_pv()`。
   - 输出到 slave：RPDO2，COB-ID `0x301`。
   - 数据内容：`[0..3]=目标速度 speed`，`[4..5]=0`。

slave 反馈去向：

- slave 通过 TPDO1 反馈 `6041 Statusword` 和 `606C Actual velocity`。
- master 接收中断解析后写入 `recv_statusword` 和 `recv_actual_vel`。
- 用户输入 `status` 时可以看到状态字解析和实际速度。

### 5.2 连续改速命令 `cv <node> <speed>`

输入来源：

- 用户从串口输入 `cv 1 200`。

master 输出流程：

1. `motion_cv_set_speed()` 先检查 `recv_statusword` 是否解析为 `Operation enabled`。
2. 如果不是 `Operation enabled`，串口打印 `[CV] not operation enabled`，不发送 RPDO2。
3. 如果已经使能，调用 `rpdo2_send_pv()` 更新目标速度。

该命令不写 `6060`，不执行 DS402 使能，要求前面已经通过 `pv` 或其他流程完成速度模式准备。

## 6. 位置模式 PP/CP/PR/CR

### 6.1 绝对位置完整命令 `pp <node> <pos> [speed]`

输入来源：

- 用户从串口输入 `pp 1 50000 1000`。
- `node=1`，`pos=50000`，可选 `speed=1000`。

master 输出流程：

1. 如果用户给了 `speed`，先写 `6081=1000`。
   - 输出到 slave：SDO 下载 Profile velocity。
2. 调用 `sendNMT(1, 0x01)`。
   - 输出到 slave：NMT Start。
3. `motion_pp_init_enable(1)` 写 `6060=1`。
   - 输出到 slave：SDO 下载 `Modes of operation = Profile Position`。
4. `motion_pp_init_enable()` 调用 `ds402_master_enable_operation(1)`。
   - 输出到 slave：`6040=0x0006/0x0007/0x000F`。
   - 输入来自 slave：TPDO1 中的 `6041`。
   - 结果：必须确认 `Operation enabled`。
5. `motion_pp_move_abs()` 调用 `pp_move_trigger()`。
   - 再次检查当前为 `Operation enabled`。
   - 通过 RPDO1 下发位置目标和 new set-point 脉冲。

RPDO1 数据去向：

- COB-ID：`0x201`。
- `[0..1]`：控制字。
- `[2]`：模式字节，固定 `0x01`。
- `[3..6]`：目标位置。

绝对位置控制字脉冲：

1. `cw_base=0x000F | bit5`，bit6 不置位，表示绝对位置。
2. 第一次发送 `bit4=0`，准备目标。
3. 第二次发送 `bit4=1`，产生 new set-point。
4. 第三次发送 `bit4=0`，释放脉冲。

slave 反馈去向：

- TPDO1 反馈 `6041` 和实际速度。
- TPDO2 反馈 `6064 Actual position` 和 `6077 Actual torque`。
- master 分别缓存到 `recv_statusword`、`recv_actual_vel`、`recv_actual_pos`、`recv_actual_torque`。

### 6.2 相对位置完整命令 `pr <node> <delta> [speed]`

`pr` 和 `pp` 的流程相同，区别是 `pp_move_trigger()` 会置位控制字 bit6：

- bit6=1：目标值解释为相对位移。
- RPDO1 `[3..6]` 发送的是相对位移 `delta`。

### 6.3 连续位置命令 `cp/cr`

输入来源：

- `cp 1 60000`：连续绝对位置。
- `cr 1 1000`：连续相对位置。

master 输出流程：

1. 如果提供可选 `speed`，先写 `6081`。
2. 不发送 NMT。
3. 不写 `6060`。
4. 不执行 DS402 使能。
5. 只检查当前 `recv_statusword` 是否为 `Operation enabled`。
6. 检查通过后，通过 RPDO1 发送 new set-point 脉冲。

也就是说，`cp/cr` 的前置条件是：外部已经用 `pp/pr` 或其他流程完成位置模式和 Operation enabled。

## 7. 转矩模式 PT/CT

### 7.1 完整启动命令 `pt <node> <torque>`

输入来源：

- 用户从串口输入 `pt 1 200`。

master 输出流程：

1. `motion_pt_run()` 调用 `sendNMT(1, 0x01)`。
   - 输出到 slave：NMT Start。
2. `pt_init_enable()` 写 `6060=4`。
   - 输出到 slave：SDO 下载 `Modes of operation = Profile Torque`。
3. `pt_init_enable()` 调用 `ds402_master_enable_operation(1)`。
   - 输出到 slave：`6040=0x0006/0x0007/0x000F`。
   - 输入来自 slave：TPDO1 `6041`。
   - 结果：必须确认 `Operation enabled`。
4. 使能成功后调用 `rpdo2_send_pt()`。
   - 输出到 slave：RPDO2，COB-ID `0x301`。
   - 数据内容：`[0..3]=0`，`[4..5]=目标转矩 torque`。

slave 反馈去向：

- TPDO2 `[4..5]` 反馈 `6077 Actual torque`。
- master 缓存到 `recv_actual_torque`。
- 用户输入 `status` 时可以看到实际力矩。

### 7.2 连续转矩命令 `ct <node> <torque>`

输入来源：

- 用户从串口输入 `ct 1 300`。

master 输出流程：

1. `motion_ct_set_torque()` 检查当前是否为 `Operation enabled`。
2. 如果不是，打印 `[CT] not operation enabled`，不发送 RPDO2。
3. 如果是，调用 `rpdo2_send_pt()` 更新目标转矩。

该命令不写 `6060`，不执行 DS402 使能，要求前面已经通过 `pt` 或其他流程完成转矩模式准备。

## 8. 回零 Homing 补充流程

虽然回零不属于用户要求的三大模式之一，但原文档要求同步纳入标准 DS402 响应流程。

命令：`hm <node> <method>`。

流程：

1. 串口命令解析出节点和回零方法。
2. `sendNMT(node, 0x01)`。
3. 写 `6060=6`，选择 Homing 模式。
4. 写 `6098=method`，设置回零方法。
5. 调用 `ds402_master_enable_operation()`，确认 `Operation enabled`。
6. 写 `6040=0x001F`，启动回零。
7. `status` 命令会打印：
   - bit10 Target reached。
   - bit12 Mode specific / Homing attained。
   - bit13 Mode specific / Homing error。

## 9. 调试命令说明

- `status`
  - 打印最近一次 TPDO 缓存。
  - 输出状态字原始值、DS402 主状态、bit4、bit5、bit9、bit10、bit12、bit13、实际速度、实际位置、实际力矩。

- `faultreset <node>`
  - 写 `6040=0x0000 -> 0x0080 -> 0x0000`。
  - 等待 `Switch on disabled`。
  - 如果故障原因没有清除，等待会超时，串口会打印当前状态。

- `disable <node>`
  - 写 `6040=0x0000`。
  - 等待 `Switch on disabled`。

- `shutdown <node>`
  - 写 `6040=0x0006`。
  - 等待 `Ready to switch on`。

- `qstop [node]`
  - 写 `6040=0x0002`。
  - 等待 `Quick stop active`。

- `halt [node]`
  - 当前必须为 `Operation enabled`。
  - 成功后写 `6040=0x010F`。
  - 如果未使能，打印 `[HALT] not operation enabled`，不写控制字。

## 10. 编译状态

已执行 Keil 批量编译：

```text
D:\Keil\UV4\UV4.exe -b Canopen_Master\USER\CAN.uvprojx -j0 -o Canopen_Master\OBJ\keil_build_ds402_master.log
```

最终日志文件：

- `Canopen_Master/OBJ/keil_build_ds402_master.log`

最终结果：

```text
"..\OBJ\CAN.axf" - 0 Error(s), 0 Warning(s).
Build Time Elapsed:  00:00:02
```
