
#include <stdio.h>

#include "kservice.h"

#include "ff_app.h"

/***********************************************************
 * 可以在kservice.h中定义一个虚拟文件系统, 这样可以接入更多的异构文件系统
 * 
 * 
 ************************************************************/


typedef struct
{
    FATFS fat_fs;
    uint8_t *logical_driver_number;
    uint8_t partition_rule;     //flash分区方式: 0, FDISK; 1, SFDISK
    uint8_t mkfs_buffer[4 * 1024];
}FATFS_INFO;

FATFS_INFO g_fatfs_info = {
    .logical_driver_number = FATFS_LOGICAL_DRIVER_NUMBER,
    .partition_rule = 0,
};

/**
 * @brief 挂载文件系统, 未格式化则进行格式化
 * @note 使用文件系统之前要先进行挂载
 */
int fatfs_mount()
{
    int ret = 0;
    /************************
     * 1:, 逻辑驱动号. diskio.c中有一个物理驱动号的定义
     * 1, 表示立即挂载. 0, 表示延迟挂载
     *************************/
    ret = f_mount(&g_fatfs_info.fat_fs, g_fatfs_info.logical_driver_number, 1);
    if (ret == FR_NO_FILESYSTEM)
    {
        /************************
         * 没有文件系统, 则进行格式化
         *************************/
        ret = f_mkfs(g_fatfs_info.logical_driver_number, 
            NULL, g_fatfs_info.mkfs_buffer, sizeof(g_fatfs_info.mkfs_buffer)); //格式化
        if (ret == FR_OK)
        {
            /*重新挂载文件系统*/
            ret = f_mount(NULL, g_fatfs_info.logical_driver_number, 0); //取消挂载
            if (ret != FR_OK)
            {
                printf("unmount failed, ret = %d\r\n", ret);
                return DY_ERROR;
            }
            ret = f_mount(&g_fatfs_info.fat_fs, g_fatfs_info.logical_driver_number, 1); //挂载
            if (ret != FR_OK)
            {
                printf("remount failed, ret = %d\r\n", ret);
                return DY_ERROR;
            }
        }
        else
        {
            printf("f_mkfs failed, ret = %d\r\n", ret);
            return DY_ERROR;
        }
    }
    if (ret != FR_OK)
    {
        printf("mount fat fs failed, ret = %d\r\n", ret);
        return DY_ERROR;
    }
    return DY_EOK;
}

/**
 * @brief 取消挂载文件系统
 * @note 不再使用文件系统, 则可以取消挂载
 */
int fatfs_unmount()
{
    int ret = f_mount(NULL, g_fatfs_info.logical_driver_number, 1);
    if (ret != FR_OK)
    {
        printf("unmount fat fs failed\r\n");
        return DY_ERROR;
    }
    return DY_EOK;
}


/**
 * @brief 显示指定目录下的所有文件
 * @note 递归调用, 如果文件名太长或者目录太深都会出现异常
 */
int fatfs_scan_path(char* path)
{
    /*打开这个路径*/
    int ret = DY_EOK;
    FILINFO file_info;
    DIR dir;

    int tail = strlen(path);
    char *file_name;

    ret = f_opendir(&dir, path);
    if (ret != FR_OK)
    {
        printf("open dir failed, ret = %d\r\n", ret);
        return DY_ERROR;
    }

    while(1)
    {
        /*读取目录下的内容*/
        ret = f_readdir(&dir, &file_info);
        if (ret != FR_OK)
        {
            printf("read dir failed, ret = %d\r\n", ret);
            break;
        }
        if (file_info.fname[0] == 0)
        {
            break;
        }
        /**/
        file_name = file_info.fname;
        if (0 == strcmp(file_name, "."))
        {
            /*是自己, 不做处理*/
            continue;
        }
        /*是目录*/
        if (file_info.fattrib & AM_DIR)
        {
            /*是一个目录, 把这个目录打印出来*/
            snprintf(&path[tail], "%s", file_name);
            ret = fatfs_scan_path(path);
            path[tail] = 0;
            if (ret != FR_OK)
            {
                printf("recursive call fatfs_scan_path failed, ret = %d\r\n", ret);
                break;
            }
        }
        else
        {
            printf("%s/%s\r\n", path, file_name);
        }
    }
    if (ret != FR_OK)
    {
        return DY_ERROR;
    }
    return DY_EOK;
}

