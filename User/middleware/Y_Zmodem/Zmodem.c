
#include "Zmodem.h"


/*
### 特点
1. **流式传输**: 无固定块大小, 支持动态数据流
2. **高性能**: 支持16K大窗口滑动窗口协议
3. **强校验**: 32位CRC校验
4. **全双工通信**: 支持数据传输与控制信号并行
5. **恢复能力**: 自带断点续传功能

### 协议格式
1. **帧类型与结构**
```
| ZPAD | ZDLE | FrameType | Header | Data | CRC |
```
	- ZPAD(1B): 固定值0x2A, 表示帧开始
	- ZDLE(1B): 0x18(转义字符)
	- FrameType(1B): 帧类型标识 
	- Header(4B): 包含序号/子类型(大端序)
	- Data(0~N B): 有效载荷. 默认最大1024字节, 可扩展为8KB
	- CRC(4B): CRC-32校验值

2. **关键帧类型**
| 类型名       | 十六进制 | 功能说明                          |
|--------------|----------|-----------------------------------|
| ZRQINIT      | 0x00     | 接收方发起初始化                  |
| ZRINIT       | 0x01     | 发送方响应初始化                  |
| ZSINIT       | 0x02     | 发送文件名/元数据                 |
| ZDATA        | 0x03     | 数据帧                            |
| ZACK         | 0x04     | 确认帧（带文件偏移量）            |
| ZFILE        | 0x05     | 文件信息帧（含文件名/大小/时间戳）|
| ZEOF         | 0x06     | 文件结束标志                      |
| ZFERR        | 0x07     | 错误通知                          |
| ZCRC         | 0x08     | CRC挑战请求                       |

3. **数据转义机制**
	- ZDLE转义: 遇到0x18、0x2A等特殊字符时插入ZDLE并异或0x40, 比如0x2A -> 0x18 0x6A
	- 连续ZPAD: 超时检测使用6个连续ZPAD

### 传输流程
1. **初始化**
    - 发送方广播"rz\r"启动ZMODEM接收
    - 接收方回应ZRQINIT帧
2. **协商参数**
    - 发送方回应ZSINIT帧(包含文件信息, 块大小等参数)
    - 接收方确认ZACK帧
3. **文件传输**
    - 发送方ZFILE帧
    - 接收方ZRPOS帧
    - 发送方ZDATA帧
    - 接收方ZACK帧
4. **结束**
    - 发送方ZEOF帧
    - 接收方ZRINIT帧(准备接收下一个文件)
    - 发送方ZFIN帧(结束会话)
    - 接收方回应OO, 会话终止


###传输流程
1、发送方发送ZRQINIT帧, 接收方回应ZRINIT帧
2、发送方发送文件名ZFILE， 接收方回应ZRPOS帧
3、发送方发送文件内容ZDATA帧, 接收方回应ZACK帧
4、发送方发送结束标志ZEOF帧, 接收方回应ZFIN帧
5、发送方发送OO结束会话, 接收方回应OO

### 包头信息
所有Zmodem帧都有包头, 可能是binary, 也可能是HEX形式
包含同样的原始信息：
    1个字节类型
    4个字节数据只是标志, 或者是数字的包类型质量标志

#### CRC Binary Header
以：ZPAD ZDLE ZBIN开头
然后：Type 4字节(flag or position)
全用ZDLE填充

* ZDLE A Type F3/P0 F2/P1 F1/P2 F0/P3 CRC-1 CRC-2

* ZDLE C Type F3/P0 F2/P1 F1/P2 F0/P3 CRC-1 CRC-2 CRC-3 CRC-4

*    ZDLE ZBIN Type
ZPAD ZDLE ZBIN ZDLE [4 flag/position bytes] - 4个ZDLE

#### CRC Hex Header
以：ZPAD ZPAD ZDLE ZHEX开头
然后：Type 4字节(flag or position)
全用ZDLE填充

* * ZDLE B Type F3/P0 F2/P1 F1/P2 F0/P3 CRC-1 CRC-2 CR LF XON

### 二进制数据包
ZDLE填充
数据包紧跟着包头



### 会话传输示例

1. 文件传输 —— one file, no errors, no CHANLLENGE, overlapped

Sender: "rz\r"
Sender: ZRQINIT
Receiver: ZRINIT
Sender: ZFILE
Receiver: ZRPOS
Sender: ZDATA data ...
Sender: ZEOF
Receiver: ZRINIT
Sender: ZFIN
Receiver: ZFIN
Sender: OO

2. 挑战串和命令下载 - Challenge and Command Download
Sender: "rz\r"
Sender: ZRQINIT(ZCOMMAND)
Receiver: ZCHALLENAGE(random-number)
Sender: ZACK(same-number)
Receiver: ZRINIT
Sender: ZCOMMAND, ZDATA
Receiver: (Performs Command), ZCOMPL
Sender: ZFIN
Receiver: ZFIN
Sender: OO

*/

// Zmodem协议控制字符
#define ZMODEM_ZPAD  '*'
#define ZMODEM_ZDLE  0x18
#define ZMODEM_ZBIN  'A'
#define ZMODEM_ZHEX  'B'
#define ZMODEM_ZDLEE 'a'

// Zmodem帧类型
#define ZRQINIT     0
#define ZRINIT      1
#define ZSINIT      2
#define ZACK        3
#define ZFILE       4
#define ZSKIP       5
#define ZNAK        6
#define ZABORT      7
#define ZFIN        8
#define ZRPOS       9
#define ZDATA       10
#define ZEOF        11
#define ZFERR       12
#define ZCRC        13
#define ZCHALLENGE  14
#define ZCOMPL      15
#define ZCAN        16
#define ZFREECNT    17
#define ZCOMMAND    18
#define ZSTDERR     19


typedef struct 
{

}ZMODEM_INIT_FRAME;

// Zmodem帧头结构
typedef struct {
    uint8_t type;
    uint32_t flags;
    uint32_t offset;
    uint32_t dataLen;
}ZMODEM_HEADER;

typedef struct 
{
    /*
    - ZPAD(1B): 固定值0x2A, 表示帧开始
	- ZDLE(1B): 0x18(转义字符)
	- FrameType(1B): 帧类型标识 
	- Header(4B): 包含序号/子类型(大端序)
	- Data(0~N B): 有效载荷
	- CRC(4B): CRC-32校验值
    */
    uint8_t zpad;
    uint8_t zdle;
    uint8_t frame_type;
    uint8_t header[4];
    uint8_t data[1024];
    uint32_t crc32;
}ZMODEM_FRAME;

/*
以字节为单位处理, 按位进行模2运算
CRC32多项式: 0xEDB88320

CRC初始值0xFFFFFFFF
字节左移24位, 变成32位, 与CRC异或
最高位为1, CRC右移1位, 与0xEDB88320异或
最高位为0, CRC右移1位
直到8bit移位完毕
*/
static uint32_t data_crc32(uint8_t *data, uint32_t len)
{
    const uint32_t polynomial = 0xEDB88320;
    uint32_t crc = 0xFFFFFFFF;
    uint32_t i = 0;
    uint8_t j = 0;
    for (i = 0; i < len; i++)
    {
        crc ^= (data[i] << 24);
        for (j = 0; j < 8; j++)
        {
            crc = ((crc & 0x80000000) != 0) ? ((crc << 1) ^ polynomial) : (crc << 1);
        }
    }
    return (crc ^ 0xFFFFFFFF);  //异或就是减法
}

static int zmodem_recv_rz_r()
{
    /* 接收"rz\r" */

    return 0;
}

static int zmodem_recv_ZRQINIT()
{
    /*接收发送方的ZRQINIT*/
    /*然后发送ZRINIT*/

    return 0;
}

static int zmodem_snd_ZRINIT()
{
    return 0;
}

static int zmodem_recv_ZFILE()
{

    return 0;
}

static int zmodem_snd_ZRPOS()
{
    return 0;
}

static int zmodem_recv_ZDATA()
{

    return 0;
}

static int zmodem_recv_ZEOF()
{

    return 0;
}

static int zmodem_recv_ZFIN()
{

    return 0;
}

static int zmodem_snd_ZFIN()
{

    return 0;
}

int zmodem_simple_receive()
{
    ZMODEM_FRAME frame;
    memset(&frame, 0, sizeof(frame));

    uint32_t filePos = 0;
    uint32_t expectedSeq = 0;

    zmodem_recv_rz_r();
    zmodem_recv_ZRQINIT();  //根据内容做一些变化
    /*发送ZRINIT初始化*/
    /*接收到广播, 发送ZRQINIT*/
    zmodem_snd_ZRINIT();
    /*接收对方的ZFILE*/
    zmodem_recv_ZFILE();
    /*发送ZRPOS*/
    zmodem_snd_ZRPOS();
    /*接收ZDATA*/
    zmodem_recv_ZDATA();

    /*如果接收到了ZEOF*/
    if (zmodem_recv_ZEOF())
    {
        /*发送ZRINIT, 进行下一个文件接收*/
        zmodem_snd_ZRINIT();
    }
    if (zmodem_recv_ZFIN())
    {
        zmodem_snd_ZFIN();
    }
    return 0;
}


static int zmodem_snd_ZCHALLENGE()
{

    return 0;
}

static int zmodem_recv_ZACK()
{

    return 0;
}

static int zmodem_snd_ZCOMMAND()
{

    return 0;
}

static int zmodem_perform_cmd()
{
    return 0;
}
/*发送ZCOMPL*/
static int zmodem_snd_ZCOMPL()
{

    return 0;
}

int zmodem_cmd_receive()
{
    ZMODEM_FRAME frame;
    memset(&frame, 0, sizeof(frame));

    uint32_t filePos = 0;
    uint32_t expectedSeq = 0;

    zmodem_recv_rz_r();
    zmodem_recv_ZRQINIT();  //根据内容做一些变化 - ZCOMMAND
    /*发送ZRINIT初始化*/
    /*接收到广播, 发送ZCHALLENGE - 发送随机数*/
    zmodem_snd_ZCHALLENGE();
    /*接收对方的ZACK - 携带了随机数*/
    zmodem_recv_ZACK();
    /*发送ZRINIT*/
    zmodem_snd_ZRINIT();
    /*接收ZCOMMAND, ZDATA*/
    zmodem_recv_ZCOMMAND();
    zmodem_recv_ZDATA();
    /*执行接收到了命令*/
    zmodem_perform_cmd();
    /*发送ZCOMPL*/
    zmodem_snd_ZCOMPL();

    /*如果接收到了ZFIN*/
    if (zmodem_recv_ZFIN())
    {
        /*发送ZFIN*/
        zmodem_snd_ZFIN();
    }
    return 0;
}
