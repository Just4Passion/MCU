

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kservice.h"

/********************************************************************************************
 * 
 *                                      总线管理架构
 * 
 * 功能包括：
 * 1. 设备创建：可以创建一个逻辑设备
 * 
 *********************************************************************************************/
// 总线管理器结构体
typedef struct bus_manager 
{
    dy_bus_t *bus_list;               // 总线链表
    int bus_count;                    // 总线数量
} bus_manager_t;

// 全局总线管理器实例
static bus_manager_t g_bus_mgr = {0};

/**
 * @brief 注册总线
 * @param bus 总线指针
 * @return DY_EOK成功，失败返回错误码
 */
int dy_bus_register(dy_bus_t *bus)
{
    if (NULL == bus)
    {
        return DY_EINVAL;
    }
    
    // 检查是否已存在同名总线
    dy_bus_t *temp = g_bus_mgr.bus_list;
    while (temp)
    {
        if (strcmp(temp->name, bus->name) == 0) 
        {
            return DY_EEXIST;
        }
        temp = temp->next;
    }
    
    // 添加到链表头部
    bus->next = g_bus_mgr.bus_list;
    g_bus_mgr.bus_list = bus;
    g_bus_mgr.bus_count++;
    
    return DY_EOK;
}

/**
 * @brief 查找总线
 * @param name 总线名称
 * @return 总线指针，失败返回NULL
 */
dy_bus_t *dy_bus_find(const char *name)
{
    if (NULL == name)
    {
        return NULL;
    }
    
    dy_bus_t *temp = g_bus_mgr.bus_list;
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
 * @brief 挂载设备到总线. 即把设备添加到总线的设备链表中
 * @param bus 总线指针
 * @param dev 设备指针
 * @return DY_EOK成功，失败返回错误码
 */
int dy_bus_attach_device(dy_bus_t *bus, dy_device_t *dev)
{
    if (NULL == bus || NULL == dev) 
    {
        return DY_EINVAL;
    }
    
    // 检查设备是否已挂载 - 我需要使用名字, 而不是指针
    dy_device_t *temp = bus->device_list;
    while (temp != NULL) 
    {
        if (0 == strcmp(temp->name, dev->name))
        {
            return DY_EEXIST;
        }
        temp = temp->next;
    }
    
    dev->bus = bus;
    //dev->next = NULL;//bus->device_list;
    //bus->device_list = dev;
    //bus->device_count++;
    return DY_EOK;
}

/**
 * @brief 从总线卸载设备
 * @param bus 总线指针
 * @param dev 设备指针
 * @return DY_EOK成功，失败返回错误码
 */
int dy_bus_detach_device(dy_bus_t *bus, dy_device_t *dev)
{
    if (NULL == bus || NULL == dev)
    {
        return DY_EINVAL;
    }

    dy_device_t **prev = &bus->device_list;
    dy_device_t *current = bus->device_list;
    
    while (current) 
    {
        if (0 == strcmp(current->name, dev->name))
        {
            *prev = current->next;
            bus->device_count--;
            return DY_EOK;
        }
        prev = &current->next;
        current = current->next;
    }
    
    return DY_ENOOBJ;
}

/**
 * @brief 设置总线状态
 * @param bus 总线指针
 * @return 总线状态
 */
static dy_bus_state_t dy_bus_set_state(dy_bus_t *bus, dy_bus_state_t state)
{
    if (NULL == bus)
    {
        return DY_BUS_STATE_ERROR;
    }
    
    bus->state = state;
    
    return state;
}

/**
 * @brief 获取总线状态
 * @param bus 总线指针
 * @return 总线状态
 */
static dy_bus_state_t dy_bus_get_state(dy_bus_t *bus)
{
    if (NULL == bus) 
    {
        return DY_BUS_STATE_ERROR;
    }
    
    dy_bus_state_t state = bus->state;
    
    return state;
}



/**
 * @brief 获取总线挂载的设备数量
 * @param bus 总线指针
 * @return 设备数量，失败返回-1
 */
static int dy_bus_get_device_count(dy_bus_t *bus)
{
    if (NULL == bus)
    {
        return -1;
    }
    
    int count = bus->device_count;
    
    return count;
}

/**
 * @brief 控制总线
 * @param bus 总线指针
 * @param cmd 控制命令
 * @param arg 参数
 * @return DY_EOK成功，失败返回错误码
 */
int dy_bus_control(dy_bus_t *bus, int cmd, void *arg)
{
    if (NULL == bus || NULL == bus->ops || NULL == bus->ops->control) 
    {
        return DY_EINVAL;
    }
    
    int ret = DY_EOK;
    switch(cmd)
    {
        case DY_BUS_CTRL_CMD_SET_NAME:
            break;
        case DY_BUS_CTRL_CMD_GET_NAME:
            strcpy(arg, bus->name);
            break;
        case DY_BUS_CTRL_CMD_SET_STATE:
            dy_bus_set_state(bus, (*(dy_bus_state_t *)arg));
            break;
        case DY_BUS_CTRL_CMD_GET_STATE:
            *(dy_bus_state_t *)arg = dy_bus_get_state(bus);
            break;
        default:
            break;
    }
    
    return ret;
}



/**
 * @brief 遍历总线挂载的所有设备
 * @param callback 回调函数
 * @param arg 用户参数
 */
void dy_bus_device_foreach(dy_bus_t *bus, void (*callback)(dy_device_t *bus, void *arg), void *arg)
{
    dy_device_t *temp = bus->device_list;
    while (temp)
    {
        callback(temp, arg);
        temp = temp->next;
    }
}

/**
 * @brief 获取总线管理器中的总线数量
 * @return 总线数量
 */
int dy_bus_get_count(void)
{
    int count = g_bus_mgr.bus_count;
    
    return count;
}

/**
 * @brief 遍历所有总线
 * @param callback 回调函数
 * @param arg 用户参数
 */
void dy_bus_foreach(void (*callback)(dy_bus_t *bus, void *arg), void *arg)
{
    dy_bus_t *temp = g_bus_mgr.bus_list;
    while (temp)
    {
        callback(temp, arg);
        temp = temp->next;
    }
}

/**
 * @brief 总线管理器初始化
 * @return OK
 */
int dy_bus_manager_init(void)
{
    g_bus_mgr.bus_list = NULL;
    g_bus_mgr.bus_count = 0;
    return DY_EOK;
}



