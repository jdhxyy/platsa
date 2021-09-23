<!-- TOC -->

- [PLATSA操作手册](#platsa操作手册)
    - [文件历史](#文件历史)
    - [概述](#概述)
    - [记录格式](#记录格式)
    - [特性](#特性)
    - [API](#api)
    - [过期机制](#过期机制)
    - [初始化](#初始化)
    - [示例1：增加，读取，重命名key，删除操作](#示例1增加读取重命名key删除操作)
    - [示例2：值为结构体](#示例2值为结构体)
    - [示例3：持久化存储和恢复](#示例3持久化存储和恢复)

<!-- /TOC -->

# PLATSA操作手册

## 文件历史
日期|作者|描述
-|-|-
2020-06-09|周鑫|1.新建

## 概述
PLATSA取名来自于家具"普拉萨"，是宜家的一个储物系列。

PLATSA是C语言的键值存储数据库，功能上与redis类似。

## 记录格式
PLATSA的每条记录有3个属性：

oid|key|value
-|-|-

- oid:object id.每条记录的唯一id
- key:键值,必须唯一,必须是字符串
- value:可以是任意格式字节流

一条典型的记录：

oid|key|value
-|-|-
12321|ip|192.168.1.110

## 特性
- 记录默认为不过期，设置过期时间，则到过期时间会自动删除记录
- 可以持久化存储，将所有记录存储在磁盘上。注意只有不会过期的记录才会存储
- 可以从磁盘恢复记录
- 存储机制有主备两块区域，如果主存储区恢复失败，自动从备用存储区恢复

## API
```c
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
void PlatsaLoad(int mid);

// PlatsaRun 模块运行
void PlatsaRun(void);

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
// 注意:返回的值是只读的,不允许修改
const uint8_t* PlatsaGet(char* key, int* valueLen);

// PlatsaGetByOid 根据oid获取值
// 返回如果为NULL,表示获取失败.值的长度保存在valueLen中
// 如果不需要保存值的长度,valueLen可传入NULL
// 注意:返回的值是只读的,不允许修改
const uint8_t* PlatsaGetByOid(intptr_t oid, int* valueLen);

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
```

大部分的API接口都有key和oid两种，优先选择oid的方式，效率最高为O(1)。如果用key的方式，则会遍历数据库，效率为O(N)。

## 过期机制
可以通过以下接口设置过期时间：
```c
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
```

记录分为3种类型：
- 参数expirationTime为0，记录可以被持久化存储，且不会过期
- 参数expirationTime为PLATSA_NEVER_EXPIRE，记录不可以持久化存储，但不会过期
- 参数expirationTime为其他值，记录不可以持久化存储，且会过期

记录默认参数expirationTime为0，可以持久化存储。

过期的记录清除机制有两种：
- 惰性删除。进行数据库操作时有时会进行遍历，如果发现某个记录超时则删除
- 定时删除。周期性遍历所有记录，删除超时节点，周期为PLATSA_INTERVAL_CLEAR_EXPIRATION_NODE秒

## 初始化
示例：
```c
int mid = TZMallocRegister(RAM_INTERNAL, "platsa", 1024);
PlatsaLoad(mid);
```

## 示例1：增加，读取，重命名key，删除操作
示例1：
```c
static void case1(void) {
    intptr_t oid = PlatsaSet("ip", (uint8_t*)"192.168.1.101", strlen("192.168.1.101") + 1);

    int valueLen = 0;
    const char* value1 = (const char*)PlatsaGet("ip", &valueLen);
    printf("%s %d\n", value1, valueLen);

    const char* value2 = (const char*)PlatsaGetByOid(oid, NULL);
    printf("%s\n", value2);

    intptr_t oid2 = PlatsaGetOid("ip");
    printf("%d %d\n", oid, oid2);

    PlatsaRenameKey("ip", "ia");
    value2 = (const char*)PlatsaGet("ip", NULL);
    printf("%p\n", value2);

    value2 = (const char*)PlatsaGet("ia", NULL);
    printf("%s\n", value2);

    PlatsaDelete("ia");
    value2 = (const char*)PlatsaGet("ia", NULL);
    printf("%p\n", value2);

    PlatsaSet("ip", (uint8_t*)"192.168.1.102", strlen("192.168.1.102") + 1);
}
```

## 示例2：值为结构体
```c
struct tTest1 {
    int a;
    int b;
    int c;
};

static void case2(void) {
    struct tTest1 test1;
    test1.a = 1;
    test1.b = 2;
    test1.c = 3;
    PlatsaSet("testa", (uint8_t*)&test1, sizeof(struct tTest1));

    int valueLen = 0;
    const struct tTest1* test2 = (const struct tTest1*)PlatsaGet("testa", &valueLen);
    printf("%d %d %d %d %d\n", test2->a, test2->b, test2->c, valueLen, (int)sizeof(struct tTest1));

    PlatsaDelete("testa");
}
```

## 示例3：持久化存储和恢复
```c
static void case3(void) {
    printf("-------------------->test platsa save\n");

    PlatsaSet("t1", (uint8_t*)"abc1", strlen("abc1") + 1);
    PlatsaSet("t2", (uint8_t*)"abc2", strlen("abc2") + 1);
    PlatsaSet("t3", (uint8_t*)"abc3", strlen("abc3") + 1);

    PlatsaSave(0, PLATSA_INVALID_ADDR, FLASH_SIZE);

    PlatsaDelete("t1");
    PlatsaDelete("t2");
    PlatsaDelete("t3");

    PlatsaRecovery(0, PLATSA_INVALID_ADDR, FLASH_SIZE);
    const char* t1 = (const char*)PlatsaGet("t1", NULL);
    const char* t2 = (const char*)PlatsaGet("t2", NULL);
    const char* t3 = (const char*)PlatsaGet("t3", NULL);

    printf("%s %s %s\n", t1, t2, t3);

    PlatsaDelete("t1");
    PlatsaDelete("t2");
    PlatsaDelete("t3");
}
```
