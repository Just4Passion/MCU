
#include "drv_fwdgt.h"
#include "Ymodem.h"

/*****************************************************************************
一、Ymodem协议
包格式
    起始标志(1B) + 包序号(1B) + 包序号反码(1B) + 数据区(128/1024) + 校验字节(CRC-16)
    CRC-16默认使用多项0x1021进行计算

特殊字符
    SOH	0x01	128字节数据包
    STX	0x02	1024字节数据包
    EOT	0x04	结束传输
    ACK	0x06	正确接收回应
    NAK	0x15	错误接收回应
    CAN	0x18	传输中止
    C	0x43	请求数据

传输流程

1、Receiver: 发送'C'
2、Sender: 发送起始包
    SOH + 00 + FF + 文件名\0文件大小\0(128字节, 0x00填充) + CRC-16
    0x01 0x00 0xFF 文件名\0文件大小\0 CRC-16 —— 文件名大小使用16进制ASCII码
3、Receiver: 发送ACK
4、Sender: 发送数据包
    SOH + 序号 + 反序号 + 数据区(128字节, 0x1A填充) + CRC-16
    STX + 序号 + 反序号 + 数据区(1024字节, 0x1A填充) + CRC-16
5、Recevier: 发送ACK
6、循环4-5
7、结束传输
    结束方可以发送EOT(0x04)
    结束方可以发送空包(数据区全部填充0x00)
    结束方可以超时不回复
二、模块实现
1、串口收发接口
2、报文解析
3、CRC校验
******************************************************************************/

#if 0

#define YMODEM_SOH 0x01
#define YMODEM_STX 0x02
#define YMODEM_EOT 0x04
#define YMODEM_ACK 0x06
#define YMODEM_NAK 0x15
#define YMODEM_CAN 0x18
#define YMODEM_C   0x43

#define SERIAL_TIMEOUT 1000

typedef enum
{
    YMODEM_OK,
    YMODEM_SOH_ERR,
    YMODEM_READ_SOH_TIMEOUT,
    YMODEM_SN_ERR,
    YMODEM_READ_SN_TIMEOUT,
    YMODEM_READ_DATA_TIMEOUT,
    YMODEM_CRC_ERR
}YMODEM_ERR;

/*状态机*/
typedef enum 
{ 
    YMODEM_STATE_INIT, 
    YMODEM_STATE_HEADER, 
    YMODEM_STATE_DATA, 
    YMODEM_STATE_END 
} YMODEM_STATE;


typedef struct {
    char filename[128];
    uint32_t filesize;
    uint16_t mode;
    uint32_t timestamp;
}YMODEM_HEADER;

typedef struct
{
    uint8_t soh_stx;
    uint8_t sn;     //序号
    uint8_t sn_c;   //序号反码
    uint8_t data[1024 + 2];
}YMODEM_FRAME;

/*
关于CRC循环校验码
1、基本数学原理
（1）本质：二进制多项式除法
（2）过程：将数据视为一个二进制多项式，与预定义的生成多项式进行模2除法
（3）结果：余数作为CRC校验码
2、计算步骤
（1）数据预处理，在原始数据末尾追加n个0(n为校验码位数)
（2）多项式除法
    用扩展后的数据除以生成多项式，得到余数
3、重点解释
（1）模2运算：包括（加、减、乘、除），忽略进位与借位
（2）模2加法/减法：加法等价于或运算; 减法等价于异或运算
（3）模2乘法：按位与后左移，用模2加法累加部分积
（4）模2除法：通过逐位异或实现，是模2减法的重复应用
3、实现
（1）直接计算
    对于大块数据来说, 可以看成一个超大数, 除以预定义的多项式，得到的余数即为校验码
    但是超大数在计算机上无法直接计算
（2）按位计算
    除法 —— 也就是进行异或，循环减去被除数
    高位对齐
    异或，得到余数补齐后续位
    0左移一位丢弃；非0对齐，再异或
    示例：
        1110 % 101
            111 ^ 101 = 010 —— 高3位异或, 0100
            100 ^ 101 = 001 —— 0左移丢弃, 100 ^ 101
*/
static uint16_t data_crc16(const uint8_t *data, uint32_t length)
{
    uint16_t crc = 0;
    uint8_t i = 0;
    while(length--)
    {
        crc = crc ^ ((uint16_t)*data++ << 8);
        for (i = 0; i < 8; ++i)
        {
            /*最高位是1, 则与0x1021异或; 最高位不是1, 则直接左移一位*/
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : crc << 1;
        }
    }
    return crc;
}

extern uint32_t serial_read(uint8_t* buf, uint32_t len, uint32_t timeout_ms);
extern uint32_t serial_write(const uint8_t data);

static int receive_file_header(YMODEM_HEADER *header)
{
    /*SOH + 00 + FF + 128 data + 2 CRC*/
    /*头, 默认使用YMODEM_SOH_FRAME*/
    uint8_t *pfilesize = NULL;
    uint16_t crc16 = 0;
    YMODEM_FRAME frame;
    /*读取帧头数据*/
    do
    {
        /*我要不断的调用接收函数, 知道接收到起始字符为止*/
        if (1 != serial_read(&frame.soh_stx, 1, SERIAL_TIMEOUT))
        {
            return -1;  //读起始字符
        }
    }while(frame.soh_stx != YMODEM_SOH);
    
    /*读取帧序和帧序反码*/
    if (2 != serial_read(&frame.sn, 2, SERIAL_TIMEOUT))
    {
        return -1;  //读帧序
    }
    if (frame.sn != 0x00 && frame.sn_c != 0xFF)
    {
        return -1;  //序号错误
    }

    /*数据和CRC校验码*/
    if (130 != serial_read(&frame.data, 130, 5 * SERIAL_TIMEOUT))       //5s内完成接收
    {
        return -1;  //读数据错误 - 一般是超时
    }

    /*校验CRC*/
    crc16 = data_crc16(frame.data, 128);
    if (crc16 != ((frame.data[128] << 8) | frame.data[129]))
    {
        return -1;  //报文校验失败
    }
    
    /*解析文件名, 文件长度*/
    strncpy(header->filename, frame.data, sizeof(header->filename));
    pfilesize = &frame.data[strlen(header->filename) + 1];
    header->filesize = atoi(pfilesize) * 16;    //是ASCII 16进制码 —— 是吗？需要测试
    return 0;
}

int ymodem_receive(uint8_t *buffer, uint32_t buffsize, YMODEM_HEADER *header)
{
    uint8_t seq = 1;
    uint16_t crc16 = 0;
    uint32_t block_size = 0;
    uint32_t recv_bytes = 0;
    YMODEM_FRAME frame;

    /*发送'C'启动接收*/
    serial_write(YMODEM_C);
    if(receive_file_header(&header) != 0)
    {
        return -1;
    }
    serial_write(YMODEM_ACK);
    serial_write(YMODEM_C);

    /*接收数据块*/
    while(recv_bytes < header->filesize) {
        /*喂狗*/
        drv_fwdgt_feed();
        /*等待块开始*/
        if(serial_read(&frame.soh_stx, 1, SERIAL_TIMEOUT) != 1) 
        {
            return -1;
        }
        switch(frame.soh_stx)
        {
            case YMODEM_EOT:
                /*结束传输*/
                return -1;
            case YMODEM_SOH:
                block_size = 128;
                break;
            case YMODEM_STX:
                block_size = 1024;
                break;
        }
        /*读取数据*/
        if(2 != serial_read(&frame.sn, 2, SERIAL_TIMEOUT))
        {
            return -1;
        }
        if ((block_size +2) != serial_read(&frame.data, (block_size + 2), 10 * SERIAL_TIMEOUT))
        {
            return -1;
        }
        
        /*校验块号*/
        if(frame.sn != seq || frame.sn_c != (uint8_t)(~seq)) 
        {
            serial_write(YMODEM_NAK);
            continue;
        }
        
        /*校验码错误, 需要重传*/
        crc16 = data_crc16(&frame.data, block_size);
        if (crc16 != ((frame.data[block_size] << 8) | frame.data[block_size + 1]))
        {
            serial_write(YMODEM_NAK);
            continue;
        }
        
        memcpy(buffer + recv_bytes, frame.data, block_size);
        recv_bytes += block_size;
        seq++;
        serial_write(YMODEM_ACK);
    }

    /*结束传输*/
    serial_write(YMODEM_ACK);
    return recv_bytes;
}

int ymodem_receive() 
{ 
    /*
    输入download后
    进入YModem下载文件模式, 接收发送方的数据
    一般情况下debug处于接收命令状态; 执行download后, debug处于接收文件状态
    */
    YMODEM_STATE state = YMODEM_STATE_INIT; 
    while(1) 
    { 
        switch(state) 
        { 
            case YMODEM_STATE_INIT: 
            // 初始化流程 
            break; 
            case YMODEM_STATE_HEADER: 
            // 处理文件头 
            break; 
            // 其他状态... 
        }
             
    } 
    return 0;
}


#endif



