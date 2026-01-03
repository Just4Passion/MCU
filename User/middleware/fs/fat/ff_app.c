
#include <stdio.h>

#include "ff_app.h"

BYTE workBuffer[1024] = {0};

/*文件系统对象, 全局唯一*/
FATFS fs;           //FatFs文件系统对象

/*文件对象: 操作的文件*/
FIL fnew;           //文件对象, 读写多个文件时, 定义多个变量即可

/*文件操作结果*/
FRESULT res_flash;  //文件操作结果

UINT fnum;                  //文件成功读写数量
BYTE readBuffer[1024] = {0};    //读缓冲区
BYTE textFileBuffer[] = "welcome to use yehuo STM32F407 development board, it is a good day to test FatFs\r\n"; //写缓冲区


/*
FatFs格式化flash, 读写flash
*/
void ff_test1()
{
    printf("\r\n================== SPI Flash FatFS test ===================\r\n");
    res_flash = f_mount(&fs, "1:", 1);
    /*格式化测试*/
    if (res_flash == FR_NO_FILESYSTEM)
    {
        printf(">> flash will format...\r\n");
        res_flash = f_mkfs("1:", NULL, workBuffer, sizeof(workBuffer));
        if (res_flash == FR_OK)
        {
            printf(">> flash formatted\r\n");
            /*格式化后先取消挂载, 再重新挂载*/
            res_flash = f_mount(NULL, "1:", 1);
            res_flash = f_mount(&fs, "1:", 1);
        }
        else
        {
            led_red_on();
            printf(">> flash format failed\r\n");
            while(1);
        }
    }
    else if (res_flash != FR_OK)
    {
        printf(">> external flash mount FatFs failed. (%d)\r\n",res_flash);
        printf(">> maybe spi flash init error.\r\n");
        while (1);
    } 
    else 
    {
        printf(">> flash mount successful\r\n");
    }

    /*写测试*/
    /*打开文件*/
    printf(">> FatFs write test: \r\n");
    res_flash = f_open(&fnew, "1:FatFs_write_test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res_flash == FR_OK)
    {
        printf(">> open file successful, will write data to file\r\n");
        res_flash = f_write(&fnew, textFileBuffer, sizeof(textFileBuffer), &fnum);
        if (res_flash == FR_OK)
        {
            printf(">> file write successful, write %d bytes, write data: %s\r\n", fnum, textFileBuffer);
        }
        else
        {
            printf(">> file write failed: %d\r\n", res_flash);
        }
        f_close(&fnew);
    }
    else
    {
        led_red_on();
        printf(">> create/open file failed\r\n");
    }

    /*读测试*/
    printf(">> FatFs read test: \r\n");
    res_flash = f_open(&fnew, "1:FatFs_write_test.txt", FA_OPEN_EXISTING | FA_READ);
    if (res_flash == FR_OK)
    {
        led_green_on();
        printf(">> open file successful, will read from file\r\n");
        res_flash = f_read(&fnew, readBuffer, sizeof(readBuffer), &fnum);
        if (res_flash == FR_OK)
        {
            printf(">> file read successful, read %d bytes, read data: %s\r\n", fnum, readBuffer);
        }
        else
        {
            printf(">> file read failed: %d\r\n", res_flash);
        }
        f_close(&fnew);
    }
    else
    {
        led_red_on();
        printf(">> open file failed\r\n");
    }

    /*关闭文件*/
    f_close(&fnew);

    /*不在使用文件系统， 取消挂载文件系统*/
    f_mount(NULL, "1:", 1);

    while(1);
}

/*
FatFs多项功能测试
    设备存储空间获取
    读写指针定位
    创建目录
    文件移动
    重命名
    文件或目录信息获取
*/
static FRESULT miscellaneous(void)
{
    DIR dir;
    FATFS *pfs;
    DWORD fre_clust, fre_sect, tot_sect; //簇, 扇区

    /*获取设备信息*/
    printf("\r\n>> flash get dev info\r\n");
    res_flash = f_getfree("1:", &fre_clust, &pfs);

    /*计算得到总扇区个数和空扇区个数*/
    tot_sect = (pfs->n_fatent - 2) * pfs->csize;
    fre_sect = fre_clust * pfs->csize;
    printf(">> flash space: %10lu KB, avaliable: %10lu KB\r\n", tot_sect * 4, fre_sect * 4);
    
    /*读写测试*/
    res_flash = f_open(&fnew, "1:FatFs_write_test.txt", FA_OPEN_EXISTING | FA_WRITE | FA_READ );
    if ( res_flash == FR_OK ) 
    {
        /*  文件定位 */
        res_flash = f_lseek(&fnew, f_size(&fnew)-1);
        if (res_flash == FR_OK) 
        {
            /* 格式化写入，参数格式类似printf函数 */
            f_printf(&fnew,"\nAdd new line, append to raw file: \n");
            f_printf(&fnew,">> flash space: %10lu KB, avaliable space: %10lu KB\r\n", tot_sect * 4, fre_sect * 4);
            /*  文件定位到文件起始位置 */
            res_flash = f_lseek(&fnew, 0);
            /* 读取文件所有内容到缓存区 */
            res_flash = f_read(&fnew, readBuffer, f_size(&fnew), &fnum);
            if (res_flash == FR_OK) 
            {
                printf(">> read file content: \n%s\n",readBuffer);
            }
        }
        f_close(&fnew);

        printf("\n********** 目录创建和重命名功能测试 **********\r\n");
        /* 尝试打开目录 */
        res_flash=f_opendir(&dir, "1:TestDir");
        if (res_flash != FR_OK) 
        {
            /* 打开目录失败，就创建目录 */
            res_flash=f_mkdir("1:TestDir");
        } 
        else 
        {
            /* 如果目录已经存在，关闭它 */
            res_flash=f_closedir(&dir);
            /* 删除文件 */
            f_unlink("1:TestDir/testdir.txt");
        }
        if (res_flash==FR_OK) 
        {
            /* 重命名并移动文件 */
            res_flash=f_rename("1:FatFs_write_test.txt",
                                            "1:TestDir/testdir.txt");
        }
    } 
    else 
    {
        printf("open file failed: %d\n",res_flash);
    }
}

