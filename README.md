# MCU - GD32F407 RT-Thread Nano 移植工程

## ? 项目简介

本项目是在 **GD32F407RE (Cortex-M4)** 微控制器上移植 **RT-Thread Nano** 实时操作系统的学习/实验工程。

RT-Thread Nano 是一个轻量级的实时操作系统内核，提供了多线程调度、线程间同步与通信、内存管理、软件定时器等核心功能。本项目通过逐步将 RT-Thread Nano 内核移植到 GD32F407 平台上，并在此基础上实现设备驱动框架，深入理解 RTOS 的工作原理和嵌入式系统开发。

## ? 项目特点

- 基于 **RT-Thread Nano** 最小内核（`RT_USING_NANO`）
- 支持 **优先级抢占式多线程调度**（32 个优先级）
- 实现了 **小内存管理算法**（small memory management）
- 支持 **信号量、互斥锁、邮箱、消息队列** 等 IPC 机制
- 支持 **软件定时器**
- 实现了 **串口调试 Shell**（支持 help、reboot 等命令）
- 具备 **栈溢出检查** 功能
- 完整的 **设备驱动框架**（从底层驱动到上层设备模型）

## ?? 硬件平台

| 项目 | 规格 |
|------|------|
| **MCU** | GD32F407RE (ARM Cortex-M4 with FPU) |
| **主频** | 72MHz (HSE) |
| **Flash** | 512KB (0x08000000 - 0x0807FFFF) |
| **SRAM** | 128KB (0x20000000 - 0x2001FFFF) |
| **CCM SRAM** | 64KB (0x10000000 - 0x1000FFFF) |
| **调试串口** | USART0 (PA9-TX, PA10-RX) |
| **波特率** | 115200 |

### 内存布局

```
+-------------------+ 0x20000000
|   ZI (BSS)        |
|   Heap (小内存算法) |
|   Stack           |
+-------------------+ 0x2001FFFF
               
+-------------------+ 0x10000000
|   CCM SRAM        |
+-------------------+ 0x1000FFFF

+-------------------+ 0x08010000
|   应用程序代码     |
+-------------------+ 0x0803FFFF
```

- ROM 起始地址: `0x08010000` (应用程序区, 192KB)
- 堆起始: ZI 段结束位置
- 堆结束: `0x20000000 + 96KB`
- SysTick: 1ms 一个 tick (`RT_TICK_PER_SECOND = 1000`)
- 栈空间: 10KB, 堆空间: 113KB

## ?? 软件架构

```
┌─────────────────────────────────────────────────────────────────┐
│                       用户应用层 (User)                          │
│   ┌──────────────────────────────────────────────────────────┐ │
│   │    main.c - 系统初始化与线程创建                           │ │
│   │    debug_sh/ - 串口调试 Shell (help, reboot)             │ │
│   └──────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                      RT-Thread 设备框架                          │
│   ┌──────────────────────────────────────────────────────────┐ │
│   │   RT-device/device.c     - 设备模型 (注册/查找/读写)      │ │
│   │   RT-device/serial_dev.c - 串口设备抽象层                 │ │
│   │   RT-device/usart_drv.c  - USART驱动适配层                │ │
│   └──────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                         驱动层                                   │
│   ┌──────────────────────────────────────────────────────────┐ │
│   │   driver/drv_usart.c - USART硬件驱动                     │ │
│   │   driver/drv_usart.h - 驱动结构体定义                    │ │
│   └──────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                    RT-Thread Nano 内核                          │
│   ┌──────────────────────────────────────────────────────────┐ │
│   │   src/  - 调度器, 线程管理, IPC, 内存管理, 定时器        │ │
│   │   libcpu/ - Cortex-M4 移植 (上下文切换, 中断)            │ │
│   └──────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                   CMSIS & 外设库                                │
│   ┌──────────────────────────────────────────────────────────┐ │
│   │   CMSIS - ARM CMSIS 核心层 (Cortex-M4)                  │ │
│   │   GD32F4xx_std_peripheral - GD32 标准外设库              │ │
│   └──────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## ? 目录结构

```
MCU/
├── Doc/                            # 文档
│   └── ReadMe.md                   # 开发笔记
├── Libraries/                      # 底层库
│   ├── CMSIS/                      # ARM CMSIS 标准库
│   │   ├── Include/                #   CMSIS 核心头文件 (core_cm4.h等)
│   │   └── Device/                 #   设备相关文件
│   │       ├── GD/                 #     GD32 设备文件
│   │       └── ST/                 #     ST 设备文件
│   └── GD32F4xx_standard_peripheral/ # GD32F4xx 标准外设库
│       ├── Include/                #   外设头文件 (ADC, CAN, USART等)
│       └── Source/                 #   外设源文件
├── RT-Thread/                      # RT-Thread Nano 内核源码 (精简版)
│   ├── bsp/                        #   板级支持包
│   │   └── gd32/                   #     GD32 系列 BSP
│   ├── components/                 #   组件
│   │   ├── drivers/                #     驱动框架
│   │   └── finsh/                  #     FinSH 控制台
│   ├── include/                    #   内核头文件
│   │   └── klibc/                  #     C 库适配
│   ├── libcpu/                     #   CPU 架构移植
│   │   └── arm/                    #     ARM 架构
│   │       ├── common/             #     通用代码
│   │       └── cortex-m4/          #     Cortex-M4 移植
│   └── src/                        #   内核源码
│       ├── clock.c                 #     时钟管理
│       ├── components.c            #     组件初始化
│       ├── cpu_up.c                #     单核 CPU 管理
│       ├── defunct.c               #     僵尸线程处理
│       ├── idle.c                  #     空闲线程
│       ├── ipc.c                   #     进程间通信 (信号量, 互斥锁, 邮箱, 消息队列)
│       ├── irq.c                   #     中断管理
│       ├── kservice.c              #     内核服务 (打印, 断言等)
│       ├── mem.c                   #     小内存管理算法
│       ├── memheap.c               #     内存堆管理
│       ├── mempool.c               #     内存池管理
│       ├── object.c                #     内核对象管理
│       ├── scheduler_comm.c        #     调度器通用部分
│       ├── scheduler_up.c          #     单核调度器
│       ├── signal.c                #     信号
│       ├── slab.c                  #     SLAB 内存管理
│       ├── thread.c                #     线程管理
│       └── timer.c                 #     定时器管理
├── RTKernel/                       # RT-Thread 完整内核 (完整功能版, 含更多组件)
│   ├── bsp/                        #   板级支持包
│   ├── components/                 #   组件 (dfs, drivers, finsh, libc, net等)
│   ├── include/                    #   内核头文件
│   ├── libcpu/                     #   CPU 架构移植 (支持多种架构)
│   └── src/                        #   内核源码
├── User/                           # 用户应用层
│   ├── main.c                      #   系统入口 & 初始化
│   ├── config/                     #   配置文件
│   │   ├── rtconfig.h              #     RT-Thread 内核配置
│   │   ├── gd32f4xx_it.c           #     中断服务函数
│   │   ├── gd32f4xx_it.h           #     中断服务头文件
│   │   └── gd32f4xx_libopt.h       #     外设库选择配置
│   ├── debug_sh/                   #   串口调试 Shell
│   │   ├── debug_entry.c           #     调试入口 (命令解析)
│   │   ├── serial.c                #     串口底层
│   │   └── serial.h                #     串口头文件
│   ├── driver/                     #   底层硬件驱动
│   │   ├── drv_usart.c             #     USART 驱动
│   │   ├── drv_usart.h             #     USART 驱动头文件
│   │   └── drv_conf.h              #     驱动配置
│   └── RT-device/                  #   RT-Thread 设备框架
│       ├── device.c                #     设备模型实现
│       ├── device.h                #     设备模型定义
│       ├── serial_dev.c            #     串口设备抽象
│       ├── serial_dev.h            #     串口设备头文件
│       ├── usart_drv.c             #     USART 驱动适配
│       └── usart_drv.h             #     USART 驱动适配头文件
└── Project/                        # Keil MDK 工程
    ├── gd32f407-rt.uvprojx         #   Keil 项目文件
    ├── gd32f407-rt.uvoptx          #   Keil 选项配置
    ├── gd32f407-rt.sct             #   分散加载文件
    ├── Objects/                    #   编译输出
    └── Listings/                   #   编译列表
```

## ?? RT-Thread Nano 配置特性

核心配置来自 `User/config/rtconfig.h`:

### 内核基础
| 配置项 | 值 | 说明 |
|--------|-----|------|
| `RT_USING_NANO` | 定义 | 启用 Nano 精简内核 |
| `RT_CPUS_NR` | 1 | 单核处理器 |
| `RT_NAME_MAX` | 8 | 对象名称最大长度 |
| `RT_ALIGN_SIZE` | 4 | 字节对齐大小 |
| `RT_TICK_PER_SECOND` | 1000 | 系统时钟节拍 (1ms) |
| `RT_THREAD_PRIORITY_MAX` | 32 | 最大线程优先级数 |
| `IDLE_THREAD_STACK_SIZE` | 256 | 空闲线程栈大小 |

### IPC (进程间通信)
| 功能 | 说明 |
|------|------|
| 信号量 (Semaphore) | 线程同步 |
| 互斥锁 (Mutex) | 互斥访问 |
| 邮箱 (Mailbox) | 线程间通信 (4字节消息) |
| 消息队列 (Message Queue) | 线程间通信 (可变长度消息) |

### 内存管理
| 功能 | 说明 |
|------|------|
| 小内存管理算法 | 启用 (`RT_USING_SMALL_MEM`) |
| 堆管理 | 小内存算法作为系统堆 (`RT_USING_SMALL_MEM_AS_HEAP`) |
| 内存池 | 独立功能，可选 |

### 其他特性
| 功能 | 说明 |
|------|------|
| 栈溢出检查 | 启用 (`RT_USING_OVERFLOW_CHECK`) |
| 软件定时器 | 启用，优先级 4，栈 256 字节 |

## ? 开发环境

| 工具 | 版本 |
|------|------|
| IDE | Keil MDK v5 (?Vision) |
| ARM Compiler | V5.06 update 7 (build 960) |
| 芯片支持包 | GigaDevice.GD32F4xx_DFP.3.0.3 |
| MCU | GD32F407RE |
| CPU | Cortex-M4 with FPU |

### 编译器预定义宏

```
USE_STDPERIPH_DRIVER,GD32F407,__RTTHREAD__,__CLK_TCK=RT_TICK_PER_SECOND
```

### 包含路径

```
..\Libraries\CMSIS\Device\GD\GD32F407\Include
..\Libraries\CMSIS\Include
..\Libraries\GD32F4xx_standard_peripheral\Include
..\RT-Thread\libcpu\arm\cortex-m4
..\RT-Thread\include
..\User\driver
..\User\config
..\User\RT-device
```

## ? 快速开始

### 1. 导入工程

打开 Keil MDK，选择 `Project` → `Open Project`，导航到 `Project/gd32f407-rt.uvprojx` 文件打开。

### 2. 编译

- 选择目标 `RT-gd32f407`
- 按 `F7` 或点击 `Project` → `Build Target` 编译
- 编译输出位于 `Project/Objects/` 目录

### 3. 下载与调试

- 连接调试器 (J-Link / ST-Link 等)
- 按 `Ctrl+F5` 下载并调试
- **注意**: 应用程序 ROM 起始地址为 `0x08010000`，需要配合 bootloader 使用

### 4. 串口调试

- 串口: USART0 (PA9-TX, PA10-RX)
- 波特率: 115200
- 数据位: 8
- 停止位: 1
- 校验位: 无

支持的命令:
- `help` — 显示帮助信息
- `reboot` — 系统复位

## ? 系统启动流程

```
1. 复位 -> 启动文件 (startup_gd32f407.s)
2. 系统初始化 (SystemInit)
3. main() 入口
   ├── rt_hw_interrupt_disable()     // 关闭中断
   ├── SysTick_Config()              // 配置系统滴答定时器
   ├── debug_init()                  // 初始化调试串口
   ├── rt_system_heap_init()         // 初始化系统堆
   ├── rt_system_timer_init()        // 初始化系统定时器
   ├── rt_system_scheduler_init()    // 初始化调度器
   ├── rt_application_init()         // 创建用户线程
   │   └── thread_debug_deal_entry   // 调试处理线程
   ├── rt_system_timer_thread_init() // 初始化定时器线程
   ├── rt_thread_idle_init()         // 初始化空闲线程
   ├── rt_thread_defunct_init()      // 初始化僵尸线程处理
   └── rt_system_scheduler_start()   // 启动调度器 (不再返回)
```

## ? 内核占用资源

| 资源 | 大小 |
|------|------|
| 栈空间 | 10KB (0x2800) |
| 堆空间 | 约 113KB (0x1C500) |
| 空闲线程栈 | 256 字节 |
| 定时器线程栈 | 256 字节 |
| 调试线程栈 | 256 字节 |
| CCM SRAM (备用) | 64KB (0x10000000) |

## ?? 开发路线图

参照 `Doc/ReadMe.md` 中的计划，后续开发方向：

- [ ] 接入设备框架 (device) - 已完成设备模型基础
- [ ] 完善驱动框架 (driver) - 已完成 USART 驱动
- [ ] 接入总线框架 (bus)
- [ ] 接入 FinSH 控制台 (finsh)
- [ ] 更多外设驱动支持 (SPI, I2C, CAN 等)
- [ ] 文件系统支持 (DFS)
- [ ] 网络协议栈支持 (lwIP)

## ? 参考资料

- [RT-Thread 官方文档](https://www.rt-thread.io/document/site/)
- [GD32F4xx 用户手册](https://www.gd32mcu.com/)
- [ARM Cortex-M4 技术参考手册](https://developer.arm.com/)

## ? 许可证

本项目基于开源协议发布，具体请参考各子模块的许可证信息。
