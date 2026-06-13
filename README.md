# MCU - STM32F407 嵌入式系统项目

> 基于 **设备-总线模型** 的嵌入式开发框架，采用 **事件驱动 + 软件定时器** 架构，以 **自顶向下** 和 **项目驱动** 的方式系统学习 Cortex-M4 MCU 开发。

---

## 目录

- [项目概述](#项目概述)
- [硬件平台](#硬件平台)
- [软件架构](#软件架构)
- [项目结构](#项目结构)
- [内核服务 (kservice)](#内核服务-kservice)
- [设备驱动 (drivers)](#设备驱动-drivers)
- [中间件 (middleware)](#中间件-middleware)
- [应用层 (app)](#应用层-app)
- [开发工具](#开发工具)
- [设计笔记与经验](#设计笔记与经验)
- [参考资料](#参考资料)

---

## 项目概述

本项目基于 **第一性原理** 和 **二八定律** 指导学习与实践：

- **第一性原理**：将嵌入式知识点分解为不可再分的元知识进行学习
- **二八定律**：聚焦 20% 的核心知识点（总线、定时器、中断、设备模型等）解决 80% 的工程问题
- **项目驱动**：通过微项目（PWM、SPWM、UI设计、文件系统等）驱动学习过程

### 设计哲学

- **设备-总线模型**：所有硬件外设通过总线挂载，统一管理
- **框架驱动**：基于事件 + 定时器的应用框架
- **模块化**：驱动、中间件、应用层分层清晰
- **可扩展**：支持新外设的快速接入

---

## 硬件平台

| 项目 | 详情 |
|------|------|
| **MCU** | STM32F407ZGTx (Cortex-M4, 168MHz) |
| **Flash** | 1024KB |
| **SRAM** | 192KB（含 64KB CCM SRAM） |
| **调试接口** | SWD/JTAG |
| **IDE** | Keil MDK v5 (UVision) |
| **启动文件** | `startup_stm32f40xx.s` |
| **标准外设库** | STM32F4xx Standard Peripheral Library |

### 已支持的板载外设

| 外设 | 驱动 | 状态 |
|------|------|------|
| USART1 (调试串口) | `drv_usart.c/h` | ✅ |
| 串口DMA传输 | `drv_usart.c/h` | ✅ |
| I2C (EEPROM) | `drv_i2c.c/h`, `drv_eeprom.c/h` | ✅ |
| SPI (Flash) | `drv_spi.c/h`, `drv_spi_flash.c/h` | ✅ |
| GPIO (LED/Key) | `drv_gpio.c/h`, `drv_led.c/h`, `drv_key.c/h` | ✅ |
| 定时器 (基本/OC/IC) | `drv_timer.c/h` | ✅ |
| RTC (实时时钟) | `drv_rtc.c/h` | ✅ |
| 独立看门狗 (IWDG) | `drv_watchdog.c/h` | ✅ |
| DAC (数模转换) | `drv_dac.c/h` | ✅ |
| RNG (随机数) | `drv_rng.c/h` | ✅ |
| FSMC-LCD (液晶屏) | `drv_fsmc_lcd.c/h` | ✅ |
| FSMC-SRAM (扩展内存) | `drv_fsmc_sram.c/h` | ✅ |

---

## 软件架构

```
┌─────────────────────────────────────────────────────────┐
│                    应用层 (app/)                          │
│  main.c (系统初始化, 主循环)  cmd_entry.c (调试控制台)     │
├─────────────────────────────────────────────────────────┤
│                    中间件 (middleware/)                   │
│     ASCII字库 (font)        FatFs文件系统 (fs/fat)        │
├─────────────────────────────────────────────────────────┤
│                  内核服务 (kservice/)                     │
│  ┌──────┐ ┌──────┐ ┌───────┐ ┌──────┐ ┌─────────────┐ │
│  │ 总线 │ │ 设备 │ │ 事件  │ │ 定时器│ │ 虚拟文件系统 │ │
│  │管理  │ │管理  │ │引擎   │ │管理器│ │ (VFS)        │ │
│  └──────┘ └──────┘ └───────┘ └──────┘ └─────────────┘ │
├─────────────────────────────────────────────────────────┤
│                    设备驱动层 (drivers/)                  │
│  USART I2C SPI GPIO TIM DAC RTC WDG FSMC KEY LED        │
├─────────────────────────────────────────────────────────┤
│           CMSIS + STM32F4xx 标准外设库                   │
├─────────────────────────────────────────────────────────┤
│                   STM32F407ZGTx (硬件)                   │
└─────────────────────────────────────────────────────────┘
```

### 主循环 (main loop)

```c
int main()
{
    board_init();   // 板级初始化: 时钟, 总线, 硬件驱动
    system_init();  // 系统初始化: 定时器管理器, 事件引擎, LCD
    app_init();     // 应用初始化: 创建定时器, 订阅事件
    while(1)
    {
        timer_manager_expired_timer_handler();  // 处理到期定时器回调
        event_process();                        // 处理事件队列
    }
}
```

---

## 项目结构

```
MCU/
├── README.md                         # 项目说明
├── Doc/
│   ├── ReadMe.md                     # 设计文档与学习笔记
│   ├── ILI9806G-Data Sheet.pdf       # LCD控制器数据手册
│   └── MCU设计.drawio.png            # 架构设计图
│
├── Libraries/
│   ├── Board/STM32/                  # 板级支持包
│   │   ├── STM32407_startup/         # 启动文件 & 系统配置
│   │   │   ├── include/              # stm32f4xx.h, system_stm32f4xx.h
│   │   │   └── src/                  # startup_stm32f40xx.s, system_stm32f4xx.c
│   │   └── STM32407_std_periph/      # STM32F4xx 标准外设库
│   │       ├── inc/                  # 外设头文件 (adc, tim, usart, spi...)
│   │       └── src/                  # 外设驱动源码
│   └── CMSIS/Core/                   # ARM CMSIS 核心文件
│       ├── core_cm4.h                # Cortex-M4 核心寄存器定义
│       ├── core_cmFunc.h             # 内核功能函数
│       └── core_cmInstr.h            # 内核内联指令
│
├── User/
│   ├── app/                          # 应用层
│   │   ├── main.c                    # 系统入口 & 初始化流程
│   │   ├── cmd_entry.c               # 串口调试控制台 (DMA接收)
│   │   └── system/                   # 系统（预留）
│   │
│   ├── config/                       # 项目配置文件
│   │   ├── stm32f4xx_conf.h          # 外设库配置文件
│   │   ├── stm32f4xx_it.c            # 中断服务函数
│   │   └── stm32f4xx_it.h            # 中断声明
│   │
│   ├── drivers/                      # 硬件设备驱动
│   │   ├── drv_usart.c/h             # USART 驱动
│   │   ├── drv_serial.c/h            # 串口设备抽象
│   │   ├── drv_i2c.c/h               # I2C 总线驱动
│   │   ├── drv_eeprom.c/h            # EEPROM 设备驱动
│   │   ├── drv_spi.c/h               # SPI 总线驱动
│   │   ├── drv_spi_flash.c/h         # SPI Flash 设备驱动
│   │   ├── drv_gpio.c/h              # GPIO 通用驱动
│   │   ├── drv_led.c/h               # LED 设备驱动
│   │   ├── drv_key.c/h               # 按键设备驱动
│   │   ├── drv_timer.c/h             # 定时器驱动 (基本/OC/IC)
│   │   ├── drv_time.c/h              # 系统时间管理
│   │   ├── drv_dac.c/h               # DAC 驱动
│   │   ├── drv_rtc.c/h               # RTC 驱动
│   │   ├── drv_rng.c/h               # 随机数生成器驱动
│   │   ├── drv_watchdog.c/h          # 看门狗驱动 (IWDG)
│   │   ├── drv_fsmc.h                # FSMC 配置头文件
│   │   ├── drv_fsmc_lcd.c/h          # FSMC-LCD 显示驱动
│   │   ├── drv_fsmc_sram.c/h         # FSMC-SRAM 扩展内存驱动
│   │   └── drv_misc_func.c/h         # 杂项功能函数
│   │
│   ├── kservice/                     # 内核服务框架
│   │   ├── kservice.h                # 核心头文件 (类型定义, 错误码, API)
│   │   ├── bus/bus.c                 # 总线管理器
│   │   ├── dev/device.c              # 设备管理器
│   │   ├── event/
│   │   │   ├── simple_event_engine.c # 简单事件引擎
│   │   │   └── simple_event_engine.h
│   │   ├── timer/
│   │   │   ├── timer_manager.c       # 软件定时器管理器
│   │   │   └── timer_manager.h
│   │   ├── vfs/vfs_core.c            # 虚拟文件系统核心
│   │   └── component/
│   │       ├── list.h                # 链表组件
│   │       └── queue.h               # 队列组件
│   │
│   ├── middleware/                   # 中间件
│   │   ├── font/                     # 字库
│   │   │   ├── ascii_fonts.c/h       # ASCII 字模 (8x16, 16x32, 24x48)
│   │   └── fs/fat/                   # 文件系统
│   │       └── ff_app.c/h            # FatFs 应用层封装
│   │
│   └── test/                        # 测试代码
│       ├── drv_test/                 # 驱动测试
│       ├── kservice_test/            # 内核服务测试
│       └── fs/                       # 文件系统测试
│
└── Project/                          # Keil MDK 工程文件
    ├── WD001.uvprojx                 # 项目工程文件
    ├── WD001.uvoptx                  # 项目选项配置
    ├── WD001.uvguix.22400            # 用户界面配置
    ├── EventRecorderStub.scvd        # 事件记录配置
    ├── DebugConfig/                  # 调试配置
    ├── Listings/                     # 编译列表文件
    │   ├── startup_stm32f40xx.lst    # 启动文件列表
    │   └── WD001.map                 # 内存映射文件
    ├── Objects/                      # 编译产物
    │   ├── WD001.axf                 # 可执行文件
    │   ├── WD001.sct                 # 链接脚本
    │   └── WD001.htm                 # 编译报告
    └── RTE/Device/                   # Run-Time Environment 配置
        └── STM32F407ZGTx/            # 芯片调试配置
```

---

## 内核服务 (kservice)

核心框架基于 **设备-总线模型**，提供统一的硬件抽象层。

### 设备模型 (Device Model)

```c
typedef struct device {
    char name[32 + 4];              // 设备名称
    int flag;                       // 设备标志 (激活/轮询/中断/DMA)
    int open_flag;                  // 打开标志 (只读/只写/读写)
    int state;                      // 设备状态
    device_ops_t *ops;              // 设备操作接口
    dy_bus_t *bus;                  // 所属总线
    struct device *next;            // 链表指针
    void *priv;                     // 私有数据
} dy_device_t;
```

**设备操作接口**：`init`, `open`, `close`, `read`, `write`, `control`, `callback`

### 总线模型 (Bus Model)

支持的总线类型：
- `DY_BUS_TYPE_I2C` - I2C 总线
- `DY_BUS_TYPE_SPI` - SPI 总线
- `DY_BUS_TYPE_USART` - UART 总线
- `DY_BUS_TYPE_CAN` - CAN 总线
- `DY_BUS_TYPE_USB` - USB 总线
- `DY_BUS_TYPE_CUSTOM` - 自定义总线

### 事件引擎 (Event Engine)

基于发布-订阅模式的事件驱动框架：

- **事件队列**：支持最多 32 个事件的队列
- **订阅管理**：最多 16 个订阅者
- **事件类型**：按键事件、定时器事件等
- **异步处理**：事件在主循环中顺序处理

### 软件定时器 (Software Timer)

- 最多 8 个软件定时器
- 支持 **单次模式** 和 **自动重载模式**
- 超时回调在 main 循环中执行（非中断上下文）
- 管理函数：`create`, `start`, `stop`, `restart`, `delete`, `set_period`

### 错误码系统

```c
#define DY_EOK       0    // 成功
#define DY_ERROR    (-1)  // 通用错误
#define DY_ENOMEM   (-2)  // 内存不足
#define DY_EINVAL   (-3)  // 无效参数
#define DY_ENOOBJ   (-4)  // 对象不存在
#define DY_ETMOUT   (-5)  // 超时
#define DY_EBUSY    (-6)  // 忙
#define DY_EEXIST   (-7)  // 已存在
#define DY_EBUSNOINIT (-8)// 总线未初始化
```

按模块分类的错误码前缀：
- `0x1000` - 存储相关
- `0x2000` - 平台设备相关
- `0x3000` - 网络相关

### 虚拟文件系统 (VFS)

提供了文件系统的抽象接口层：
- 文件操作：`open`, `close`, `read`, `write`, `seek`, `tell`
- 打开模式：读、写、创建、追加
- 支持挂载点管理

---

## 设备驱动 (drivers)

所有硬件驱动遵循统一的 **设备-总线** 注册模型：

```
1. 创建总线实例 → dy_bus_register()
2. 创建设备实例 → dy_device_create(attach_size)
3. 注册设备     → dy_device_register(name, dev)
4. 挂载到总线   → dy_bus_attach_device(bus, dev)
5. 查找使用     → dy_find_device("device_name")
```

### 驱动列表

| 驱动文件 | 设备名 | 总线 | 功能 |
|---------|--------|------|------|
| `drv_usart` | usart1_serial | USART1 | 串口通信 (115200, 8N1) |
| `drv_led` | led | - | RGB LED 控制 |
| `drv_key` | key1 | - | 按键检测 (按下/释放/长按) |
| `drv_eeprom` | eeprom | I2C1 | AT24Cxx EEPROM 读写 |
| `drv_spi_flash` | spi_flash | SPI1 | SPI Flash 读写 |
| `drv_rtc` | rtc | - | 实时时钟 (日期/时间) |
| `drv_watchdog` | iwdg | - | 独立看门狗 |
| `drv_timer` | timer6/9/10 | - | 定时器 (基本/OC/IC) |
| `drv_fsmc_lcd` | lcd | FSMC | 液晶显示屏 |
| `drv_fsmc_sram` | sram | FSMC | 外部SRAM扩展 |
| `drv_dac` | dac | - | 数模转换 |

### LCD 显示驱动特性

支持 ILI9806G 控制器驱动：
- **图形绘制**：直线、矩形（实心/空心）、圆形（实心/空心）
- **字符显示**：ASCII 字符/字符串、中文、混合中英文
- **字体支持**：8x16、16x32、24x48 三种字模，支持缩放
- **颜色支持**：16位色彩 (RGB565)
- **扫描模式**：8 种 GRAM 扫描方向可配置

### 调试控制台

通过 USART1 (115200-8N1) 提供交互式命令行：
- 支持回显、退格删除
- 内置命令：`help`, `reboot`
- 可扩展命令解析框架
- DMA 接收模式（可选）

---

## 中间件 (middleware)

### 字库 (Font)
- ASCII 字模：`ascii_fonts.c/h`
- 三种字模表：8x16、16x32、24x48
- 支持字模缩放

### 文件系统 (FatFs)
- 集成 FatFs 文件系统
- 提供挂载/卸载/扫描目录接口
- 针对 Flash 存储优化（读-修改-写入策略）

---

## 应用层 (app)

### 系统初始化流程

```
board_init()
├── systick_init(1000)          // SysTick 1ms 中断
├── dy_bus_manager_init()       // 总线管理器初始化
├── dy_device_manager_init()    // 设备管理器初始化
├── drv_usart_hw_init()         // USART 硬件初始化
├── drv_serial_init()           // 串口设备注册
├── drv_iwdg_init()             // 看门狗初始化
├── drv_fsmc_lcd_init()         // LCD 初始化
├── drv_led_init()              // LED 设备注册
├── drv_key_init()              // 按键设备注册
├── drv_rtc_init()              // RTC 初始化
└── set_rtc_time()              // 设置初始时间

system_init()
├── timer_manager_init()        // 软件定时器管理器
├── event_engine_init()         // 事件引擎初始化
└── lcd_init_show()             // LCD 初始化显示

app_init()
├── timer_create(10, key_cb, true)     // 按键扫描定时器 (10ms)
├── timer_create(2000, feed_iwdg, true) // 喂狗定时器 (2s)
├── event_subscribe(BUTTON_PRESS, ...)  // 订阅按键事件
├── event_subscribe(BUTTON_RELEASE, ...)
└── event_subscribe(BUTTON_LONG_PRESS, ...)
```

---

## 设计与开发笔记

> 详细的开发笔记、遇到的问题和解决方案见 [Doc/ReadMe.md](Doc/ReadMe.md)

### 微项目规划

1. **微项目一**：SPWM波、正弦波、全彩LED灯、呼吸灯、电容充放电检测
   - 学习定时器：定时、PWM、SPWM
   - 学习DAC：输出正弦波

2. **微项目二**：设计一个UI
   - LCD、SRAM、Flash
   - 图形驱动与硬件驱动分离设计
   - 核心接口：`OpenWindow` + `FillPixelData`

### 常见问题与解决方案

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| printf 阻塞 | 未使能 MicroLIB | 使能 Keil MicroLIB |
| HardFault | 堆栈越界/指针异常 | 使用 HardFault_Handler 打印堆栈分析 |
| LCD 闪屏 | 看门狗超时复位 | 优化绘制效率 / 增加喂狗频率 |
| 字体显示异常 | OpenWindow 边界计算错误 | `终点 = 起点 + 宽/高 - 1` |
| FatFs 崩溃 | 栈空间不足 | 扩大栈空间 / 增加栈溢出检测 |
| 定时器不触发 | 未启用 NVIC | 使能 NVIC 中断通道 |

### HardFault 调试方法

1. 通过 SP 指针定位堆栈帧
2. 打印 R0-R3, R12, LR, PC, xPSR 寄存器
3. 使用 `fromelf.exe -c --output list.txt WD001.axf` 反汇编
4. 根据 PC 值定位崩溃位置

### 开发原则

- **自顶向下学习**：先理解整体架构，再深入细节
- **设备-总线模型**：所有硬件统一抽象，便于替换和扩展
- **事件驱动**：解耦模块间通信，提高系统可维护性
- **迭代开发**：先实现核心功能，再逐步完善

---

## 开发工具

- **IDE**: Keil MDK v5 (μVision)
- **编译器**: ARM Compiler v5/v6
- **调试器**: J-Link / ST-Link
- **串口工具**: 115200-8N1
- **反汇编**: `fromelf.exe -c --text --output list.txt .\WD001.axf`

---

## 参考资料

- [STM32F4xx 标准外设库](https://www.st.com/en/embedded-software/stsw-stm32065.html)
- [ARM Cortex-M4 技术参考手册](https://developer.arm.com/documentation/ddi0439/latest/)
- [ILI9806G 数据手册](Doc/ILI9806G-Data%20Sheet.pdf)
- [FatFs 文件系统](http://elm-chan.org/fsw/ff/00index_e.html)
- [GitHub: Just4Passion/MCU](https://github.com/Just4Passion/MCU)
