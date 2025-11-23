

# 目录结构

## Doc
## Libraries
1. **Board**
    - STM32
    - GD32
2. **CMSIS**
    - Core: CM3, CM4, CM7
## Project
## User
### app
### config
### drivers
### kservice
1. **bus**: i2c, spi, uart(485, rs232)
2. **dev**: key, led, touch, lcd, sram, flash...
3. **timer**: 定时器
4. **event**: 异步事件驱动(生产者/消费者, 消息订阅者/发布者)
5. **component**: 其他组件
### middleware
1. **fs**: 文件系统
2. **net**: 网络协议栈
3. **gui**: 图形界面
### test: 选中一个文件, 将内容复制到main.c中, 编译运行
1. **drv_test**: 驱动测试
2. **kservice_test**: 内核服务测试


# 





