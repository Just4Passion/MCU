
# 1 概述
    - 文本主要描述我是如何一步一步搭建起来整个系统的
    - 采用整体后部分，先易后难的方式
    - ReadMe是UTF-8编码, 代码是GB2313编码
    - 芯片采用GD32F407

# 2 创建项目
## 2.1 目录结构

    ```
   GD32F407
    ├── Doc/
    │   └── ReadMe.md                                   # ReadMe.md
    ├── Libraries/
    │   ├── Board/
    │   │   └── GD32/
    │   │       ├── GD32F4xx_standard_peripheral/       # 固件库
    │   │       └── GD32F4xx_startup/                   # 启动文件和系统文件
    │   └── CMSIS/
    │       └── Core/
    │           ├── CM3/
    │           └── CM4/                                # 包含ARM核心支持文件
    ├── Project/                                        # 项目工程文件
    │       └── build.bat                               # 编译脚本
    └── User/
        ├── main.c
        ├── config/
        │   └── GD/
        │       └── GD32F4xx/                           # 配置文件, 空项目必备
        │           ├── gd32f4xx_it.c
        │           ├── gd32f4xx_it.h
        │           └── gd32f4xx_libopt.h
        └── drivers/                                    # 驱动文件

    ```

## 2.2 适配
    - gd32f4xx_libopt.h：包含所有固件库的头文件声明, 并根据设备型号条件编译某个芯片
    - gd32f4xx_it.c: 包含启动文件中导入的函数定义, 解决编译问题

## 2.3 编译
    - build.bat
    ```
    cd \Project\
    \Keil_v5\UV4\UV4.exe -b Project.uvprojx -o build_log.txt
    \Keil_v5\ARM\ARMCC\bin\fromelf.exe --bin -o ./Bin/Project.bin ./Objects/Project.axf
    ```

# 3 系统开发
    - 系统开发遵循先最小系统, 然后逐步增加的方式
    - 编译方式：使用Cline，使用下述提示词
    ```
    重新执行\Project\build.bat脚本，并检查编译结果是否存在编译报错，存在则给出修复意见
    ```

## 3.1 串口调试
    - 


## 3.2 Bootloader





