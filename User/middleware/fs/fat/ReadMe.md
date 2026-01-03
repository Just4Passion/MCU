# FatFs文件系统概述
1. **FatFs源码下载**
    - 官网: https://elm-chan.org/fsw/ff/
    - 移植接口: https://elm-chan.org/fsw/ff/doc/appnote.html#port
2. **FatFs源码目录**: 下载R0.16
    - documents
    - source
        - 00readme.txt   This file.
        - 00history.txt  Revision history.
        - ff.c           FatFs module.
        - ffconf.h       Configuration file of FatFs module.
        - ff.h           Common include file for FatFs and application module.
        - diskio.h       Common include file for FatFs and disk I/O module.
        - diskio.c       An example of glue function to attach existing disk I/O module to FatFs.
        - ffunicode.c    Optional Unicode utility functions.
        - ffsystem.c     An example of optional OS related functions.
3. **FatFs源码概述**
    - ffconf.h: 功能配置裁剪文件
        - 每个配置项目有啥用: 全部复制进去, 让AI告诉你
        - #define FF_CODE_PAGE	936: 使用GBK
        - FF_USE_LFN: 长文件名
        - FF_VOLUMES: 物理设备数量
        - FF_MIN_SS, FF_MAX_SS: 扇区最大值和最小值
    - ff.c: 实现了文件系统的数据结构和软件逻辑, 实现了文件系统的用户接口
    - diskio.c: 给出了底层存储设备与ff.c对接的demo
    - ffunicode.c: 提供unicode支持, 可以使用中文目录/文件名
    - ffsystem.c: 

# FatFs文件系统移植
1. **添加文件到工程**: 根据需要添加源码文件到工程中
2. **实现移植接口**: diskio.c中只有这5个接口
    - disk_status
    - disk_initialize
    - disk_read
    - disk_write
    - disk_ioctl
        - disk_ioctl(CTRL_SYNC)
        - disk_ioctl(GET_SECOTR_COUNT)
        - disk_ioctl(GET_BLOCK_SIZE)
    - get_fattime: 获取时间戳, 使用RTC获取
















