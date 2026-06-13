# MCU - GD32F407 FreeRTOS 嵌入式开发学习项目

## 项目概述

本项目是一个基于 **GD32F407** (ARM Cortex-M4) 微控制器的嵌入式开发学习平台，深度集成了 **FreeRTOS** 实时操作系统。项目从零开始搭建，涵盖了从底层硬件驱动、RTOS内核机制、中间件移植到应用开发的完整嵌入式软件栈，旨在系统性地学习和实践嵌入式软件开发。

| 项目属性 | 说明 |
|---|---|
| **主控芯片** | GD32F407 (Cortex-M4, 168MHz) |
| **RTOS** | FreeRTOS (V10.x+) |
| **开发环境** | Keil MDK v5 (ARMCC) |
| **固件库** | GD32F4xx 标准外设库 |
| **编码格式** | 源码: GB2312, 文档: UTF-8 |

---

## 项目结构

```
MCU/
├── FreeRTOS/                          # FreeRTOS 内核源码
│   ├── include/                       # FreeRTOS 头文件
│   ├── port/                          # 移植层 (RVDS/MemMang)
│   │   ├── MemMang/                   # 内存管理策略 (heap_1 ~ heap_5)
│   │   └── RVDS/                      # ARM RVDS 编译器移植
│   └── src/                           # 内核源码 (tasks.c, queue.c, timers.c 等)
├── Libraries/                         # 硬件抽象层
│   ├── Board/
│   │   └── GD32/
│   │       ├── GD32F4xx_standard_peripheral/  # GD32 标准外设库 (Include/Source)
│   │       └── GD32F4xx_startup/             # 启动文件和系统文件
│   └── CMSIS/
│       └── Core/                      # ARM CMSIS 核心支持 (CM4)
├── Project/                           # Keil 工程文件
│   ├── gd32f407_freertos.uvprojx      # 项目工程
│   ├── Bins/                          # 编译输出 (.bin)
│   ├── Listings/                      # 编译列表文件
│   └── Objects/                       # 目标文件
├── User/                              # 用户应用代码
│   ├── main.c                         # 主函数入口
│   ├── md_test.c                      # mbedTLS 散列函数测试
│   ├── config/                        # GD32 配置 (gd32f4xx_it, libopt)
│   ├── debug/                         # 串口调试模块
│   ├── drivers/                       # 硬件驱动层
│   ├── led_timer/                     # LED 定时器控制
│   ├── middleware/                    # 中间件库 (mbedTLS, MQTT, HTTP 等)
│   ├── middleware_test/               # 中间件测试代码
│   ├── queue_extend/                  # 队列扩展使用
│   ├── rtos_cli/                      # 命令行接口 (CLI)
│   ├── stream_buffer/                 # 流缓冲区和消息缓冲区
│   ├── task_control/                  # 任务管理 (并发/监控/自杀)
│   ├── task_notify/                   # 任务通知 / 信号量 / 互斥量
│   └── timer/                         # 定时器应用
└── Doc/                               # 项目文档
```

---

## 硬件驱动层 (User/drivers)

硬件驱动基于 GD32 标准外设库封装，提供简洁易用的底层接口。

| 驱动模块 | 文件 | 功能说明 |
|---|---|---|
| **USART 驱动** | `drv_usart.c/h` | 多路 USART 初始化与收发，支持中断模式。预配置了 10 组 USART 引脚映射 (USART0~5, UART3~4) |
| **Flash 驱动** | `drv_flash.c/h` | Flash 读写操作 |
| **独立看门狗** | `drv_fwdgt.c/h` | FWDGT 初始化与喂狗。基于 IRC32K 时钟，分频系数 256 |
| **RTC 驱动** | `drv_rtc.c/h` | 实时时钟，提供完整的年月日时分秒日历功能，支持 BCD 码与十进制互转 |

### USART 驱动特性

- 支持 USART0/1/2/5 和 UART3/4 共 6 个串口外设
- 预配置多种引脚复用映射表
- 灵活的 GPIO 配置：TX/RX 引脚可独立指定
- 支持初始化时使能接收中断

### 看门狗使用

```
drv_fwdgt_init(10);    // 初始化看门狗，10秒超时
drv_fwdgt_feed();      // 喂狗
```

---

## 串口调试模块 (User/debug)

提供两种差异化串口交互方案：

### 1. debug_entry.c - 简单中断式命令处理

- **设计理念**: 在中断中直接接收并处理字符，回显支持 Backspace/Enter
- **工作流程**: 中断接收 → 缓存行 → 设置标志 → 任务轮询处理
- **已实现命令**: `help`, `reboot`
- **`fputc` 重定向**: 通过 `fputc_debug_entry()` 重定向 printf

### 2. serial.c - 队列驱动的异步串口

- **设计理念**: 基于 FreeRTOS 队列实现异步收发
- **发送**: 发送队列 (1024字节) + 发送中断使能/禁用的流控机制
- **接收**: 接收队列 (64字节) + 任务轮询处理
- **多线程安全**: 配合互斥量 (Mutex) 实现临界区保护
- **`fputc` 重定向**: 非阻塞方式向发送队列写入

```c
// 初始化
serial_init();

// 发送
serial_put_char('A', 0);         // 非阻塞发送
serial_put_string("Hello", 5);   // 发送字符串

// 接收
char c;
serial_get_char(&c, portMAX_DELAY);  // 阻塞接收
```

---

## FreeRTOS 内核学习与移植

### 移植要点 (参见 Doc/ReadMe_Kernel.md)

- **port.c**: 适配 PendSV_Handler (上下文切换)、SysTick_Handler (时基)、SVC_Handler (系统调用)
- **FreeRTOSConfig.h**: 配置中断优先级、内存管理、调度策略、定时器等参数

### 内核学习内容

#### 1. 任务管理 (User/task_control/)

| 模块 | 文件 | 学习内容 |
|---|---|---|
| **任务监控** | `task_monitor.c` | 创建者任务创建/销毁子任务 (自杀式任务)，测试任务生命周期管理 |
| **并发队列** | `task_concurrent_access_queue.c` | 高/低优先级任务并发访问队列，模拟中断向队列发送数据，验证数据一致性 |

##### 任务监控
- `vCreateSuicidalTask()`: 创建一个"创建者"任务
- 创建者周期创建两个"自杀式"子任务，子任务执行后删除自己
- 验证任务的创建、删除、挂起、恢复全生命周期

##### 并发队列访问
- 两个高优先级任务从队列接收数据
- 一个低优先级任务在接收不到数据时提升自身优先级，向队列发送数据后恢复原优先级
- 模拟两个定时器中断向队列发送/接收数据
- 验证 FreeRTOS 队列在多任务并发访问下的数据完整性

#### 2. 信号量与互斥量 (User/task_notify/)

| 模块 | 文件 | 学习内容 |
|---|---|---|
| **二进制信号量** | `binary_semtest.c` | 信号量实现任务同步，阻塞/非阻塞模式切换 |
| **计数信号量** | `mutex_count_semtest.c` | 资源计数、中断中释放信号量 |
| **互斥量** | `mutex_count_semtest.c` | 优先级继承演示 |
| **递归锁** | `recursive_mutex.c` | 递归互斥量的深层嵌套上锁 / 解锁匹配，优先级继承验证 |

##### 优先级继承示例
```
高优先级任务 (Rec1) ── 获取 Mutex ── 被阻塞 ── 等待释放
中优先级任务 (Rec2) ── 尝试获取 ── 被阻塞 ── 等待释放
低优先级任务 (Rec3) ── 获取锁 ── 继承高优先级 ── 释放锁 ── 恢复原优先级
```

#### 3. 任务通知 (User/task_notify/)

| 特性 | API | 说明 |
|---|---|---|
| **eNoAction** | `xTaskNotify` | 仅设置通知状态，轻量事件标志 |
| **eSetBits** | `xTaskNotify` | 按位或操作，实现事件组 |
| **eIncrement** | `xTaskNotify` | 通知值加1，轻量计数信号量 |
| **eSetValueWithOverwrite** | `xTaskNotify` | 无条件覆盖通知值 |
| **eSetValueWithoutOverwrite** | `xTaskNotify` | 仅当未接收时覆盖 |
| **等待通知** | `xTaskNotifyWait` | 阻塞等待通知，支持进入/退出位清除 |
| **计数通知** | `xTaskNotifyGive` / `ulTaskNotifyTake` | 简化的计数信号量模拟 |
| **通知数组** | `xTaskNotifyWaitIndexed` | 多通道通知 (configTASK_NOTIFICATION_ARRAY_ENTRIES) |

#### 4. 队列与队列集 (User/queue_extend/)

- **通用队列**: 使用 `xQueueSendToFront` 模拟栈操作
- **队列集**: 监控多个队列/信号量的联合就绪状态

#### 5. 软件定时器 (User/timer/)

| 模块 | 文件 | 学习内容 |
|---|---|---|
| **定时器任务** | `timer_task.c` | 自动重载定时器、单次定时器、ISR 安全定时器，定时器操作队列流控 |
| **定时器抖动** | `timer_jitter.c` | 使用 TIM2 + TIM3 测量 FreeRTOS 中断响应的抖动性能 |

##### 定时器类型
- **自动重载定时器**: 周期触发，持续有效直到停止或删除
- **单次定时器**: 触发一次后失活

注意: 在调度器启动前创建定时器需注意 `configTIMER_QUEUE_LENGTH`，超过队列长度可能导致死等。

#### 6. 流缓冲区 (User/stream_buffer/)

| 类型 | 说明 |
|---|---|
| **StreamBuffer** | 连续数据流，无消息边界 |
| **MessageBuffer** | 离散消息，每个消息携带长度信息 |

支持阻塞/非阻塞操作，支持中断上下文通信。

#### 7. 事件组

32 位的位图，通过 `xEventGroupWaitBits` / `xEventGroupSetBits` 实现多事件同步。

#### 8. 调度器特性
- **抢占式调度**: 高优先级任务可抢占低优先级
- **任务选择**: 支持 CLZ 指令优化 (configUSE_PORT_OPTIMISED_TASK_SELECTION)
- **FromISR 接口**: 延迟调度机制，确保中断安全

---

## 命令行接口 (User/rtos_cli/)

### 传统 CLI (rtos_cli.c / rtos_cli.h)

基于 FreeRTOS-Plus CLI 的设计模式实现：

- **命令节点**: 命令名 + 帮助信息 + 处理函数 + 期望参数个数
- **注册机制**: 动态内存 (`FreeRTOS_CLIRegisterCommand`) 或静态内存 (`FreeRTOS_CLIRegisterCommandStatic`)
- **命令处理**: 前缀匹配，支持参数解析
- **命令示例**: `task-stats`, `echo-parameters`, `run-time-stats`, `query-heap`, `reboot`

### 重构 CLI (me_rtos_cli.c / me_rtos_cli.h)

自定义轻量级 CLI 实现，使用 ARMCC 的 `__attribute__((section("cmd_table")))` 将命令注册到自定义段：

```c
// 注册一个无参数命令
CMD_REGISTER_BASE(help, help: displays the supported commands);

// 注册一个指定参数个数的命令
CMD_REGISTER(taskinfo, taskinfo: displays all tasks information, 0);

// 注册一个不限参数个数的命令
CMD_REGISTER_DYNAMIC(echo, echo: echo parameters back);
```

通过 `cmd_table$$Base` 和 `cmd_table$$Limit` 遍历命令表，无需运行时注册。

### 命令行示例 (rtos_cli_example.c)

完整的 CLI 命令行应用示例：

- 使用互斥量 (`xTxMutex`) 实现多线程对串口的互斥访问
- 支持命令历史 (按 Enter 重复执行上一条命令)
- 已注册命令: `task-stats`, `echo-3-parameters`, `echo-parameters`, `reboot`, `help`, `timerstatus`

---

## LED 定时器控制 (User/led_timer/)

演示 FreeRTOS 软件定时器控制硬件 LED：

- `vStartLedTimers(n)`: 创建 n 个自动重载定时器，每个定时器以不同频率闪烁 LED
- 定时器回调函数通过 `pvTimerGetTimerID` 获取定时器 ID 控制对应 LED
- 硬件 LED 初始化: GPIOB_PIN_5, 推挽输出

---

## 中间件 (User/middleware/)

| 中间件 | 说明 | 状态 |
|---|---|---|
| **mbedTLS** | 密码学库 (散列、对称/非对称加密、X.509、TLS) | 已移植/测试中 |
| **wolfSSL** | 轻量级 SSL/TLS 实现 | 移植中 |
| **MQTT** | MQTT 客户端协议 (coreMQTT) | 移植中 |
| **HTTP** | HTTP 客户端协议 (coreHTTP) | 移植中 |
| **SNTP** | 简单网络时间协议 (coreSNTP) | 移植中 |
| **cJSON** | 轻量级 JSON 解析器 | 移植中 |
| **PKCS#11** | 密码令牌接口标准 (corePKCS11) | 移植中 |
| **TCP/IP** | TCP/IP 协议栈接口 | 移植中 |
| **Cellular** | 蜂窝网络接口 | 移植中 |
| **Y/ZModem** | YModem/ZModem 文件传输协议 | 已移植 |

### mbedTLS 移植详情 (参见 Doc/mbedtls移植.md)

已移植的散列函数:
- **MD5** (已测试)
- **SHA-1/SHA-256/SHA-512** (已测试)
- **SHA-3** (已移植)
- **RIPEMD-160** (已测试)

已移植的对称加密:
- **AES** (ECB/CBC/CFB/OFB/CTR/CCM/GCM)
- **ChaCha20**

已移植的非对称加密:
- **RSA** (依赖: md.c, bignum, constant_time, oid)
- **ECC/ECDSA/ECDH**

测试代码位于 `User/middleware_test/mbedtls_test/`:
- `md5_test.c` - MD5 散列测试
- `sha_test.c` - SHA 系列测试
- `ripemd160_test.c` - RIPEMD-160 测试
- `aes_test.c` - AES 加解密测试
- `hmac_test.c` - HMAC 消息认证码测试

### PKCS#11 概念

PKCS#11 是公钥密码标准中定义的一套独立于设备的加密令牌接口：

| 组件 | 说明 |
|---|---|
| **Slot** | 插槽，代表一个密码硬件位置 |
| **Token** | 令牌，物理/逻辑的安全加密存储区域 |
| **Session** | 会话，App 与 Token 的连接 |
| **Object** | 对象，包含证书、公钥、私钥、对称密钥等 |
| **Mechanism** | 机制，定义了加解密/签名/验签/摘要等引擎接口 |

---

## 启动流程

```c
int main(void)
{
    // 可选: 看门狗初始化
    // drv_fwdgt_init(10);

    // 可选: 串口/调试初始化
    // serial_init();
    
    // 启动 CLI 命令行 
    vUARTCommandConsoleStart(configMINIMAL_STACK_SIZE * 3, (tskIDLE_PRIORITY + 1));
    
    // 可选的演示任务 (按需取消注释)
    // vStartTimerDemoTask(1000);            // 定时器演示
    // xRegisterSampleCLICommands();         // 注册 CLI 命令
    // xStartSemaphoreTasks(tskIDLE_PRIORITY + 4);  // 信号量演示
    // vStartLedTimers(1);                   // LED 闪烁
    // vStartTaskNotifyTask();               // 任务通知演示
    // vStartRecursiveMutexTasks();          // 递归锁演示
    // vCreateSuicidalTask(tskIDLE_PRIORITY+3);  // 任务自杀演示
    // vStartInterruptQueueTasks();          // 中断队列演示
    // xTaskCreate(xTask_RTC_showtime, ...); // RTC 时间显示

    vTaskStartScheduler();  // 启动调度器
    return 0;
}
```

---

## 构建与烧录

### 编译

使用 Keil MDK v5 打开 `Project/gd32f407_freertos.uvprojx`，编译或运行 build 脚本：

```bat
cd \Project\
\Keil_v5\UV4\UV4.exe -b gd32f407_freertos.uvprojx -o build_log.txt
\Keil_v5\ARM\ARMCC\bin\fromelf.exe --bin -o ./Bins/Project.bin ./Objects/Project.axf
```

### 内存布局

| 区域 | 起始地址 | 大小 | 内容 |
|---|---|---|---|
| **Flash (IROM1)** | 0x08000000 | 64KB (0x10000) | 代码 + 只读数据 |
| **RAM (IRAM1)** | 0x20000000 | 20KB (0x5000) | RW 数据 + ZI 数据 + 堆栈 |

启动文件预留:
- 栈 (Stack): 10KB (0x2800)
- 堆 (Heap): ~113KB (0x1C500)

> **注意**: 编译后可执行文件大小若超过 Flash/RAM 容量，程序将无法运行。请根据实际需求调整堆栈大小。

---

## 文档索引

| 文档 | 内容 |
|---|---|
| `Doc/ReadMe.md` | 项目搭建过程记录 |
| `Doc/ReadMe_Kernel.md` | FreeRTOS 内核移植笔记 |
| `Doc/ReadMe_App.md` | 应用层设计笔记 (串口、CLI、PKCS#11) |
| `Doc/mbedtls移植.md` | mbedTLS 移植详细记录 |
| `Doc/UTF-8-Kernel.md` | UTF-8 编码的 Kernel 笔记 |
| `Doc/UTF-8-App.md` | UTF-8 编码的 App 笔记 |

---

## 常见问题

### 中断优先级配置
`configKERNEL_INTERRUPT_PRIORITY` 和 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 配置不当会导致系统崩溃。CM4 的 NVIC 支持 16 级优先级 (4 位)，配置值范围为 255~32。

### 定时器队列溢出
在调度器启动前创建定时器不能超过 `configTIMER_QUEUE_LENGTH`，否则会导致死等。

### 系统堆/栈不足
编译成功但无法运行时，检查 Code + Data 大小是否超过了芯片的 Flash/RAM 容量。

### 多线程串口访问
多个任务同时访问串口时需使用互斥量保护发送队列，避免输出混乱。
