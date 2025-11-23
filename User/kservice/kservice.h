
#ifndef __KSERVICE_H__
#define __KSERVICE_H__

#define DY_ALIGN_SIZE 4
#define DY_ALIGN(size, align)           (((size) + (align) - 1) & ~((align) - 1))

#define DY_DEVICE_FLAG_STREAM           0x040           // 流模式
#define DY_DEVICE_FLAG_INT_RX           0x100           // 中断接收
#define DY_DEVICE_FLAG_DMA_RX           0x200           // DMA接收
#define DY_DEVICE_FLAG_INT_TX           0x400           // 中断发送
#define DY_DEVICE_FLAG_DMA_TX           0x800           // DMA发送

#define DY_EBUSNOINIT                  (-8)             // 总线尚未初始化
#define DY_EEXIST                      (-7)             // 对象已存在
#define DY_EBUSY                       (-6)             // 忙, 资源不可操作
#define DY_ETMOUT                      (-5)             // 超时
#define DY_ENOOBJ                      (-4)             // 对象不存在
#define DY_EINVAL                      (-3)             // 无效参数
#define DY_ENOMEM                      (-2)             // 内存不足
#define DY_ERROR                       (-1)             // 通用或未知错误
#define DY_EOK                          0               // ok  

struct device;
typedef struct device dy_device_t;

struct bus;
typedef struct bus dy_bus_t;


/**********************************************
 * 
 * 
 *              总线相关定义和操作
 * 
 * 
************************************************/
// 总线类型定义
typedef enum 
{
    DY_BUS_TYPE_I2C = 0,              // I2C总线
    DY_BUS_TYPE_SPI,                  // SPI总线
    DY_BUS_TYPE_UART,                 // UART总线
    DY_BUS_TYPE_CAN,                  // CAN总线
    DY_BUS_TYPE_USB,                  // USB总线
    DY_BUS_TYPE_CUSTOM                // 自定义总线
}dy_bus_type_t;

typedef enum 
{
    DY_BUS_STATE_UNINITIALIZED = 0,   // 未初始化
    DY_BUS_STATE_INITIALIZED,         // 已初始化
    DY_BUS_STATE_OPENED,              // 已打开: 说明有设备正在使用总线, 说明资源在被占用
    DY_BUS_STATE_CLOSED,              // 已关闭: 说明没有设备使用总线
    DY_BUS_STATE_ERROR,               // 错误状态
    DY_BUS_STATE_BUSY                 // 忙碌状态
}dy_bus_state_t;

typedef enum
{
    DY_BUS_CTRL_CMD_NONE = 0,
    DY_BUS_CTRL_CMD_SET_NAME,
    DY_BUS_CTRL_CMD_GET_NAME,
    DY_BUS_CTRL_CMD_SET_STATE,
    DY_BUS_CTRL_CMD_GET_STATE,
    DY_BUS_CTRL_CMD_GET_DEVICE_COUNT,
    DY_BUS_CTRL_CMD_MAX
}dy_bus_ctl_cmd_t;

typedef struct 
{
    int (*init)(dy_bus_t *bus);              		 	// 总线初始化
    int (*control)(dy_bus_t *bus, int cmd, void *arg); 	// 总线控制, 重启总线
}dy_bus_ops_t;


typedef struct bus
{
	char name[32+1];
    dy_bus_type_t type;             // 总线类型
    dy_bus_state_t state;           // 总线状态
	dy_bus_ops_t *ops;				// 总线操作
	int device_count;				// 挂载的设备总数
	dy_device_t *device_list;		// 挂载的设备
    struct bus *next;               // 下一个总线节点
	void *priv_data;				// 私有数据里面可以放配置, 可以放驱动
}dy_bus_t;


/**
 * @brief 注册总线
 * @param bus 总线指针
 * @return DY_EOK成功，失败返回错误码
 */
int dy_bus_register(dy_bus_t *bus);

/**
 * @brief 查找总线
 * @param name 总线名称
 * @return 总线指针，失败返回NULL
 */
dy_bus_t *dy_bus_find(const char *name);


/**
 * @brief 挂载设备到总线. 即把设备添加到总线的设备链表中
 * @param bus 总线指针
 * @param dev 设备指针
 * @return DY_EOK成功，失败返回错误码
 */
int dy_bus_attach_device(dy_bus_t *bus, dy_device_t *dev);

/**
 * @brief 从总线卸载设备
 * @param bus 总线指针
 * @param dev 设备指针
 * @return DY_EOK成功，失败返回错误码
 */
int dy_bus_detach_device(dy_bus_t *bus, dy_device_t *dev);


/**
 * @brief 控制总线
 * @param bus 总线指针
 * @param cmd 控制命令
 * @param arg 参数
 * @return DY_EOK成功，失败返回错误码
 */
int dy_bus_control(dy_bus_t *bus, int cmd, void *arg);


/**
 * @brief 遍历总线挂载的所有设备
 * @param callback 回调函数
 * @param arg 用户参数
 */
void dy_bus_device_foreach(dy_bus_t *bus, void (*callback)(dy_device_t *bus, void *arg), void *arg);

/**
 * @brief 获取总线管理器中的总线数量
 * @return 总线数量
 */
int dy_bus_get_count(void);

/**
 * @brief 遍历所有总线
 * @param callback 回调函数
 * @param arg 用户参数
 */
void dy_bus_foreach(void (*callback)(dy_bus_t *bus, void *arg), void *arg);


/**
 * @brief 总线管理器初始化
 * @return OK
 */
int dy_bus_manager_init(void);


/**********************************************
 * 
 * 
 *              设备相关定义和操作
 * 
 * 
************************************************/
// 设备操作接口定义
typedef struct device_ops {
    int (*init)(void *dev);              // 设备初始化
    int (*open)(void *dev);              // 设备打开
    int (*close)(void *dev);             // 设备关闭
    int (*read)(void *dev, void *buf, int len);  // 设备读操作
    int (*write)(void *dev, void *buf, int len); // 设备写操作
    int (*control)(void *dev, int cmd, void *arg); // 设备控制
    void (*callback)(void *dev, int event, void *data); // 设备回调函数
}device_ops_t;


// 设备结构体定义
typedef struct device 
{
    char name[32 + 1];                   // 设备名称
    int open_flag;                       // 设备打开标志, 轮询, 中断, DMA等
    device_ops_t *ops;                   // 设备操作接口
    dy_bus_t *bus;                       // 设备所属总线
    struct device *next;                 // 链表指针

    /****************************
     * 私有数据, 放在最后, 
     * 申请大于sizeof(struct device)的空间
     * 末尾作为私有数据存储
     * 使用的时候, 直接取地址
     ****************************/
    void *priv;                          // 设备私有数据 - 这样可以直接覆盖
}dy_device_t;


/**
 * @brief 查找设备
 * @param name 设备名称
 * @return 设备指针，失败返回NULL
 */
dy_device_t *dy_find_device(const char *name);

/**
 * @brief 创建设备
 * @param attach_size 私有数据大小
 * @return 设备指针，失败返回NULL
 */
dy_device_t *dy_device_create(int attach_size);

/**
 * @brief 销毁设备
 * @param dev 设备指针
 * @return 成功返回
 */
int dy_device_destroy(dy_device_t *dev);


/**
 * @brief 注册设备
 * @param name 设备名称
 * @param dev 设备指针
 * @return 设备指针，失败返回NULL
 */
int dy_device_register(const char *name, dy_device_t *dev);

/**
 * @brief 注销设备
 * @param name 设备名称
 * @return DY_EOK, 成功; 
 */
int dy_device_unregister(const char *name);

/**
 * @brief 获取设备数量
 * @return 设备数量
 */
int dy_device_get_count(void);

/**
 * @brief 遍历所有设备
 * @param callback 回调函数
 * @param arg 用户参数
 */
void dy_device_foreach(void (*callback)(dy_device_t *dev, void *arg), void *arg);


/**
 * @brief 初始化设备管理器
 * @return 0成功，-1失败
 */
int dy_device_manager_init(void);

#endif


