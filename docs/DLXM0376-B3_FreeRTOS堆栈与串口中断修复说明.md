# DLXM0376-B3 FreeRTOS 堆栈与串口中断修复说明

## 1. 文档信息

| 项目 | 内容 |
| --- | --- |
| 当前工程 | `DLXM0376-B3` |
| 当前分支 | `高峰入仓式遥控器MCU主机程序（带音箱和灯带驱动）` |
| 修改基线提交 | `0ffac76` |
| 参考工程 | `D:\software\nation\project\DLXM0419-B3` |
| 修改日期 | 2026-07-31 |
| 编译工具 | Keil μVision / ARM Compiler 5.06 update 6 (build 750) |
| 目标 | 量产配置下修复 FreeRTOS 堆栈风险及四路串口接收健壮性问题 |

本文记录本分支本次迁移的全部源码修改、问题原因、实现方式、编译结果和硬件验证建议。修改原则是只迁移参考工程中与 FreeRTOS 堆栈、串口接收溢出、中断标志以及中断/任务并发有关的修复，不迁移按键业务、485 协议、灯带效果、音响状态或其他产品功能改动。

## 2. 修改文件总览

| 文件 | 修改内容 |
| --- | --- |
| `FreeRTOS/FreeRTOSConfig.h` | 关闭 SystemView，FreeRTOS 堆由 10 KB 调整为 12 KB，开启栈溢出与 malloc 失败检测。 |
| `User/src/main.c` | 增加 FreeRTOS 故障状态变量和两个故障钩子；5 个风险任务由 64 words 调整为 96 words。 |
| `User/src/ble_module.c` | 修复 UART4 接收溢出、硬件错误标志及接收中断与定时器回调竞争。 |
| `User/src/sound_box.c` | 修复 USART1 接收溢出、硬件错误标志及接收中断与定时器回调竞争。 |
| `User/src/fan_heat_massage.c` | 修复 USART2 溢出、短帧越界和硬件错误标志。 |
| `User/src/remote_control.c` | 修复 UART3 溢出后继续解析半帧、接收数组宏使用错误及硬件错误标志。 |
| `docs/DLXM0376-B3_FreeRTOS堆栈与串口中断修复说明.md` | 本说明文档。 |

## 3. FreeRTOS 堆栈问题与修复

### 3.1 原配置风险

原 `FreeRTOSConfig.h` 的关键配置为：

```c
#define configTOTAL_HEAP_SIZE             ( ( size_t ) ( 10 * 1024 ) )
#define configUSE_TRACE_FACILITY          1
#define configQUEUE_REGISTRY_SIZE         8
#define configCHECK_FOR_STACK_OVERFLOW    0
#define configUSE_MALLOC_FAILED_HOOK      0
#define configUSE_STATS_FORMATTING_FUNCTIONS 1
#define configUSE_SEGGER_SYSTEM_VIEWER_HOOKS 1
```

同时直接包含：

```c
#include "SEGGER_SYSVIEW_FreeRTOS.h"
```

存在以下风险：

1. `xTaskCreate()` 的栈深度单位是 `StackType_t` 数量，不是字节。本工程 Cortex-M4 的 `StackType_t` 为 4 字节，因此原来的 `64` 实际只有 256 B。
2. 多个 64-word 任务的调用链已经接近 256 B；开启 SystemView 时，队列、调度和中断调用链还会进入 SEGGER 记录函数，进一步压缩余量。
3. 没有开启栈溢出检查，任务越界后可能表现为随机死机或数据破坏。
4. 没有开启 malloc 失败钩子，任务、队列或软件定时器创建失败时缺少明确故障现场。
5. SystemView 的 RTT、任务跟踪和缓冲区会占用量产固件不需要的 RAM。

### 3.2 FreeRTOSConfig.h 修改

量产配置修改为：

```c
#define configTOTAL_HEAP_SIZE             ( ( size_t ) ( 12 * 1024 ) )
#define configUSE_TRACE_FACILITY          0
#define configQUEUE_REGISTRY_SIZE         0
#define configCHECK_FOR_STACK_OVERFLOW    2
#define configUSE_MALLOC_FAILED_HOOK      1
#define configGENERATE_RUN_TIME_STATS     0
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configUSE_SEGGER_SYSTEM_VIEWER_HOOKS 0
```

同时完成：

- 删除 `SEGGER_SYSVIEW_FreeRTOS.h` 的直接包含。
- 删除 `portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()`。
- 删除 `portGET_RUN_TIME_COUNTER_VALUE()`。
- SystemView 源文件仍保留在 Keil 工程组中，但没有最终调用入口，链接器会把相关代码和 BSS 剔除。

配置含义：

- `configCHECK_FOR_STACK_OVERFLOW = 2`：FreeRTOS 在任务切换时检查更完整的栈边界，需要实现 `vApplicationStackOverflowHook()`。
- `configUSE_MALLOC_FAILED_HOOK = 1`：`pvPortMalloc()` 分配失败时进入 `vApplicationMallocFailedHook()`。
- 堆从 10 KB 增加到 12 KB，为增大的任务栈和启动期动态对象提供空间。
- 关闭 SystemView 后释放的静态 RAM大于新增的 2 KB FreeRTOS 堆，最终仍保留足够物理 SRAM。

### 3.3 故障状态变量

`User/src/main.c` 增加：

```c
volatile uint32_t FreeRTOSFaultCode = 0U;
volatile TaskHandle_t FreeRTOSFaultTaskHandle = NULL;
volatile const char *FreeRTOSFaultTaskName = NULL;
```

| 变量 | 正常值 | 故障含义 |
| --- | --- | --- |
| `FreeRTOSFaultCode` | `0` | `1` 表示 FreeRTOS 堆分配失败；`2` 表示任务栈溢出。 |
| `FreeRTOSFaultTaskHandle` | `NULL` | 栈溢出任务的句柄。 |
| `FreeRTOSFaultTaskName` | `NULL` | 栈溢出任务名。 |

### 3.4 故障钩子

新增实现：

```c
void vApplicationMallocFailedHook(void)
{
    FreeRTOSFaultCode = 1U;
    taskDISABLE_INTERRUPTS();
    while (1)
    {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    FreeRTOSFaultTaskName = pcTaskName;
    FreeRTOSFaultCode = 2U;
    FreeRTOSFaultTaskHandle = xTask;
    taskDISABLE_INTERRUPTS();
    while (1)
    {
    }
}
```

处理方式与参考项目一致：记录现场后关闭中断并停机，不自动复位。调试时可在 Keil Watch 中直接观察三个变量，避免复位后丢失故障现场。

### 3.5 任务栈调整

下列任务由 64 words（256 B）调整为 96 words（384 B）：

| 任务 | 修改前 | 修改后 | 最终静态最大调用深度 | 估算余量 |
| --- | ---: | ---: | ---: | ---: |
| `bsp_spi_Task` | 256 B | 384 B | 128 B | 256 B |
| `Sound_Box_Task` | 256 B | 384 B | 156 B | 228 B |
| `Sound_Box_Config_Task` | 256 B | 384 B | 120 B | 264 B |
| `KeyScanTask` | 256 B | 384 B | 112 B | 272 B |
| `KeyEventHandlerTask` | 256 B | 384 B | 112 B | 272 B |

5 个任务共增加 `5 × 32 words × 4 B = 640 B` 的 FreeRTOS 动态堆需求。

原本已分配 128 words（512 B）的任务保持不变：

| 任务 | 分配 | 最终静态最大调用深度 | 估算余量 |
| --- | ---: | ---: | ---: |
| `ble_control_Task` | 512 B | 200 B | 312 B |
| `Remote_Task` | 512 B | 140 B | 372 B |
| `MotoInitTask` | 512 B | 168 B | 344 B |
| `Motor_Task` | 512 B | 104 B | 408 B |
| `fan_heat_massage1_tx_Task` | 512 B | 188 B | 324 B |

静态调用图不能完整覆盖函数指针、软件定时器回调和中断嵌套，因此余量不能继续压缩；后续仍建议用 `uxTaskGetStackHighWaterMark()` 做长时间运行验证。

## 4. 串口接收通用问题与修复原则

### 4.1 原问题

四路串口存在不同程度的相同风险：

1. 某些 RXDNE 分支仅在缓存未满时读取 DAT。缓存满后不读取硬件数据寄存器，可能让接收/错误状态持续存在。
2. 缓存满后没有 `overflow` 标记，帧结束时仍可能把缓存中的前半帧当作完整命令解析。
3. 中断更新的计数变量没有 `volatile`，编译器无法明确知道它会被异步修改。
4. UART1、UART4 的软件定时器回调直接读取中断正在写入的全局数组，存在数据长度和内容不一致的竞争窗口。
5. 没有统一处理 ORE、NE、PE、FE 等硬件错误标志。
6. USART2 在没有检查最小长度时直接访问 `rx_buf1[6]`、`rx_buf1[7]`、`rx_buf1[8]`，短帧可能造成越界或读取旧数据。

### 4.2 通用接收规则

四路串口统一遵循：

1. RXDNE 触发后先读取 DAT。
2. 缓存未满时保存字节。
3. 缓存已满时设置软件溢出标记，不再写数组，但仍持续读取 DAT。
4. 帧结束时如果溢出标记非零，整帧丢弃。
5. 清空计数和溢出状态，保证下一帧可以恢复。
6. 检查 ORE、NE、PE、FE；按芯片要求依次读取 STS、DAT 清理错误状态。
7. 不改变现有帧头、校验和算法、业务命令编号和队列数据格式。

硬件错误处理形式：

```c
if ((USART_Flag_Status_Get(UARTx, USART_FLAG_OREF) != RESET) ||
    (USART_Flag_Status_Get(UARTx, USART_FLAG_NEF) != RESET)  ||
    (USART_Flag_Status_Get(UARTx, USART_FLAG_PEF) != RESET)  ||
    (USART_Flag_Status_Get(UARTx, USART_FLAG_FEF) != RESET))
{
    (void)UARTx->STS;
    (void)UARTx->DAT;
}
```

## 5. UART4：BLE 接收修复

文件：`User/src/ble_module.c`

### 5.1 状态变量

```c
volatile uint8_t ble_rx_count = 0;
static volatile uint8_t ble_rx_overflow = 0;
```

### 5.2 RXDNE 处理

- 每次中断先调用 `USART_Data_Receive(UART4)`。
- `ble_rx_count < BLE_MAX_BUF_LEN` 时写入数组。
- 缓存满后设置 `ble_rx_overflow = 1`。
- 继续重置 10 ms 单次软件定时器，用帧间隔结束当前帧。

### 5.3 定时器回调快照

`UsartTimeoutCallback()` 不再直接解析 ISR 正在写入的全局数组。回调中增加 20 字节局部帧缓冲，并在临界区内完成：

- 快照当前长度。
- 快照溢出状态。
- 复制接收数据。
- 清零全局计数与溢出状态。

离开临界区后再执行长度、帧头和校验和检查。这样临界区保持很短，校验和及队列发送不会长时间屏蔽中断。

溢出或长度不大于 4 字节时直接丢帧；合法帧仍通过 `ble_rx_queue` 复制整块 20 字节队列项，业务任务接口不变。

## 6. USART1：音响接收修复

文件：`User/src/sound_box.c`

状态变量调整为：

```c
volatile uint8_t Sound_rx_buf[SOUND_RX_BUF_LEN] = {0};
volatile uint8_t sound_rx_count = 0;
static volatile uint8_t sound_rx_overflow = 0;
```

修改内容：

- RXDNE 分支无条件读取 DAT。
- 10 字节缓存满后只设置 `sound_rx_overflow`，不再越界写入。
- 继续重置原 5 ms 单次软件定时器。
- `UsartSoundTimeoutCallback()` 使用 10 字节局部数组，在临界区内快照并清空 ISR 状态。
- 溢出帧直接丢弃。
- 原有帧头、校验和、等待回复判断和上电通知处理保持不变。
- 增加 USART1 的 ORE、NE、PE、FE 清理。

## 7. USART2：风热按摩接收修复

文件：`User/src/fan_heat_massage.c`

状态变量：

```c
volatile uint8_t rx_count1 = 0;
static volatile uint8_t rx_overflow1 = 0;
```

USART2 使用 IDLE 中断作为帧结束标志，修改后：

- RXDNE 始终先读取 DAT。
- 20 字节缓存满后设置 `rx_overflow1`。
- IDLE 时仅在 `rx_overflow1 == 0` 且 `rx_count1 >= 10` 时解析。
- 最小长度检查保证访问索引 6、7、8 和最后一个校验字节之前，数据确实存在。
- 帧处理结束后同时清零 `rx_count1` 和 `rx_overflow1`。
- 增加 USART2 的 ORE、NE、PE、FE 清理。

原有 `0xD0 0xD0` 帧头、`Reply_flag1`、校验和、状态字段及队列发送行为保持不变。

## 8. UART3：遥控接收修复

文件：`User/src/remote_control.c`

原接收数组错误地使用发送长度宏声明：

```c
uint8_t remote_rx_buf[REMOTE_TX_BUF_LEN];
```

虽然当前发送和接收宏都为 20，但语义不正确，后续单独修改长度时容易引入隐患。已改为：

```c
uint8_t remote_rx_buf[REMOTE_RX_BUF_LEN];
volatile uint8_t remote_rx_count = 0;
static volatile uint8_t remote_rx_overflow = 0;
```

UART3 同样使用 IDLE 结束一帧：

- RXDNE 始终读取 DAT。
- 缓存满后设置 `remote_rx_overflow`。
- IDLE 时只有未溢出且长度大于 4 才检查帧头和校验和。
- 溢出帧不会进入命令 `switch`。
- 帧结束后清零计数和溢出标记。
- 增加 UART3 的 ORE、NE、PE、FE 清理。

以下业务命令保持原样：BLE 重置、BLE 断开、灯带增减、振动增减和音量增减。

### 8.1 无行为变化的代码清理

重写接收段时同时移除了原本没有被使用的局部变量和失效注释，避免继续占用阅读成本或让后续维护者误判：

- `UART4_IRQHandler()` 删除已整段注释、且 UART4 当前未启用 IDLE 中断的旧帧解析草稿。
- `USART1_IRQHandler()` 删除未使用的 `INTStatus status`；`UsartSoundTimeoutCallback()` 删除未使用的 `check`。
- `USART2_IRQHandler()` 删除未使用的 `check`。
- `UART3_IRQHandler()` 删除未使用的 `moto_cmd`、`check` 和 `mac_temp`。
- 将 `xQueueSend()` 的等待时间参数由 `NULL` 统一写为数值 `0`，语义仍为不等待。
- 为原本缺少文件末尾换行的 `remote_control.c` 补充标准换行。

上述清理不改变协议、命令或运行时控制流程。
## 9. 编译与 map 验证

### 9.1 编译结果

Keil 全量编译结果：

```text
Program Size: Code=43312 RO-data=400 RW-data=404 ZI-data=13820
0 Error(s), 45 Warning(s)
```

编译产物：

- `Objects/N32G430xx_0176BY_X.axf`
- `Listings/N32G430xx_0176BY_X.map`
- `Objects/N32G430xx_0176BY_X.htm`

### 9.2 RAM 与 FreeRTOS 堆

map 结果：

```text
ucHeap:        12288 B
RW_IRAM1:      0x3790 / 0x4000
Total RW:      14224 B / 16384 B
剩余物理 SRAM: 2160 B
```

结论：

- 12 KB FreeRTOS 堆成功链接。
- SRAM 没有溢出。
- 最终仍保留 2160 B 物理 RAM，超过 1 KB 的最低建议余量。
- map 中没有 `SEGGER_RTT`、`SEGGER_SYSVIEW`、`SYSVIEW_AddTask` 等最终镜像符号，说明 SystemView 代码和 BSS 已被链接器剔除。

### 9.3 回调和中断调用深度

| 函数 | 最终静态最大调用深度 |
| --- | ---: |
| `UART4_IRQHandler` | 168 B |
| `USART1_IRQHandler` | 160 B |
| `USART2_IRQHandler` | 112 B |
| `UART3_IRQHandler` | 104 B |
| `UsartTimeoutCallback` | 152 B |
| `UsartSoundTimeoutCallback` | 144 B |

两个软件定时器回调运行在 FreeRTOS Timer Task 中。当前 `configTIMER_TASK_STACK_DEPTH` 为 `configMINIMAL_STACK_SIZE * 2 = 260 words = 1040 B`，对上述回调调用深度有充足余量。

## 10. 编译警告说明

本次全量编译有 45 条警告，但没有错误：

1. 43 条来自 `SystemView/SEGGER_SYSVIEW_FreeRTOS.c` 的 trace 宏重复定义。原因是 SystemView 源文件仍保留在 Keil 工程组中参与编译；最终 map 已确认这些对象没有进入量产镜像。
2. 2 条来自 `User/src/ble_module.c` 原有的 `ble_tx_buf` 下标越界告警，位于读取第三个电机位置并组帧的代码。本次没有修改该业务发送逻辑，因此该问题不属于本次堆栈/串口接收修复范围，但建议后续单独处理。

如果希望编译日志完全消除前 43 条警告，可在量产 Target 中将 SystemView 两个源文件设为 `Exclude from Build`；这不是本次必须项，因为最终镜像已确认不包含 SystemView。

## 11. 明确未修改的内容

本次没有修改：

- 电机方向、位置和控制命令。
- BLE、音响、灯带、风热按摩的协议编号与帧格式。
- 遥控器各业务按键的命令映射。
- 按键扫描周期、任务优先级和事件处理逻辑。
- 软件定时器周期。
- 任务优先级。
- 485 协议或主从机框架。
- 音量、EQ、灯带颜色和效果业务。

## 12. 调试方法

连接 DAP 后，在 Keil Watch 窗口加入：

```text
FreeRTOSFaultCode
FreeRTOSFaultTaskName
FreeRTOSFaultTaskHandle
```

判断方法：

| 状态 | 含义 | 建议动作 |
| --- | --- | --- |
| `FreeRTOSFaultCode == 0` | 未触发 FreeRTOS 堆/栈保护 | 继续检查供电、复位和外围上电时序。 |
| `FreeRTOSFaultCode == 1` | FreeRTOS 动态堆分配失败 | 检查队列、任务、定时器创建数量和堆剩余量。 |
| `FreeRTOSFaultCode == 2` | 任务栈溢出 | 查看任务名和句柄，提高对应任务栈并检查局部数组。 |

## 13. 硬件验证建议

### 13.1 启动和堆栈

- 连续冷上电、热复位各不少于 100 次。
- 音响、灯带、电机、风热按摩同时工作并持续运行。
- 观察三个 FreeRTOS 故障变量始终保持正常值。
- 如条件允许，记录各任务 `uxTaskGetStackHighWaterMark()` 的长期最小值。

### 13.2 串口溢出恢复

分别向 UART1、USART2、UART3、UART4 注入超过各自 10/20 字节缓存长度的连续数据：

1. 确认设备不死机、不越界、不误执行前半帧命令。
2. 等待当前异常帧结束。
3. 再发送一帧合法数据。
4. 确认下一帧可以正常接收和执行，说明计数及溢出标记已正确恢复。

### 13.3 错误标志

在可控测试环境下使用错误波特率、噪声或不完整停止位制造 ORE、NE、PE、FE：

- 确认中断不会持续进入。
- 确认系统任务调度仍正常。
- 恢复正确串口参数后，后续合法帧能够继续通信。

### 13.4 并发压力

- UART4 连续接收 BLE 命令，同时操作电机、灯带和音响。
- USART1 连续接收音响回复和主动通知。
- 重点确认软件定时器回调解析的是完整快照，没有校验偶发失败或帧内容撕裂。

## 14. 结论

本次修复完成了以下目标：

- 量产固件关闭 SystemView，释放静态 RAM 并缩短 FreeRTOS 调用链。
- 5 个风险任务从 256 B 提升到 384 B，并启用可观测的堆/栈故障保护。
- FreeRTOS 堆扩大到 12 KB，最终仍保留 2160 B 物理 SRAM。
- UART1、USART2、UART3、UART4 均具备缓存溢出丢整帧和硬件错误标志恢复能力。
- UART1、UART4 消除了 ISR 接收缓冲与软件定时器回调之间的并发读取风险。
- USART2 增加短帧保护，UART3 不再解析溢出半帧。
- Keil 全量编译通过，最终镜像不包含 SystemView。
