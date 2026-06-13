
#include <stdbool.h>

#include "drv_time.h"

#include "ascii_fonts.h"

#include "drv_fsmc_lcd.h"

/****************************************
 * LCD的几个参数
 *      同步时钟信号: 使用哪个引脚同步时钟信号输入
 *      RGB信号: 
 *      水平同步信号: 
 *      垂直同步信号: 
 *      数据使能信号: 
 * 
 * FMSC
 *      使用什么类型的控制器: NOR Flash信号线
 *      数据线: D[15:0]
 *      地址线: A[25:0]
 *      NEx: 片选
 *      NOE: 输出使能
 *      NWE: 写使能
 *      NWAIT: 等待信号(输入)
 *      NADV: 地址、数据线复用时作锁存信号(输出)
 * 
 * LCD<->FSMC: 8080时序
 *      ILI9806G控制器
 *          根据IM[3:0]信号线电平决定与MCU的通信方式, 支持SPI/8080
 *      信号线
 *          LCD_DB[15:0]: 数据信号, RGB565格式
 *          LCD_RD: 读取, 低电平有效
 *          LCD_RS: 数据/命令信号, 高电平, 数据; 低电平, 控制命令
 *          LCD_RESET: 复位信号, 低电平有效
 *          LCD_WR: 写数据信号, 低电平有效
 *          LCD_CS: 片选信号, 低电平有效
 *          LCD_BK: 背光信号, 低电平点亮 
 *          RST: 触摸IC复位引脚
 *          INT: 触摸IC中断信号引脚
 *          SCL: 与触摸IC相连. IIC时钟
 *          SDA: 与触摸IC相连, IIC数据
 * 
 * 映射关系
 *      
 * 
 *****************************************/


/*数据线*/
typedef struct
{
    /*GPIO端口*/
    GPIO_TypeDef *gpio_d;
    /*GPIO引脚*/
    uint16_t gpio_dpin;
    /*GPIO复用关系*/
    //uint8_t gpio_daf;
}fsmc_DLine_t;

/*控制线*/
typedef struct
{
    GPIO_TypeDef *gpio_c;
    uint16_t gpio_cpin;
    //uint8_t gpio_caf;
}fsmc_CLine_t;

typedef struct 
{
    /*数据线*/
    fsmc_DLine_t dports[16];

    /*控制线*/
    fsmc_CLine_t cs;    //片选, FSMC_NE1<->LCD_CS
    fsmc_CLine_t oe;    //读取, FSMC_NOE<->LCD_RD
    fsmc_CLine_t we;    //写入, FSMC_NWE<->LCD_WR
    fsmc_CLine_t dc;    //数据/命令, FSMC_A0<->LCD_DC, 奇数/偶数地址自动转成类型控制电平
    fsmc_CLine_t rst;   //复位
    fsmc_CLine_t bkl;   //背光
}fsmc_lcd_hw_cfg_t;

typedef struct
{
    /*像素*/
    uint16_t width;
    uint16_t height;

    /*坐标原点*/
    uint16_t origin_x;
    uint16_t origin_y;

    /**/
    uint32_t cmd_addr;  //向这个地址写入数据, 会向LCD控制器发送命令
    uint32_t data_addr; //向这个地址写入数据, 会向LCD控制器发送数据

    /*GRAM扫描模式*/
    uint8_t gram_scan_mode; // 0-7
    uint16_t x_pixel;       // 不同扫描模式下X, Y的分辨率不同
    uint16_t y_pixel;       // 不同扫描模式下X, Y的分辨率不同

    /*当前颜色*/
    uint16_t cur_background_color;
    uint16_t cur_text_color;

    /*当前字体*/
    ascii_font_t *cur_fonts;

}fsmc_lcd_pw_cfg_t;

 /*把LCD映射到FSMC某个块区域上, 采用8080时序*/
typedef struct 
{
    fsmc_lcd_hw_cfg_t hw_cfg;
    fsmc_lcd_pw_cfg_t pg_cfg;
}fsmc_lcd_cfg_t;

/*LCD设备*/
typedef struct
{
    dy_device_t device;
    fsmc_lcd_cfg_t cfg;
}fsmc_lcd_dev_t;


/************************************************************************************************************************
                                                ILI9806G LCD 驱动接口
*************************************************************************************************************************/

/************************************************************
                    ILI9806G LCD 指令集
    系统控制类
        芯片复位, 睡眠模式, 显示模式切换
    显示设置类
        显示开关, 行列地址, 显示方向旋转, 像素格式设置
    电源管理类
        配置芯片内部电源泵, 调节VCOM电压, 确保液晶面板工作电压正常
    GRAM访问
        将像素数据连续写入到GRAM中, 点亮屏幕上的点
    Gamma矫正: 通过调整Gamma曲线, 可以优化屏幕在不同灰阶下的显示效果
*************************************************************/
/*
Column Address Set: 0x2A，4个参数，SC[15:8] SC[7:0] EC[15:8] EC[7:0]，参数表示列地址起始位置和结束位置
*/
#define ILI9806G_LCD_CMD_SetCoordinateX         0x2A
/*
Page Address Set: 0x2B，4个参数，SP[15:8] SP[7:0] EP[15:8] EP[7:0]，数表示一页的开始位置和结束位置
*/
#define ILI9806G_LCD_CMD_SetCoordinateY         0x2B
/*
Memory Write(RAMWR): 0x2C，N个参数: 像素数据，从SC和SP开始写入，发送其他任何命令都会终止 Write，支持D[23:0]写入, 即24bit
*/
#define ILI9806G_LCD_CMD_FillPixel               0x2C
/*
Memory Access Control: 0x36，定义帧内存读/写的扫描方向
    - 一帧数据
       0               1
        1 1 1 1 1 1 1 1 
        1 1 1 1 1 1 1 1 
        1 1 1 1 1 1 1 1 
        1 1 1 1 1 1 1 1 
        1 1 1 1 1 1 1 1
       2               3
        - 可以选择0为起点, 0->1为X, 0->2为Y; 0->1为Y, 0->2为X
        - 可以选择1为起点, 1->0为X, 1->3为Y; 1->0为Y, 1->3为X 
        - 可以选择2位起点, 2->0为X, 2->3为Y; 2->0为Y, 2->3为X
        - 可以选择3位起点, 3->1为X, 3->2为Y; 3->1为Y, 3->2为X
    - 参数: 1字节
        - D7: MY, 表示行地址方向. 0表示从上到下; 1表示从下到上
        - D6: MX, 表示列地址方向. 0表示从左到右; 1表示从右到左
        - D5: MV, 表示行列是否交换. 0, 行列不交换; 1表示行列交换
        - D4: ML, 垂直刷新方向
        - D3: BGR, 0 = RGB color filter panel, 1 = BGR color filter panel
        - D2: MH, 水平刷新方向.
        - D1: SS, Flip Horizontal, 选择源极驱动扫描方向. 0, 显存快照和屏幕快照相同; 1, 显存快照和屏幕快照基于竖轴对称, 比如 1 | 1, 左右是对称的
        - D0: GD, Flip Vertical, 选择栅驱动扫描方向. 0, 显存快照和屏幕快照相同; 1, 显存快照和屏幕快照基于横轴对称, 比如 十, 上下是对称的
   
*/
#define ILI9806G_LCD_CMD_SetScanDirection       0x36

/*向LCD控制器写入命令*/
static void fsmc_lcd_ILI9806G_write_cmd(dy_device_t *dev, uint16_t cmd)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    *(__IO uint16_t*)(fsmc_lcd->cfg.pg_cfg.cmd_addr) = cmd;
}

/*向LCD控制写数据*/
static void fsmc_lcd_ILI9806G_write_data(dy_device_t *dev, uint16_t data)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    *(__IO uint16_t*)(fsmc_lcd->cfg.pg_cfg.data_addr) = data;
}

/*从LCD控制器读取数据*/
static uint16_t fsmc_lcd_ILI9806G_read_data(dy_device_t *dev)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    return (*(__IO uint16_t*)(fsmc_lcd->cfg.pg_cfg.data_addr));
}

/**
 * @brief  初始化ILI9806G寄存器
 * @note:
 *      配置参数通常由屏幕模组厂商提供，确保芯片与特定面板完美匹配
 */
static  void fsmc_lcd_ILI9806G_registers_config(dy_device_t *dev)
{

    /*解锁扩展命令集*/
    fsmc_lcd_ILI9806G_write_cmd(dev, 0xFF); // EXTC Command Set enable register 
	fsmc_lcd_ILI9806G_write_data(dev, 0xFF); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x98); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x06); 

    /*设置SPI接口: 使用SDI和SDO*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xBA); // SPI Interface Setting 
	fsmc_lcd_ILI9806G_write_data(dev, 0x60); 

    /*GIP时序配置: GIP 是集成在屏幕玻璃上的栅极驱动电路，用于产生逐行扫描信号
    这些数值设定了栅极驱动器的扫描时序、波形宽度、偏移等，
    必须与面板的物理特性（如分辨率、像素排列）严格匹配，才能保证正常显示且无闪烁、无串扰
    */
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xBC); // GIP 1 
	fsmc_lcd_ILI9806G_write_data(dev, 0x01); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0E); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x61); 
	fsmc_lcd_ILI9806G_write_data(dev, 0xFB); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x10); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x10); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0B); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0F); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x2E); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x73); 
	fsmc_lcd_ILI9806G_write_data(dev, 0xFF); 
	fsmc_lcd_ILI9806G_write_data(dev, 0xFF); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0E); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0E); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x03); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x66); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x63); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x01); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 

	fsmc_lcd_ILI9806G_write_cmd(dev, 0xBD); // GIP 2 
	fsmc_lcd_ILI9806G_write_data(dev, 0x01); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x23); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x45); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x67); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x01); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x23); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x45); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x67); 

	fsmc_lcd_ILI9806G_write_cmd(dev, 0xBE); // GIP 3 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x21); 
	fsmc_lcd_ILI9806G_write_data(dev, 0xAB); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x60); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x22); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x22); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x22); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x22); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x22); 

    /*VCOM电压调节寄存器*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xC7); // Vcom 
	fsmc_lcd_ILI9806G_write_data(dev, 0x6F); 

    /*内部电压调节: VGMP / VGMN /VGSP / VGSN Voltage Measurement Set
    开启并配置电荷泵、参考电压等，为显示提供稳定的电源*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xED); // EN_volt_reg 
	fsmc_lcd_ILI9806G_write_data(dev, 0x7F); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0F); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 

    /*电源控制： 设置内部电源电压, 控制升压电路的输出幅度，确保栅极开启电压和关闭电压满足液晶驱动需求*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xC0); // Power Control 1
	fsmc_lcd_ILI9806G_write_data(dev, 0x37); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0B); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0A); 

    /*Low Voltage Gamma Control*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xFC); // LVGL 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0A); 

    /*工程设置*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xDF); // Engineering Setting 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x20); 

    /*数字核心电压设置*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xF3); // DVDD Voltage Setting 
	fsmc_lcd_ILI9806G_write_data(dev, 0x74); 

    /*配置液晶驱动极性反转方式（点反转、列反转等）*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xB4); // Display Inversion Control 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 

    /*分辨率配置*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xF7); // 480x800
	fsmc_lcd_ILI9806G_write_data(dev, 0x8A); 

    /*帧率配置*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xB1); // Frame Rate 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x12); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x13); 

    /*面板时序控制: 设定水平/垂直前后肩、同步脉冲宽度等，确保图像完整显示*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xF2); //Panel Timing Control 
	fsmc_lcd_ILI9806G_write_data(dev, 0x80); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x5B); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x40); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x28); 

    /*电源控制2: */
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xC1); // Power Control 2 
	fsmc_lcd_ILI9806G_write_data(dev, 0x17); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x7D); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x7A); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x20); 

    /*正负伽马校正曲线: 每个 16 个字节对应 16 个灰度级的电压微调。通过调整这些值，可以使屏幕色彩更真实、对比度合适、无偏色*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0xE0); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); //P1 
	fsmc_lcd_ILI9806G_write_data(dev, 0x11); //P2 
	fsmc_lcd_ILI9806G_write_data(dev, 0x1C); //P3 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0E); //P4 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0F); //P5 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0C); //P6 
	fsmc_lcd_ILI9806G_write_data(dev, 0xC7); //P7 
	fsmc_lcd_ILI9806G_write_data(dev, 0x06); //P8 
	fsmc_lcd_ILI9806G_write_data(dev, 0x06); //P9 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0A); //P10 
	fsmc_lcd_ILI9806G_write_data(dev, 0x10); //P11 
	fsmc_lcd_ILI9806G_write_data(dev, 0x12); //P12 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0A); //P13 
	fsmc_lcd_ILI9806G_write_data(dev, 0x10); //P14 
	fsmc_lcd_ILI9806G_write_data(dev, 0x02); //P15 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); //P16 

	fsmc_lcd_ILI9806G_write_cmd(dev, 0xE1); 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); //P1 
	fsmc_lcd_ILI9806G_write_data(dev, 0x12); //P2 
	fsmc_lcd_ILI9806G_write_data(dev, 0x18); //P3 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0C); //P4 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0F); //P5 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0A); //P6 
	fsmc_lcd_ILI9806G_write_data(dev, 0x77); //P7 
	fsmc_lcd_ILI9806G_write_data(dev, 0x06); //P8 
	fsmc_lcd_ILI9806G_write_data(dev, 0x07); //P9 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0A); //P10 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0E); //P11 
	fsmc_lcd_ILI9806G_write_data(dev, 0x0B); //P12 
	fsmc_lcd_ILI9806G_write_data(dev, 0x10); //P13 
	fsmc_lcd_ILI9806G_write_data(dev, 0x1D); //P14 
	fsmc_lcd_ILI9806G_write_data(dev, 0x17); //P15 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); //P16 

    /*撕裂效果: */
	fsmc_lcd_ILI9806G_write_cmd(dev, 0x35); //Tearing Effect ON 
	fsmc_lcd_ILI9806G_write_data(dev, 0x00); 

    /*像素格式配置*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0x3A);
	fsmc_lcd_ILI9806G_write_data(dev, 0x55);

    /*退出睡眠*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0x11); //Exit Sleep 

	drv_time_delay_ms(20);

    /*开启显示*/
	fsmc_lcd_ILI9806G_write_cmd(dev, 0x29); // Display On
}

/**
 * @brief  设置显存扫描方向
 * @param 8个显示方向: 0-7
 * @note:
 *      原点不同, 方向不同; 4个拐角就有4个原点, 每个原点有两种方向(上下X轴, 或者左右X轴)
 */
static  void fsmc_lcd_ILI9806G_GRAM_scan(dy_device_t *dev, uint8_t gram_scan_mode)
{
    if (gram_scan_mode > 7) { return; }
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;

    fsmc_lcd->cfg.pg_cfg.gram_scan_mode = gram_scan_mode;
    switch (gram_scan_mode)
    {
        case 0:
        case 2:
        case 4:
        case 6:
            fsmc_lcd->cfg.pg_cfg.x_pixel = 480;
            fsmc_lcd->cfg.pg_cfg.y_pixel = 800;
            break;
        default:
            fsmc_lcd->cfg.pg_cfg.x_pixel = 800;
            fsmc_lcd->cfg.pg_cfg.y_pixel = 480;
            break;
    }

    /*Memory Access Control*/
    fsmc_lcd_ILI9806G_write_cmd(dev, 0x36);
	fsmc_lcd_ILI9806G_write_data(dev, 0x00 | (gram_scan_mode << 5));

    /*Column Address Set: 4个参数*/
    fsmc_lcd_ILI9806G_write_cmd(dev, ILI9806G_LCD_CMD_SetCoordinateX);
	fsmc_lcd_ILI9806G_write_data(dev, 0x00);    //SC
    fsmc_lcd_ILI9806G_write_data(dev, 0x00);
    fsmc_lcd_ILI9806G_write_data(dev, ((fsmc_lcd->cfg.pg_cfg.x_pixel - 1) >> 8) & 0xFF);    //EC
    fsmc_lcd_ILI9806G_write_data(dev, (fsmc_lcd->cfg.pg_cfg.x_pixel - 1) & 0xFF);

    /*Page Address Set: 4个参数*/
    fsmc_lcd_ILI9806G_write_cmd(dev, ILI9806G_LCD_CMD_SetCoordinateY);
	fsmc_lcd_ILI9806G_write_data(dev, 0x00);    //SC
    fsmc_lcd_ILI9806G_write_data(dev, 0x00);
    fsmc_lcd_ILI9806G_write_data(dev, ((fsmc_lcd->cfg.pg_cfg.y_pixel - 1) >> 8) & 0xFF);    //EC
    fsmc_lcd_ILI9806G_write_data(dev, (fsmc_lcd->cfg.pg_cfg.y_pixel - 1) & 0xFF);

    /*Memory Write: 向GRAM中写入数据*/
    fsmc_lcd_ILI9806G_write_cmd(dev, ILI9806G_LCD_CMD_FillPixel);
}

/**
 * @brief  创建一个窗口
 * @note x的起点和终点; y的起点和终点
 * @retval 无
 */
void fsmc_lcd_ILI9806G_open_window(dy_device_t *dev, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{	
    uint16_t x_start = 0, x_end = 0;
    uint16_t y_start = 0, y_end = 0;

    x_start = x;
    x_end = x_start + width - 1;
    y_start = y;
    y_end = y_start + height - 1;

	/*Column Address Set: 4个参数*/
    fsmc_lcd_ILI9806G_write_cmd(dev, ILI9806G_LCD_CMD_SetCoordinateX);
	fsmc_lcd_ILI9806G_write_data(dev, (x_start >> 8) & 0xFF);    //SC
    fsmc_lcd_ILI9806G_write_data(dev, x_start & 0xFF);
    fsmc_lcd_ILI9806G_write_data(dev, (x_end >> 8) & 0xFF);    //EC
    fsmc_lcd_ILI9806G_write_data(dev, x_end & 0xFF);

	/*Page Address Set: 4个参数*/
    fsmc_lcd_ILI9806G_write_cmd(dev, ILI9806G_LCD_CMD_SetCoordinateY);
	fsmc_lcd_ILI9806G_write_data(dev, (y_start >> 8) & 0xFF);    //SC
    fsmc_lcd_ILI9806G_write_data(dev, y_start & 0xFF);
    fsmc_lcd_ILI9806G_write_data(dev, (y_end >> 8) & 0xFF);    //EC
    fsmc_lcd_ILI9806G_write_data(dev, y_end & 0xFF);
}

/**
 * @brief  设定ILI9806G的光标坐标
 * @note 光标的位置就是一个1像素的窗口
 * @retval 无
 */
static void fsmc_lcd_ILI9806G_set_cursor(dy_device_t *dev, uint16_t x, uint16_t y)	
{
	fsmc_lcd_ILI9806G_open_window(dev, x, y, 1, 1 );
}

/**
 * @brief  在ILI9806G显示器上以某一颜色填充像素点
 * @param  ulAmout_Point ：要填充颜色的像素点的总数目
 * @param  usColor ：颜色
 * @retval 无
 */
static __inline void fsmc_lcd_ILI9806G_fill_color(dy_device_t *dev, uint32_t amount_pixels, uint16_t color)
{
	uint32_t i = 0;
    /*写GRAM*/
	fsmc_lcd_ILI9806G_write_cmd(dev, ILI9806G_LCD_CMD_FillPixel);	
		
	for (i = 0; i < amount_pixels; i++)
    {
        fsmc_lcd_ILI9806G_write_data(dev, color);
    }
}

/**
 * @brief 对ILI9806G显示器的某一窗口以某种颜色进行清屏
 * @note 
 */
void fsmc_lcd_ILI9806G_clear(dy_device_t *dev, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
	fsmc_lcd_ILI9806G_open_window(dev, x, y, width, height);
	fsmc_lcd_ILI9806G_fill_color(dev, (width * height), fsmc_lcd->cfg.pg_cfg.cur_background_color);
}

/**
 * @brief  对ILI9806G显示器的某一点以某种颜色进行填充
 */
void fsmc_lcd_ILI9806G_set_point_pixel(dy_device_t *dev, uint16_t x, uint16_t y)
{	
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
	if ((x < fsmc_lcd->cfg.pg_cfg.x_pixel) && (y < fsmc_lcd->cfg.pg_cfg.y_pixel))
    {
		fsmc_lcd_ILI9806G_set_cursor(dev, x, y);
		
		fsmc_lcd_ILI9806G_fill_color(dev, 1, fsmc_lcd->cfg.pg_cfg.cur_text_color);
	}
}

/**
 * @brief  对ILI9806G显示器的(SC, SP)像素数据进行读取
 */
static uint16_t fsmc_lcd_ILI9806G_read_pixel(dy_device_t *dev)
{
    uint16_t pixel_ret = 0;
    uint16_t red = 0, green = 0, blue = 0;

    /*第一个是无效数据*/
    red = fsmc_lcd_ILI9806G_read_data(dev);

    /*读取颜色数据*/
    red = fsmc_lcd_ILI9806G_read_data(dev);
    blue = fsmc_lcd_ILI9806G_read_data(dev);
    green = fsmc_lcd_ILI9806G_read_data(dev);

    return (((red >> 11) << 11) | ((green >> 10) << 5) | (blue >> 11));  //RGB565
}

/**
 * @brief  对ILI9806G显示器, 先设置(SC, SP)值, 再对(SC, SP)像素数据进行读取
 */
static uint16_t fsmc_lcd_ILI9806G_get_pixel(dy_device_t *dev, uint16_t x, uint16_t y)
{
    uint16_t pixel_ret;

    fsmc_lcd_ILI9806G_set_cursor(dev, x, y);

    pixel_ret = fsmc_lcd_ILI9806G_read_pixel(dev);

    return pixel_ret;  //RGB565
}

/***********************************************************************************
                        ILI9806G LCD 图形显示驱动: 点, 线, 矩形, 圆
************************************************************************************/

/**
 * @brief  画直线
 */
void fsmc_lcd_ILI9806G_draw_line(dy_device_t *dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    int16_t dx, dy;
    int16_t x_inc, y_inc;
    int16_t error, error2;
    int16_t x, y;

    /* 计算X和Y方向的增量 */
    dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);

    /* 确定X方向的步进方向 */
    x_inc = (x1 < x2) ? 1 : -1;
    
    /* 确定Y方向的步进方向 */
    y_inc = (y1 < y2) ? 1 : -1;

    /* 初始化起始点 */
    x = x1;
    y = y1;

    /* Bresenham直线算法 */
    if (dx > dy)
    {
        /* X方向为主方向 */
        error = dx / 2;
        while (x != x2)
        {
            fsmc_lcd_ILI9806G_set_point_pixel(dev, x, y);
            error -= dy;
            if (error < 0)
            {
                y += y_inc;
                error += dx;
            }
            x += x_inc;
        }
    }
    else
    {
        /* Y方向为主方向 */
        error = dy / 2;
        while (y != y2)
        {
            fsmc_lcd_ILI9806G_set_point_pixel(dev, x, y);
            error -= dx;
            if (error < 0)
            {
                x += x_inc;
                error += dy;
            }
            y += y_inc;
        }
    }
    
    /* 绘制终点 */
    fsmc_lcd_ILI9806G_set_point_pixel(dev, x2, y2);
}

/**
 * @brief  画矩形
 */
void fsmc_lcd_ILI9806G_draw_rectangle(dy_device_t *dev, 
        uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height, 
        bool filled)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    uint16_t end_x, end_y;
    
    /* 检查参数有效性 */
    if ((start_x >= fsmc_lcd->cfg.pg_cfg.x_pixel) || (start_y >= fsmc_lcd->cfg.pg_cfg.y_pixel))
    {
        return;
    }
    
    /* 计算结束坐标 */
    end_x = start_x + width - 1;
    end_y = start_y + height - 1;
    
    /* 限制坐标在屏幕范围内 */
    if (end_x >= fsmc_lcd->cfg.pg_cfg.x_pixel)
    {
        end_x = fsmc_lcd->cfg.pg_cfg.x_pixel - 1;
        width = end_x - start_x + 1;
    }
    
    if (end_y >= fsmc_lcd->cfg.pg_cfg.y_pixel)
    {
        end_y = fsmc_lcd->cfg.pg_cfg.y_pixel - 1;
        height = end_y - start_y + 1;
    }
    
    if (filled)
    {
        /* 填充矩形 */
        fsmc_lcd_ILI9806G_open_window(dev, start_x, start_y, width, height);
        fsmc_lcd_ILI9806G_fill_color(dev, width * height, fsmc_lcd->cfg.pg_cfg.cur_text_color);
    }
    else
    {
        /* 绘制矩形边框（四条边） */
        /* 上边 */
        fsmc_lcd_ILI9806G_draw_line(dev, start_x, start_y, end_x, start_y);
        /* 右边 */
        fsmc_lcd_ILI9806G_draw_line(dev, end_x, start_y, end_x, end_y);
        /* 下边 */
        fsmc_lcd_ILI9806G_draw_line(dev, start_x, end_y, end_x, end_y);
        /* 左边 */
        fsmc_lcd_ILI9806G_draw_line(dev, start_x, start_y, start_x, end_y);
    }
}

/**
 * @brief  画圆
 * @note 使用中点圆算法绘制圆形，支持空心和填充模式
 */
void fsmc_lcd_ILI9806G_draw_circle(dy_device_t *dev, 
        uint16_t center_x, uint16_t center_y, uint16_t radius, 
        bool filled)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;
    
    /* 检查参数有效性 */
    if (radius == 0) {
        return;
    }
    
    /* 检查坐标是否在屏幕范围内 */
    if (center_x >= fsmc_lcd->cfg.pg_cfg.x_pixel || center_y >= fsmc_lcd->cfg.pg_cfg.y_pixel) {
        return;
    }
    
    if (filled) {
        /* 填充圆：使用中点圆算法计算每个y对应的x范围，然后填充水平线段 */
        while (x >= y) {
            /* 计算当前y对应的x范围 */
            int16_t start_x, end_x;
            
            /* 对于y，填充从center_x - x到center_x + x的水平线段在center_y + y和center_y - y位置 */
            start_x = center_x - x;
            end_x = center_x + x;
            
            /* 确保坐标在屏幕范围内 */
            if (start_x < 0) start_x = 0;
            if (end_x >= fsmc_lcd->cfg.pg_cfg.x_pixel) end_x = fsmc_lcd->cfg.pg_cfg.x_pixel - 1;
            
            /* 填充center_y + y的水平线 */
            if (center_y + y < fsmc_lcd->cfg.pg_cfg.y_pixel) {
                for (int16_t px = start_x; px <= end_x; px++) {
                    if (px >= 0 && px < fsmc_lcd->cfg.pg_cfg.x_pixel) {
                        fsmc_lcd_ILI9806G_set_point_pixel(dev, px, center_y + y);
                    }
                }
            }
            
            /* 填充center_y - y的水平线（如果y != 0） */
            if (y != 0 && center_y - y >= 0) {
                for (int16_t px = start_x; px <= end_x; px++) {
                    if (px >= 0 && px < fsmc_lcd->cfg.pg_cfg.x_pixel) {
                        fsmc_lcd_ILI9806G_set_point_pixel(dev, px, center_y - y);
                    }
                }
            }
            
            /* 对于x，填充从center_x - y到center_x + y的水平线段在center_y + x和center_y - x位置 */
            start_x = center_x - y;
            end_x = center_x + y;
            
            /* 确保坐标在屏幕范围内 */
            if (start_x < 0) start_x = 0;
            if (end_x >= fsmc_lcd->cfg.pg_cfg.x_pixel) end_x = fsmc_lcd->cfg.pg_cfg.x_pixel - 1;
            
            /* 填充center_y + x的水平线 */
            if (center_y + x < fsmc_lcd->cfg.pg_cfg.y_pixel) {
                for (int16_t px = start_x; px <= end_x; px++) {
                    if (px >= 0 && px < fsmc_lcd->cfg.pg_cfg.x_pixel) {
                        fsmc_lcd_ILI9806G_set_point_pixel(dev, px, center_y + x);
                    }
                }
            }
            
            /* 填充center_y - x的水平线（如果x != 0） */
            if (x != 0 && center_y - x >= 0) {
                for (int16_t px = start_x; px <= end_x; px++) {
                    if (px >= 0 && px < fsmc_lcd->cfg.pg_cfg.x_pixel) {
                        fsmc_lcd_ILI9806G_set_point_pixel(dev, px, center_y - x);
                    }
                }
            }
            
            y++;
            err += 1 + 2*y;
            if (2*(err - x) + 1 > 0) {
                x--;
                err += 1 - 2*x;
            }
        }
    } else {
        /* 空心圆：只绘制轮廓 */
        while (x >= y) {
            /* 绘制八分圆上的8个对称点 */
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x + x, center_y + y);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x + y, center_y + x);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x - y, center_y + x);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x - x, center_y + y);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x - x, center_y - y);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x - y, center_y - x);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x + y, center_y - x);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x + x, center_y - y);
            
            y++;
            err += 1 + 2*y;
            if (2*(err - x) + 1 > 0) {
                x--;
                err += 1 - 2*x;
            }
        }
    }
}

/**
 * @brief  画圆优化版本（v2）
 * @note 使用窗口填充优化填充圆的绘制过程，减少多次窗口设置开销
 */
void fsmc_lcd_ILI9806G_draw_circle_v2(dy_device_t *dev, 
        uint16_t center_x, uint16_t center_y, uint16_t radius, 
        bool filled)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    
    /* 检查参数有效性 */
    if (radius == 0) {
        return;
    }
    
    /* 检查坐标是否在屏幕范围内 */
    if (center_x >= fsmc_lcd->cfg.pg_cfg.x_pixel || center_y >= fsmc_lcd->cfg.pg_cfg.y_pixel) {
        return;
    }
    
    if (filled) {
        /* 优化填充圆：使用中点圆算法计算每一行的x范围，然后使用窗口命令一次性填充整行 */
        int16_t x = radius;
        int16_t y = 0;
        int16_t err = 0;
        
        /* 使用数组记录需要填充的行，避免重复设置窗口 */
        while (x >= y) {
            /* 计算当前y和x对应的水平线段范围 */
            int16_t start_x, end_x;
            
            /* 对于y，填充center_y ± y的两条水平线 */
            start_x = center_x - x;
            end_x = center_x + x;
            
            /* 限制坐标在屏幕范围内 */
            int16_t clamped_start_x = (start_x < 0) ? 0 : start_x;
            int16_t clamped_end_x = (end_x >= fsmc_lcd->cfg.pg_cfg.x_pixel) ? (fsmc_lcd->cfg.pg_cfg.x_pixel - 1) : end_x;
            
            if (clamped_start_x <= clamped_end_x) {
                /* 填充center_y + y的水平线 */
                int16_t target_y = center_y + y;
                if (target_y >= 0 && target_y < fsmc_lcd->cfg.pg_cfg.y_pixel) {
                    uint16_t width = clamped_end_x - clamped_start_x + 1;
                    fsmc_lcd_ILI9806G_open_window(dev, clamped_start_x, target_y, width, 1);
                    fsmc_lcd_ILI9806G_fill_color(dev, width, fsmc_lcd->cfg.pg_cfg.cur_text_color);
                }
                
                /* 填充center_y - y的水平线（如果y != 0） */
                if (y != 0) {
                    target_y = center_y - y;
                    if (target_y >= 0 && target_y < fsmc_lcd->cfg.pg_cfg.y_pixel) {
                        uint16_t width = clamped_end_x - clamped_start_x + 1;
                        fsmc_lcd_ILI9806G_open_window(dev, clamped_start_x, target_y, width, 1);
                        fsmc_lcd_ILI9806G_fill_color(dev, width, fsmc_lcd->cfg.pg_cfg.cur_text_color);
                    }
                }
            }
            
            /* 对于x，填充center_y ± x的两条水平线（如果x != y） */
            if (x != y) {
                start_x = center_x - y;
                end_x = center_x + y;
                
                clamped_start_x = (start_x < 0) ? 0 : start_x;
                clamped_end_x = (end_x >= fsmc_lcd->cfg.pg_cfg.x_pixel) ? (fsmc_lcd->cfg.pg_cfg.x_pixel - 1) : end_x;
                
                if (clamped_start_x <= clamped_end_x) {
                    /* 填充center_y + x的水平线 */
                    int16_t target_y = center_y + x;
                    if (target_y >= 0 && target_y < fsmc_lcd->cfg.pg_cfg.y_pixel) {
                        uint16_t width = clamped_end_x - clamped_start_x + 1;
                        fsmc_lcd_ILI9806G_open_window(dev, clamped_start_x, target_y, width, 1);
                        fsmc_lcd_ILI9806G_fill_color(dev, width, fsmc_lcd->cfg.pg_cfg.cur_text_color);
                    }
                    
                    /* 填充center_y - x的水平线（如果x != 0） */
                    if (x != 0) {
                        target_y = center_y - x;
                        if (target_y >= 0 && target_y < fsmc_lcd->cfg.pg_cfg.y_pixel) {
                            uint16_t width = clamped_end_x - clamped_start_x + 1;
                            fsmc_lcd_ILI9806G_open_window(dev, clamped_start_x, target_y, width, 1);
                            fsmc_lcd_ILI9806G_fill_color(dev, width, fsmc_lcd->cfg.pg_cfg.cur_text_color);
                        }
                    }
                }
            }
            
            y++;
            err += 1 + 2*y;
            if (2*(err - x) + 1 > 0) {
                x--;
                err += 1 - 2*x;
            }
        }
    } else {
        /* 空心圆：使用中点圆算法（与v1相同） */
        int16_t x = radius;
        int16_t y = 0;
        int16_t err = 0;
        
        while (x >= y) {
            /* 绘制八分圆上的8个对称点 */
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x + x, center_y + y);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x + y, center_y + x);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x - y, center_y + x);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x - x, center_y + y);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x - x, center_y - y);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x - y, center_y - x);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x + y, center_y - x);
            fsmc_lcd_ILI9806G_set_point_pixel(dev, center_x + x, center_y - y);
            
            y++;
            err += 1 + 2*y;
            if (2*(err - x) + 1 > 0) {
                x--;
                err += 1 - 2*x;
            }
        }
    }
}



/***********************************************************************************
                        ILI9806G LCD 字符显示驱动: 字模, 字体(字模缩放)
************************************************************************************/
void fsmc_lcd_ILI9806G_set_font_table(dy_device_t *dev, fsmc_lcd_font_table_t table_type)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    fsmc_lcd->cfg.pg_cfg.cur_fonts = &ascii_font_16x32;
}

const uint8_t* fsmc_lcd_ILI9806G_get_font_table(dy_device_t *dev, fsmc_lcd_font_table_t table_type)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    return (fsmc_lcd->cfg.pg_cfg.cur_fonts->font_table);
}

void fsmc_lcd_ILI9806G_display_ascii_char(dy_device_t *dev, int16_t x, int16_t y, const char c)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    uint8_t  byte_count, bit_count, font_bytes;
    uint8_t font_width, font_height;
    uint16_t font_position;
	uint8_t *font;
	
    /*字体宽度高度*/
    font_width = fsmc_lcd->cfg.pg_cfg.cur_fonts->width;
    font_height = fsmc_lcd->cfg.pg_cfg.cur_fonts->height;

	/*对ascii码表偏移（字模表不包含ASCII表的前32个非图形符号）*/
	font_position = c - ' ';
	
	/*每个字模的字节数*/
	font_bytes = (font_width * font_height) / 8;
	
	/*字模首地址, ascii码表偏移值乘以每个字模的字节数，求出字模的偏移位置*/
	font = (uint8_t *)&(fsmc_lcd->cfg.pg_cfg.cur_fonts->font_table[font_position * font_bytes]);
	
	/*设置显示窗口*/
	fsmc_lcd_ILI9806G_open_window(dev, x, y, font_width, font_height);
    fsmc_lcd_ILI9806G_write_cmd(dev, ILI9806G_LCD_CMD_FillPixel);

	/*按字节读取字模数据, 由于前面直接设置了显示窗口，显示数据会自动换行*/
	for (byte_count = 0; byte_count < font_bytes; byte_count++)
	{
        /*一位一位处理要显示的颜色*/
        for (bit_count = 0; bit_count < 8; bit_count++)
        {
                if (font[byte_count] & (0x80 >> bit_count))
                {
                    fsmc_lcd_ILI9806G_write_data(dev, fsmc_lcd->cfg.pg_cfg.cur_text_color);
                }
                else
                {
                    fsmc_lcd_ILI9806G_write_data(dev, fsmc_lcd->cfg.pg_cfg.cur_background_color);
                }
        }
	}
}

void fsmc_lcd_ILI9806G_display_ascii_string(dy_device_t *dev, int16_t x, int16_t y, const char *str)
{
	fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    uint8_t font_width, font_height;
    /*字体宽度高度*/
    font_width = fsmc_lcd->cfg.pg_cfg.cur_fonts->width;
    font_height = fsmc_lcd->cfg.pg_cfg.cur_fonts->height;

	while (*str != '\0')
	{
		if ((x - fsmc_lcd->cfg.pg_cfg.origin_x + font_width) > fsmc_lcd->cfg.pg_cfg.x_pixel)
		{
			x = fsmc_lcd->cfg.pg_cfg.origin_x;
			y += font_height;
		}
		
		if ((y - fsmc_lcd->cfg.pg_cfg.origin_y + font_height) > fsmc_lcd->cfg.pg_cfg.y_pixel)
		{
			x = fsmc_lcd->cfg.pg_cfg.origin_x;
			y = fsmc_lcd->cfg.pg_cfg.origin_y;
		}
		
		fsmc_lcd_ILI9806G_display_ascii_char(dev, x, y, *str);
		
		str++;
		x += font_width;
		
	}
}


/************************************************************************************************************************
                                                ILI9806G LCD 设备抽象接口
*************************************************************************************************************************/

fsmc_lcd_dev_t g_lcd_dev;

static int fsmc_lcd_init(dy_device_t *dev)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;

    uint8_t i = 0;
    GPIO_InitTypeDef  stGPIOInit;
    stGPIOInit.GPIO_OType = GPIO_OType_PP;
    stGPIOInit.GPIO_PuPd = GPIO_PuPd_UP;
    stGPIOInit.GPIO_Mode = GPIO_Mode_AF;
    stGPIOInit.GPIO_Speed = GPIO_Speed_100MHz;

    /*****************************************
     *              数据端口初始化
     *****************************************/
    for (i = 0; i < 16; ++i)
    {
        /*时钟*/
        drv_gpio_enable_clk(fsmc_lcd->cfg.hw_cfg.dports[i].gpio_d);
        /*初始化*/
        stGPIOInit.GPIO_Pin = fsmc_lcd->cfg.hw_cfg.dports[i].gpio_dpin;
        GPIO_Init(fsmc_lcd->cfg.hw_cfg.dports[i].gpio_d, &stGPIOInit);
        /*复用*/
        GPIO_PinAFConfig(fsmc_lcd->cfg.hw_cfg.dports[i].gpio_d, 
            drv_gpio_get_af_src(fsmc_lcd->cfg.hw_cfg.dports[i].gpio_dpin), GPIO_AF_FSMC);
    }

    /*****************************************
     *          控制线路初始化 - 片选
     *****************************************/
    /*片选*/
    drv_gpio_enable_clk(fsmc_lcd->cfg.hw_cfg.cs.gpio_c);
    /*初始化*/
    stGPIOInit.GPIO_Pin = fsmc_lcd->cfg.hw_cfg.cs.gpio_cpin;
    GPIO_Init(fsmc_lcd->cfg.hw_cfg.cs.gpio_c, &stGPIOInit);
    /*复用*/
    GPIO_PinAFConfig(fsmc_lcd->cfg.hw_cfg.cs.gpio_c,
        drv_gpio_get_af_src(fsmc_lcd->cfg.hw_cfg.cs.gpio_cpin), GPIO_AF_FSMC);

    /*****************************************
     *          控制线路初始化 - 写入使能
     *****************************************/
    drv_gpio_enable_clk(fsmc_lcd->cfg.hw_cfg.we.gpio_c);
    stGPIOInit.GPIO_Pin = fsmc_lcd->cfg.hw_cfg.we.gpio_cpin;
    GPIO_Init(fsmc_lcd->cfg.hw_cfg.we.gpio_c, &stGPIOInit);
    /*复用*/
    GPIO_PinAFConfig(fsmc_lcd->cfg.hw_cfg.we.gpio_c,
        drv_gpio_get_af_src(fsmc_lcd->cfg.hw_cfg.we.gpio_cpin), GPIO_AF_FSMC);
    
    /*****************************************
     *          控制线路初始化 - 输出使能
     *****************************************/
    drv_gpio_enable_clk(fsmc_lcd->cfg.hw_cfg.oe.gpio_c);
    stGPIOInit.GPIO_Pin = fsmc_lcd->cfg.hw_cfg.oe.gpio_cpin;
    GPIO_Init(fsmc_lcd->cfg.hw_cfg.oe.gpio_c, &stGPIOInit);
    /*复用*/
    GPIO_PinAFConfig(fsmc_lcd->cfg.hw_cfg.oe.gpio_c,
        drv_gpio_get_af_src(fsmc_lcd->cfg.hw_cfg.oe.gpio_cpin), GPIO_AF_FSMC);
    
    /*****************************************
     *          控制线路初始化 - 数据/命令线
     *****************************************/
    drv_gpio_enable_clk(fsmc_lcd->cfg.hw_cfg.dc.gpio_c);
    stGPIOInit.GPIO_Pin = fsmc_lcd->cfg.hw_cfg.dc.gpio_cpin;
    GPIO_Init(fsmc_lcd->cfg.hw_cfg.dc.gpio_c, &stGPIOInit);
    /*复用*/
    GPIO_PinAFConfig(fsmc_lcd->cfg.hw_cfg.dc.gpio_c,
        drv_gpio_get_af_src(fsmc_lcd->cfg.hw_cfg.dc.gpio_cpin), GPIO_AF_FSMC);


    /*****************************************
     *          控制线路初始化 - 输出控制初始化
     *****************************************/
    stGPIOInit.GPIO_OType = GPIO_OType_PP;
    stGPIOInit.GPIO_PuPd = GPIO_PuPd_UP;
    stGPIOInit.GPIO_Mode = GPIO_Mode_OUT;
    stGPIOInit.GPIO_Speed = GPIO_Speed_50MHz;
    /*****************************************
     *          控制线路初始化 - 复位线
     *****************************************/
    drv_gpio_enable_clk(fsmc_lcd->cfg.hw_cfg.rst.gpio_c);
    stGPIOInit.GPIO_Pin = fsmc_lcd->cfg.hw_cfg.rst.gpio_cpin;
    GPIO_Init(fsmc_lcd->cfg.hw_cfg.rst.gpio_c, &stGPIOInit);
    /*****************************************
     *          控制线路初始化 - 背光灯
     *****************************************/
    drv_gpio_enable_clk(fsmc_lcd->cfg.hw_cfg.bkl.gpio_c);
    stGPIOInit.GPIO_Pin = fsmc_lcd->cfg.hw_cfg.bkl.gpio_cpin;
    GPIO_Init(fsmc_lcd->cfg.hw_cfg.bkl.gpio_c, &stGPIOInit);


    /******************************************************************
     * 
     *                      FSMC基础配置
     * 
     ******************************************************************/
    /*使能时钟*/
    RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC, ENABLE);
    /*初始化: 使用什么控制器, 使用哪个区域, 数据宽度, 传输模式*/
    FSMC_NORSRAMInitTypeDef  stSramInit;
    /*建立时间*/
	FSMC_NORSRAMTimingInitTypeDef  stWRTimingInit;
	stWRTimingInit.FSMC_AddressSetupTime = 0x04;        //地址建立时间
    stWRTimingInit.FSMC_DataSetupTime = 0x0b;		    //数据保持时间
    stWRTimingInit.FSMC_AccessMode = FSMC_AccessMode_B;	//模式B,异步NOR FLASH模式, 与ILI9806G的8080时序匹配
    /*配置模式B, 以下参数无效*/
    stWRTimingInit.FSMC_AddressHoldTime = 0x00;
	stWRTimingInit.FSMC_BusTurnAroundDuration = 0x00;   //设置总线转换周期，仅用于复用模式的NOR操作
	stWRTimingInit.FSMC_CLKDivision = 0x00;	            //设置时钟分频，仅用于同步类型的存储器
	stWRTimingInit.FSMC_DataLatency = 0x00;		        //数据保持时间，仅用于同步型的NOR
	/*初始化*/
	stSramInit.FSMC_Bank = FSMC_Bank1_NORSRAM3;                         // 选择FSMC映射的存储区域： Bank1 sram4
	stSramInit.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;       //设置地址总线与数据总线是否复用，仅用于NOR
	stSramInit.FSMC_MemoryType = FSMC_MemoryType_NOR;                   //设置要控制的存储器类型：SRAM类型
	stSramInit.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;         //存储器数据宽度：16位
	stSramInit.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;      //设置是否使用突发访问模式，仅用于同步类型的存储器
	//stSramInit.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;     //设置是否使能等待信号，仅用于同步类型的存储器
	stSramInit.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;   //设置等待信号的有效极性，仅用于同步类型的存储器
	stSramInit.FSMC_WrapMode = FSMC_WrapMode_Disable;                   //设置是否支持把非对齐的突发操作，仅用于同步类型的存储器
	stSramInit.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;   //设置等待信号插入的时间，仅用于同步类型的存储器
	stSramInit.FSMC_WriteOperation = FSMC_WriteOperation_Enable;        //存储器写使能 
	stSramInit.FSMC_WaitSignal = FSMC_WaitSignal_Disable;  		        //不使用等待信号
	stSramInit.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;           // 不使用扩展模式，读写使用相同的时序
	stSramInit.FSMC_WriteBurst = FSMC_WriteBurst_Disable;               //突发写操作
	stSramInit.FSMC_ReadWriteTimingStruct = &stWRTimingInit;            //读写时序配置
	stSramInit.FSMC_WriteTimingStruct = &stWRTimingInit;                //读写同样时序，使用扩展模式时这个配置才有效

    FSMC_NORSRAMInit(&stSramInit);                  //初始化FSMC配置
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM3, ENABLE);   //使能BANK

    printf("=======================fsmc lcd init\r\n");
    
    return DY_EOK;
}

static void fsmc_lcd_ILI9806G_set_pixel_color(dy_device_t *dev, uint16_t color)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    fsmc_lcd->cfg.pg_cfg.cur_text_color = color;
}

static void fsmc_lcd_ILI9806G_set_background_color(dy_device_t *dev, uint16_t color)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    fsmc_lcd->cfg.pg_cfg.cur_background_color = color;
}

/*LCD控制*/
static int fsmc_lcd_control(dy_device_t *dev, int cmd, void *arg)
{
    fsmc_lcd_dev_t *fsmc_lcd = (fsmc_lcd_dev_t *)dev;
    switch (cmd)
    {
        case LCD_RST:
            GPIO_ResetBits(fsmc_lcd->cfg.hw_cfg.rst.gpio_c, fsmc_lcd->cfg.hw_cfg.rst.gpio_cpin);   //低电平
            drv_time_delay_ms(50);
            GPIO_SetBits(fsmc_lcd->cfg.hw_cfg.rst.gpio_c, fsmc_lcd->cfg.hw_cfg.rst.gpio_cpin);     //高电平
            drv_time_delay_ms(50);
            break;
        case LCD_REG_CONFIG:
            fsmc_lcd_ILI9806G_registers_config(dev);
            break;
        case LCD_GRAM_SCAN_CONFIG:
            fsmc_lcd_ILI9806G_GRAM_scan(dev, *((uint8_t*)arg));
            break;
        case LCD_CLEAR_SCREEN:
            fsmc_lcd_window_config_t *window = (fsmc_lcd_window_config_t*)arg;
            if (arg != NULL)
            {
                fsmc_lcd_ILI9806G_clear(dev, window->start_point.x, window->start_point.y, window->width, window->height);
            }
            else
            {
                fsmc_lcd_ILI9806G_clear(dev, 0, 0, fsmc_lcd->cfg.pg_cfg.x_pixel, fsmc_lcd->cfg.pg_cfg.y_pixel);
            }
            break;
        case LCD_SET_PIXEL_COLOR:
            fsmc_lcd_ILI9806G_set_pixel_color(dev, *((uint16_t*)arg));
            break;
        case LCD_SET_BACKGROUND_COLOR:
            fsmc_lcd_ILI9806G_set_background_color(dev, *((uint16_t*)arg));
            break;
        case LCD_DRAW_LINE:
            fsmc_lcd_line_t *line = (fsmc_lcd_line_t*)arg;
            /*设置颜色*/
            fsmc_lcd_ILI9806G_set_pixel_color(dev, line->color);
            /*画图*/
            fsmc_lcd_ILI9806G_draw_line(dev, line->point1.x, line->point1.y, line->point2.x, line->point2.y);
            break;
        case LCD_DRAW_RECTANGLE:
            fsmc_lcd_rectangle_t *rectangle = (fsmc_lcd_rectangle_t*)arg;
            /*设置颜色*/
            fsmc_lcd_ILI9806G_set_pixel_color(dev, rectangle->color);
            /*画矩形*/
            fsmc_lcd_ILI9806G_draw_rectangle(dev,
                rectangle->start_point.x, rectangle->start_point.y,
                rectangle->width, rectangle->height, rectangle->filled);
            break;
        case LCD_DRAW_CIRCLE:
            fsmc_lcd_circle_t *circle = (fsmc_lcd_circle_t*)arg;
            /*设置颜色*/
            fsmc_lcd_ILI9806G_set_pixel_color(dev, circle->color);
            /*画圆*/
            fsmc_lcd_ILI9806G_draw_circle_v2(dev,
                circle->center.x, circle->center.y,
                circle->radius, circle->filled);
            break;
        case LCD_DISPLAY_EN_CHAR:
            fsmc_lcd_string_t *charactor = (fsmc_lcd_string_t*)arg;
            /*设置字符颜色*/
            fsmc_lcd_ILI9806G_set_pixel_color(dev, charactor->color);
            /*显示字符*/
            fsmc_lcd_ILI9806G_display_ascii_char(dev, charactor->position.x, charactor->position.y, charactor->str[0]);
            break;
        case LCD_DISPLAY_EN_STRING:
            fsmc_lcd_string_t *str = (fsmc_lcd_string_t*)arg;
            /*设置字符颜色*/
            fsmc_lcd_ILI9806G_set_pixel_color(dev, str->color);
            /*显示字符*/
            fsmc_lcd_ILI9806G_display_ascii_string(dev, str->position.x, str->position.y, str->str);
            break;
        case LCD_OPEN_BK_LIGHT:
            GPIO_SetBits(fsmc_lcd->cfg.hw_cfg.bkl.gpio_c, fsmc_lcd->cfg.hw_cfg.bkl.gpio_cpin);
            break;
        case LCD_CLOSE_BK_LIGHT:
            GPIO_ResetBits(fsmc_lcd->cfg.hw_cfg.bkl.gpio_c, fsmc_lcd->cfg.hw_cfg.bkl.gpio_cpin);
            break;
    }
    return DY_EOK;
}

device_ops_t g_fsmc_lcd_opt = {
    .init = fsmc_lcd_init,
    .control = fsmc_lcd_control
};

static int fsmc_lcd_port_init()
{
    /* 数据端口初始化 (16位数据线) */
    g_lcd_dev.cfg.hw_cfg.dports[0].gpio_d = GPIOD;
    g_lcd_dev.cfg.hw_cfg.dports[0].gpio_dpin = GPIO_Pin_14;  // FSMC_D0
    
    g_lcd_dev.cfg.hw_cfg.dports[1].gpio_d = GPIOD;
    g_lcd_dev.cfg.hw_cfg.dports[1].gpio_dpin = GPIO_Pin_15;  // FSMC_D1
    
    g_lcd_dev.cfg.hw_cfg.dports[2].gpio_d = GPIOD;
    g_lcd_dev.cfg.hw_cfg.dports[2].gpio_dpin = GPIO_Pin_0;   // FSMC_D2
    
    g_lcd_dev.cfg.hw_cfg.dports[3].gpio_d = GPIOD;
    g_lcd_dev.cfg.hw_cfg.dports[3].gpio_dpin = GPIO_Pin_1;   // FSMC_D3
    
    g_lcd_dev.cfg.hw_cfg.dports[4].gpio_d = GPIOE;
    g_lcd_dev.cfg.hw_cfg.dports[4].gpio_dpin = GPIO_Pin_7;   // FSMC_D4
    
    g_lcd_dev.cfg.hw_cfg.dports[5].gpio_d = GPIOE;
    g_lcd_dev.cfg.hw_cfg.dports[5].gpio_dpin = GPIO_Pin_8;   // FSMC_D5
    
    g_lcd_dev.cfg.hw_cfg.dports[6].gpio_d = GPIOE;
    g_lcd_dev.cfg.hw_cfg.dports[6].gpio_dpin = GPIO_Pin_9;   // FSMC_D6
    
    g_lcd_dev.cfg.hw_cfg.dports[7].gpio_d = GPIOE;
    g_lcd_dev.cfg.hw_cfg.dports[7].gpio_dpin = GPIO_Pin_10;  // FSMC_D7
    
    g_lcd_dev.cfg.hw_cfg.dports[8].gpio_d = GPIOE;
    g_lcd_dev.cfg.hw_cfg.dports[8].gpio_dpin = GPIO_Pin_11;  // FSMC_D8
    
    g_lcd_dev.cfg.hw_cfg.dports[9].gpio_d = GPIOE;
    g_lcd_dev.cfg.hw_cfg.dports[9].gpio_dpin = GPIO_Pin_12;  // FSMC_D9
    
    g_lcd_dev.cfg.hw_cfg.dports[10].gpio_d = GPIOE;
    g_lcd_dev.cfg.hw_cfg.dports[10].gpio_dpin = GPIO_Pin_13; // FSMC_D10
    
    g_lcd_dev.cfg.hw_cfg.dports[11].gpio_d = GPIOE;
    g_lcd_dev.cfg.hw_cfg.dports[11].gpio_dpin = GPIO_Pin_14; // FSMC_D11
    
    g_lcd_dev.cfg.hw_cfg.dports[12].gpio_d = GPIOE;
    g_lcd_dev.cfg.hw_cfg.dports[12].gpio_dpin = GPIO_Pin_15; // FSMC_D12
    
    g_lcd_dev.cfg.hw_cfg.dports[13].gpio_d = GPIOD;
    g_lcd_dev.cfg.hw_cfg.dports[13].gpio_dpin = GPIO_Pin_8;  // FSMC_D13
    
    g_lcd_dev.cfg.hw_cfg.dports[14].gpio_d = GPIOD;
    g_lcd_dev.cfg.hw_cfg.dports[14].gpio_dpin = GPIO_Pin_9;  // FSMC_D14
    
    g_lcd_dev.cfg.hw_cfg.dports[15].gpio_d = GPIOD;
    g_lcd_dev.cfg.hw_cfg.dports[15].gpio_dpin = GPIO_Pin_10; // FSMC_D15


    /*控制线*/
    g_lcd_dev.cfg.hw_cfg.cs.gpio_c = GPIOG;
    g_lcd_dev.cfg.hw_cfg.cs.gpio_cpin = GPIO_Pin_10;        // 片选

    g_lcd_dev.cfg.hw_cfg.oe.gpio_c = GPIOD;
    g_lcd_dev.cfg.hw_cfg.oe.gpio_cpin = GPIO_Pin_4;         // 读使能

    g_lcd_dev.cfg.hw_cfg.we.gpio_c = GPIOD;
    g_lcd_dev.cfg.hw_cfg.we.gpio_cpin = GPIO_Pin_5;         // 写使能

    g_lcd_dev.cfg.hw_cfg.dc.gpio_c = GPIOF;
    g_lcd_dev.cfg.hw_cfg.dc.gpio_cpin = GPIO_Pin_0;         // 数据/命令

    g_lcd_dev.cfg.hw_cfg.rst.gpio_c = GPIOF;
    g_lcd_dev.cfg.hw_cfg.rst.gpio_cpin = GPIO_Pin_11;       // 复位

    g_lcd_dev.cfg.hw_cfg.bkl.gpio_c = GPIOF;
    g_lcd_dev.cfg.hw_cfg.bkl.gpio_cpin = GPIO_Pin_9;        // 背光

    return DY_EOK;
}

static int fsmc_lcd_pg_init()
{

    g_lcd_dev.cfg.pg_cfg.width = 480;
    g_lcd_dev.cfg.pg_cfg.height = 800;
    g_lcd_dev.cfg.pg_cfg.origin_x = 0;
    g_lcd_dev.cfg.pg_cfg.origin_y = 0;
    g_lcd_dev.cfg.pg_cfg.cmd_addr = 0x68000000;
    g_lcd_dev.cfg.pg_cfg.data_addr = 0x68000002;

    //g_lcd_dev.cfg.pg_cfg.cur_background_color = 0xFFFF;     //白色
    g_lcd_dev.cfg.pg_cfg.cur_background_color = 0x0000;   //红色
    g_lcd_dev.cfg.pg_cfg.cur_text_color = 0xF800;   //红色
    //g_lcd_dev.cfg.pg_cfg.cur_background_color = 0x07E0;   //绿色

    g_lcd_dev.cfg.pg_cfg.cur_fonts = &ascii_font_16x32;

    return DY_EOK;
}

int drv_fsmc_lcd_init()
{
    int ret = DY_EOK;

    /*device初始化*/
    strncpy(g_lcd_dev.device.name, "lcd", sizeof(g_lcd_dev.device.name) - 1);
    g_lcd_dev.device.ops = &g_fsmc_lcd_opt;

    ret = fsmc_lcd_port_init();
    ret = fsmc_lcd_pg_init();

    /*注册*/
    if (DY_EOK != dy_device_register(g_lcd_dev.device.name, &g_lcd_dev.device))
    {
        return DY_ERROR;
    }  

    /*调用初始化接口*/
    ret = g_lcd_dev.device.ops->init(&g_lcd_dev.device);

    return DY_EOK;
}


