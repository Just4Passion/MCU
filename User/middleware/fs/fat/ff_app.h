
#ifndef FF_APP
#define FF_APP

#include "ff.h"
#include "diskio.h"

#define FATFS_LOGICAL_DRIVER_NUMBER "1:"


/**
 * @brief 挂载文件系统, 未格式化则进行格式化
 * @note 使用文件系统之前要先进行挂载
 */
int fatfs_mount();


/**
 * @brief 取消挂载文件系统
 * @note 不再使用文件系统, 则可以取消挂载
 */
int fatfs_unmount();


/**
 * @brief 显示指定目录下的所有文件
 * @note 
 */
int fatfs_scan_path(char* path);

#endif


