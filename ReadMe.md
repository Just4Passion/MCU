# TinyOS - 基于ARM Cortex-M3/M4的MCU操作系统学习项目

## 项目简介

本项目基于书籍 **《ARM Cortex-M3/M4权威指南》**，以 **GD32F30x** 系列MCU为硬件平台，从零开始学习和设计嵌入式实时操作系统(RTOS)的核心机制。

> **项目目标**：通过逐步迭代实现操作系统的核心功能——任务上下文切换、任务调度、中断管理、SVC系统调用等，深入理解ARM Cortex-M内核架构与OS设计的底层原理。

---

## 硬件平台

| 项目 | 说明 |
|------|------|
| **MCU** | GD32F30x (ARM Cortex-M3/M4，GigaDevice) |
| **主频** | 最高 120MHz |
| **Flash** | 最高 1024KB |
| **SRAM** | 最高 96KB |
| **IDE** | Keil MDK (µVision) |
| **编译器** | ARM Compiler version 5 |
| **调试器** | SEGGER J-Link |
| **外设** | USART0 (调试串口)、TIMER7 (PWM/呼吸灯)、GPIO (LED指示) |

### LED引脚定义

| 功能 | 引脚 | 说明 |
|------|------|------|
| 绿灯 (LED0) | PA6 | 高电平点亮 |
| 蓝灯/红灯 (LED1) | PA7 | 高电平点亮 |

---

## 目录结构

```
TinyOS/
├── ReadMe.md                  # 项目说明文档
├── Doc/                       # 文档目录
│   └── ReadMe.md              # 编译说明
├── FreeRTOS/                  # FreeRTOS 内核源码（参考学习）
│   ├── include/               # FreeRTOS 头文件
│   ├── port/                  # 移植层
│   │   ├── MemMang/           # 内存管理方案
│   │   └── RVDS/              # ARM RVDS 编译器移植
│   └── src/                   # FreeRTOS 内核源码
│       ├── tasks.c            # 任务管理
│       ├── queue.c            # 队列
│       ├── timers.c           # 软件定时器
│       ├── event_groups.c     # 事件组
│       ├── stream_buffer.c    # 流式缓冲区
│       ├── croutine.c         # 协程
│       └── list.c             # 链表
├── Libraries/                 # 芯片外设库
│   ├── Board/                 # 板级支持包
│   │   └── GD32/              # GD32 开发板相关
│   └── CMSIS/                 # ARM CMSIS 标准库
│       └── Core/              # CMSIS-Core 核心层
├── User/                      # 用户代码（项目核心代码）
│   ├── main.c                 # 程序入口
│   ├── config/                # 芯片配置
│   │   └── GD/                # GD32 配置
│   ├── system/                # ★ OS核心实现
│   ├── debug/                 # 串口调试终端
│   ├── drivers/               # 硬件驱动
│   └── breath_led/            # 呼吸灯驱动
└── Project/                   # Keil 工程文件
    ├── TinyOS.uvprojx         # 主工程文件
    ├── TinyOS.uvoptx          # 工程选项配置
    ├── TinyOS.uvguix.*        # 用户界面配置
    ├── Bins/                  # 编译产物（bin/hex）
    ├── Listings/              # 汇编列表文件
    └── Objects/               # 编译中间文件
```

---

## 核心源码详解

### 1. 程序入口 (`User/main.c`)

```c
int main(void)
{
    system_mini_init();     // 系统时钟 & NVIC 初始化
    debug_init();           // 串口调试初始化
    main_os();              // 启动OS任务系统
    while(1)
    {
        debug_deal();       // 处理串口命令
        my_delay(100);
    }
}
```

### 2. 系统管理 (`User/system/system_mng.c`)

系统管理模块实现了MCU底层的基础配置：

| 功能 | 说明 |
|------|------|
| `system_mini_init()` | 系统时钟初始化、AFIO使能、中断向量偏移(0x10000)、优先级分组(抢占2位/子优先级2位) |
| `system_info_print()` | 打印堆栈信息、向量表信息、系统复位源 |
| `systick_config()` | 系统滴答定时器(1ms中断)，优先级0x01 |
| `svc_config()` / `svc_trigger()` | SVC异常配置(优先级0x0E)与触发 |
| `pend_sv_config()` / `pend_sv_trigger()` | PendSV异常配置(最低优先级0x0F)与触发 |

**优先级分组**：NVIC_PRIGROUP_PRE2_SUB2（2位抢占优先级 + 2位子优先级）

**中断向量重定向**：`nvic_vector_table_set(NVIC_VECTTAB_FLASH, 0x10000)` - 向量表偏移至0x08010000

### 3. OS核心实现 - 三个演进版本

项目包含三个不同阶段的OS任务切换实现，展示了学习过程中的逐步演进：

#### 版本1：经典PendSV任务切换 (`os_pendsv_class.c`)

- **特点**：最基本的任务切换实现
- **任务数**：4个任务（task0~task3）
- **调度方式**：时间片轮转（每5000个SysTick中断切换一次）
- **栈管理**：每个任务独立栈（1024个long long），任务栈指针保存在`PSP_array[]`数组中
- **上下文切换**：PendSV中断中手动保存/恢复R4~R11（硬件自动保存R0~R3,R12,LR,PC,xPSR）
- **栈帧初始化**：手动在任务栈中填充初始PC和xPSR

#### 版本2：TCB结构化任务管理 (`os_pendsv_TCB.c`)

- **特点**：引入任务控制块(TCB)，结构化任务信息
- **TCB结构**：
  ```c
  typedef struct {
      void (*entry)(void);      // 任务入口函数
      uint32_t stack_ptr;       // 栈底地址
      uint32_t stack_size;      // 栈大小
      uint32_t psp_ptr;         // 进程栈指针
      uint32_t state;           // 任务状态（就绪/运行/挂起）
      uint32_t runing_time;     // 运行时间计数
  } tcb_t;
  ```
- **调度算法**：基于运行时间的简单轮转（运行时间到达5s后切换）
- **切换效率**：通过TCB结构体偏移量计算（24字节/任务），更规范地管理上下文

#### 版本3：FPU + 特权/非特权任务 (`os_pendsv_FPU_Priv_NonPriv.c`)

- **特点**：支持浮点运算单元(FPU) + 混合特权/非特权任务
- **增强的上下文**：
  - 增加S16~S31浮点寄存器的保存/恢复
  - 增加CONTROL寄存器和EXC_RETURN的保存
  - 栈帧扩展为18个寄存器（原本16个）
- **任务启动方式**：通过SVC系统调用（`SVC #0`）进入特权模式进行初始化
- **SVC Handler**：在特权模式下完成栈初始化、PSP设置、PendSV优先级配置、SysTick启动
- **CONTROL寄存器**：设置0x3（使用PSP + 非特权模式）

### 4. 调试终端 (`User/debug/`)

项目实现了完整的串口交互式调试终端：

| 命令 | 说明 |
|------|------|
| `help` | 显示帮助信息 |
| `reboot` | 系统软复位 |
| `pendsv` | 触发PendSV异常 |
| `svc` | 触发SVC异常 |
| `svc:1` ~ `svc:3` | 调用SVC服务（加法/减法/自增） |
| `timer7:N` | 设置TIMER7通道1占空比（呼吸灯亮度控制） |
| `led:10/11` | 绿灯灭/亮 |
| `led:20/21` | 蓝灯灭/亮 |
| `led:30/31` | 背光灭/亮 |
| `led:40/41` | 呼吸灯灭/亮 |

**串口配置**：
- 外设：USART0 (PA9-TX, PA10-RX)
- 波特率：115200
- 数据位：8位
- 停止位：1位
- 无校验

**接收缓冲区**：64字节，支持行编辑（Backspace）

**重定向**：通过 `fputc()` 重定向 `printf()` 到串口输出

### 5. USART驱动 (`User/drivers/drv_usart.c`)

基础USART驱动，提供：
- `drv_usart0_init()` - 初始化USART0（GPIO复用推挽输出 + 浮空输入）
- `usart_send_data()` - 阻塞式发送单字节数据
- `usart_recv_data()` - 阻塞式接收单字节数据

此外，还包含一个基于FreeRTOS队列的串口实现 (`User/debug/serial.c`)：
- 使用FreeRTOS队列进行发送和接收缓冲
- 中断驱动方式，非阻塞收发
- `serial_put_char()` / `serial_get_char()` / `serial_put_string()`

### 6. 呼吸灯驱动 (`User/breath_led/breath_led.c`)

基于TIMER7的PWM呼吸灯实现：

| 参数 | 值 |
|------|-----|
| 定时器 | TIMER7 |
| 通道 | CH1 (PC7) |
| 预分频 | 11 (120MHz/12 = 10MHz) |
| 自动重载 | 99999 (PWM周期10ms) |
| PWM模式 | 模式1 |
| 占空比范围 | 0 ~ 99999 |
| 中断频率 | 10ms (100Hz) |

**呼吸效果**：每40ms调整一次占空比，在150~300范围内渐变，实现平滑呼吸效果。

---

## 构建与烧录

### 开发环境要求

1. **Keil MDK** (µVision IDE) - 推荐使用v5以上版本
2. **ARM Compiler version 5** - 编译器配置
3. **SEGGER J-Link** 驱动 - 用于调试和烧录
4. **GD32 DFP (Device Family Pack)** - 芯片支持包

### 编译步骤

1. 使用 Keil µVision 打开 `Project/TinyOS.uvprojx`
2. 在工程配置中确认编译器为 **ARM Compiler version 5**
3. 点击 **Build (F7)** 编译工程
4. 编译产物位于 `Project/Bins/` 和 `Project/Objects/`

### 烧录与调试

- 使用 J-Link 连接目标板（SWD接口）
- 在Keil中点击 **Download (F8)** 烧录固件
- 串口调试终端连接：115200-8N1
- 复位后输出提示符 `root>`，即可输入调试命令

---

## 参考资料

- 《ARM Cortex-M3/M4权威指南》（Joseph Yiu 著）
- [GigaDevice GD32F30x 参考手册](https://www.gigadevice.com/)
- [FreeRTOS 官方文档](https://www.freertos.org/)
- [ARM CMSIS 文档](https://www.keil.com/pack/doc/CMSIS/)
- [ARM Cortex-M3 技术参考手册](https://developer.arm.com/)
