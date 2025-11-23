

#if 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kservice.h"


/**********************************************
 * 
 * 设备管理层代码框架
 * 
 * 功能包括：
 * 1. 设备创建：可以创建一个逻辑设备
 * 2. 设备销毁：回收分配给逻辑设备的资源
 * 3. 设备注册：把逻辑设备和设备名称一起注册到设备管理器中
 * 4. 设备注销：从设备管理器中删除设备
 * 5. 设备查询：根据设备名称查询设备
 * 6. 设备初始化：设备硬件初始化
 * 7. 设备打开：给设备分配内存资源
 * 8. 设备关闭：回收给设备分配的内存资源
 * 9. 设备读写
 * 10. 设备控制
 * 11. 设备收发回调
 * 
 ***********************************************/

// 设备管理器结构体
typedef struct device_manager 
{
    dy_device_t *device_list;               // 设备链表
    int device_count;                       // 设备数量
} device_manager_t;

// 全局设备管理器实例
static device_manager_t g_device_mgr = {0};

/**
 * @brief 查找设备
 * @param name 设备名称
 * @return 设备指针，失败返回NULL
 */
dy_device_t *dy_find_device(const char *name)
{
    if (NULL == name)
    {
        return NULL;
    }
    dy_device_t *temp = g_device_mgr.device_list;
    while (temp)
    {
        if (strcmp(temp->name, name) == 0) 
        {
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

/**
 * @brief 创建设备
 * @param attach_size 私有数据大小
 * @return 设备指针，失败返回NULL
 */
dy_device_t *dy_device_create(int attach_size)
{
    int size;
    dy_device_t *device = NULL;
    size = DY_ALIGN(sizeof(dy_device_t), DY_ALIGN_SIZE);
    attach_size = DY_ALIGN(attach_size, DY_ALIGN_SIZE);

    size += attach_size;
    device = (dy_device_t *)malloc(size);
    if (NULL == device)
    {
        return NULL;
    }
    memset(device, 0, sizeof(dy_device_t));
    return device;
}

/**
 * @brief 销毁设备
 * @param dev 设备指针
 * @return 成功返回
 */
int dy_device_destroy(dy_device_t *dev)
{
    if (NULL == dev)
    {
        return DY_EINVAL;
    }
    free(dev);
    return DY_EOK;
}


/**
 * @brief 注册设备
 * @param name 设备名称
 * @param dev 设备指针
 * @return 设备指针，失败返回NULL
 */
int dy_device_register(const char *name, dy_device_t *dev)
{
    if (NULL == name || NULL == dev)
    {
        return DY_EINVAL;
    }
    if (NULL != dy_find_device(name))
    {
        return DY_ERROR;
    }

    dev->next = g_device_mgr.device_list;
    g_device_mgr.device_list = dev;
    g_device_mgr.device_count++;
    return DY_EOK;
}

/**
 * @brief 注销设备
 * @param name 设备名称
 * @return DY_EOK, 成功; 
 */
int dy_device_unregister(const char *name)
{
    if (NULL == name)
    {
        return DY_EINVAL;
    }
    /*通过获取指针地址的方式, 始终拿着前一个节点的指针的地址, 从控制了前一个节点*/
    dy_device_t **prev = &g_device_mgr.device_list;
    dy_device_t *current = g_device_mgr.device_list;
    while (current)
    {
        if (0 == strcmp(current->name, name))
        {
            /*把current从链表中移除*/
            *prev = current->next; // prevNode->next = contenxt->next
            free(current);         // 释放内存
            g_device_mgr.device_count--;
            return DY_EOK;
        }
        /*将指针指向当前节点的next指针*/
        prev = &current->next;
        /*当前指向下一个指针*/
        current = current->next;
    } 
    return DY_ENOOBJ;
}


/**
 * @brief 初始化设备: 硬件初始化
 * @param dev 设备指针
 * @return 0成功，-1失败
 */
int dy_device_init(dy_device_t *dev)
{
    if (NULL == dev || NULL == dev->ops || NULL == dev->ops->init) 
    {
        return DY_EINVAL;
    }
    
    return dev->ops->init(dev);
}

/**
 * @brief 打开设备: 给设备分配内存资源
 * @param dev 设备指针
 * @return 0成功，-1失败
 */
int dy_device_open(dy_device_t *dev)
{
    if (NULL == dev || NULL == dev->ops || NULL == dev->ops->open) 
    {
        return DY_EINVAL;
    }
    
    return dev->ops->open(dev);
}

/**
 * @brief 关闭设备: 回收设备的内存资源
 * @param dev 设备指针
 * @return 0成功，-1失败
 */
int dy_device_close(dy_device_t *dev)
{
    if (NULL == dev || NULL == dev->ops || NULL == dev->ops->close) 
    {
        return DY_EINVAL;
    }
    
    return dev->ops->close(dev);
}

/**
 * @brief 读取设备数据
 * @param dev 设备指针
 * @param buf 缓冲区
 * @param len 读取长度
 * @return 实际读取长度，-1失败
 */
int dy_device_read(dy_device_t *dev, void *buf, int len)
{
    if (NULL == dev || NULL == dev->ops || NULL == dev->ops->read) 
    {
        return DY_EINVAL;
    }
    
    return dev->ops->read(dev, buf, len);
}

/**
 * @brief 写入设备数据
 * @param dev 设备指针
 * @param buf 缓冲区
 * @param len 写入长度
 * @return 实际写入长度，-1失败
 */
int dy_device_write(dy_device_t *dev, void *buf, int len)
{
    if (NULL == dev || NULL == dev->ops || NULL == dev->ops->write) 
    {
        return DY_EINVAL;
    }
    
    return dev->ops->write(dev, buf, len);
}


/**
 * @brief 控制设备
 * @param dev 设备指针
 * @param cmd 控制命令
 * @param arg 参数
 * @return 0成功，-1失败
 */
int dy_device_control(dy_device_t *dev, int cmd, void *arg)
{
    if (NULL == dev || NULL == dev->ops || NULL == dev->ops->control) 
    {
        return DY_EINVAL;
    }
    
    return dev->ops->control(dev, cmd, arg);
}

/**
 * @brief 设备回调函数
 * @param dev 设备指针
 * @param event 事件类型
 * @param data 事件数据
 */
void dy_device_callback(dy_device_t *dev, int event, void *data)
{
    if (NULL == dev || NULL == dev->ops || NULL == dev->ops->callback) 
    {
        return;
    }
    
    dev->ops->callback(dev, event, data);
}

/**
 * @brief 获取设备数量
 * @return 设备数量
 */
int dy_device_get_count(void)
{
    return g_device_mgr.device_count;
}

/**
 * @brief 遍历所有设备
 * @param callback 回调函数
 * @param arg 用户参数
 */
void dy_device_foreach(void (*callback)(dy_device_t *dev, void *arg), void *arg)
{
    dy_device_t *temp = g_device_mgr.device_list;
    
    while (temp) 
    {
        callback(temp, arg);
        temp = temp->next;
    }
}


/**
 * @brief 初始化设备管理器
 * @return 0成功，-1失败
 */
int dy_device_manager_init(void)
{
    memset(&g_device_mgr, 0, sizeof(g_device_mgr));
    return DY_EOK;
}


#endif





