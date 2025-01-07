// Copyright 2020-2021 The jdh99 Authors. All rights reserved.
// 键值数据库
// Authors: jdh99 <jdh821@163.com>
// 记录(record)格式:
//   oid|key|value
//   oid:object id.每条记录的唯一id
//   key:键值,必须唯一,必须是字符串
//   value:可以是任意格式字节流
// 记录有过期属性的属性,会自动清除过期节点.过期时间为0表示不会过期
// 持久化存储会同时存主备,只会存储不会过期的节点
// 如果要使用持久化存储的功能,必须保证不能使用超过一半的内存,
// 因为持久化存储现有机制会先将参数全部紧凑的存储在一块连续的内存中

#ifndef PLATSA_H
#define PLATSA_H

#include <stdint.h>
#include <stdbool.h>

// 最长key字节数
#define PLATSA_KEY_LEN_MAX 512

// 清除过期节点的间隔.单位:s
#define PLATSA_INTERVAL_CLEAR_EXPIRATION_NODE 10

// 无效地址
#define PLATSA_INVALID_ADDR 0xffffffff

// 永不过期.但不可以持久化存储
#define PLATSA_NEVER_EXPIRE -1

// PlatsaLoad 模块载入
// mid是tzmalloc的内存id
bool PlatsaLoad(int mid);

// PlatsaGetOid 获取OID
// 返回0表示失败
intptr_t PlatsaGetOid(char* key);

// PlatsaSet 设置键对应的值.如果键不存在,会新建键
// 返回的是oid,如果为0表示设置失败
intptr_t PlatsaSet(char* key, uint8_t* bytes, int size);

// PlatsaSetByOid 设置oid对应的值
bool PlatsaSetByOid(intptr_t oid, uint8_t* bytes, int size);

// PlatsaGet 获取值
// 返回如果为NULL,表示获取失败.值的长度保存在valueLen中
// 如果不需要保存值的长度,valueLen可传入NULL
// 注意:修改返回的值是不安全的,可能会引发内存泄漏
uint8_t* PlatsaGet(char* key, int* valueLen);

// PlatsaGetByOid 根据oid获取值
// 返回如果为NULL,表示获取失败.值的长度保存在valueLen中
// 如果不需要保存值的长度,valueLen可传入NULL
// 注意:修改返回的值是不安全的,可能会引发内存泄漏
uint8_t* PlatsaGetByOid(intptr_t oid, int* valueLen);

// PlatsaDelete 删除key对应的记录
void PlatsaDelete(char* key);

// PlatsaDeleteByOid 删除key对应的记录
void PlatsaDeleteByOid(intptr_t oid);

// PlatsaIsExistKey 判断key是否存在
bool PlatsaIsExistKey(char* key);

// PlatsaRenameKey 重命名key
bool PlatsaRenameKey(char* oldKey, char* newKey);

// PlatsaRenameKey 重命名key
bool PlatsaRenameKeyByOid(intptr_t oid, char* newKey);

// PlatsaSetExipirationTime 设置过期时间
// expirationTime:单位:ms.当前时刻之后经过expirationTime毫秒过期
// 如果过期时间设置为0,表示不会过期,且可以持久化存储
// 如果过期时间设置为PLATSA_NEVER_EXPIRE,表示不会过期,但不可以持久化存储
void PlatsaSetExipirationTime(char* key, int expirationTime);

// PlatsaSetExipirationTimeByOid 设置过期时间
// expirationTime:单位:ms.当前时刻之后经过expirationTime毫秒过期
// 如果过期时间设置为0,表示不会过期,且可以持久化存储
// 如果过期时间设置为PLATSA_NEVER_EXPIRE,表示不会过期,但不可以持久化存储
void PlatsaSetExipirationTimeByOid(intptr_t oid, int expirationTime);

// PlatsaGetUsedSize 获取已经使用的内存字节数
// 0表示获取失败
int PlatsaGetUsedSize(void);

// PlatsaSave 持久化数据到磁盘
// primaryAddr是主要存储区,地址必须不为PLATSA_INVALID_ADDR
// standbyAddr是备份存储区,为PLATSA_INVALID_ADDR表示无备份存储区
// size是存储区字节数
// 成功返回true,失败返回false
bool PlatsaSave(uint32_t primaryAddr, uint32_t standbyAddr, int size);

// PlatsaRecovery 从磁盘恢复数据
// primaryAddr是主要存储区,地址必须不为PLATSA_INVALID_ADDR
// standbyAddr是备份存储区,为PLATSA_INVALID_ADDR表示无备份存储区
// size是存储区字节数
// 成功返回true,失败返回false
bool PlatsaRecovery(uint32_t primaryAddr, uint32_t standbyAddr, int size);

// PlatsaSetAlign 设置对齐字节数
void PlatsaSetAlign(int align);

#endif
