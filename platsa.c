// Copyright 2020-2021 The jdh99 Authors. All rights reserved.
// 键值数据库
// Authors: jdh99 <jdh821@163.com>

#include "platsa.h"
#include "tzlist.h"
#include "tzmalloc.h"
#include "crc16.h"
#include "tzflash.h"
#include "tztime.h"
#include "async.h"
#include "tztype.h"

#include <string.h>

#define MAGIC_WORD 0x3782

#pragma pack(1)

typedef struct {
    uint64_t expirationTime;
    TZBufferDynamic* key;
    TZBufferDynamic* value;
} tItem;

typedef struct {
    uint16_t magic;
    uint32_t size;
    // crc校验的字段为bytes
    uint16_t crc16;
    uint8_t bytes[];
} tSave;

#pragma pack()

static int gMid = -1;
static intptr_t gList = 0;

static int task(void);
static void clearExpirationNode(void);
static TZListNode* createNode(void);
static bool writeFlash(uint32_t addr, int size, tSave* save);
static bool readFlash(uint32_t addr, int size);
static bool recovery(uint8_t* bytes, int size);

// PlatsaLoad 模块载入
// mid是tzmalloc的内存id
bool PlatsaLoad(int mid) {
    gMid = mid;
    gList = TZListCreateList(gMid);
    if (gList == 0) {
        return false;
    }
    return AsyncStart(task, PLATSA_INTERVAL_CLEAR_EXPIRATION_NODE * ASYNC_SECOND);
}

static int task(void) {
    static struct pt pt = {0};

    PT_BEGIN(&pt);

    clearExpirationNode();

    PT_END(&pt);
}

static void clearExpirationNode(void) {
    TZListNode* node = TZListGetHeader(gList);
    TZListNode* nodeNext = NULL;
    tItem* item = NULL;
    uint64_t now = TZTimeGet();
    for (;;) {
        if (node == NULL) {
            break;
        }
        nodeNext = node->Next;
        item = (tItem*)node->Data;
        if (item->expirationTime != 0 && item->expirationTime != (uint64_t)PLATSA_NEVER_EXPIRE && 
            now > item->expirationTime) {
            PlatsaDeleteByOid((intptr_t)node);
        }
        node = nodeNext;
    }
}

// PlatsaGetOid 获取OID
// 返回0表示失败
intptr_t PlatsaGetOid(char* key) {
    int keyLen = (int)strlen(key) + 1;
    if (keyLen > PLATSA_KEY_LEN_MAX) {
        return 0;
    }

    TZListNode* node = TZListGetHeader(gList);
    TZListNode* nodeNext = NULL;
    tItem* item = NULL;
    uint64_t now = TZTimeGet();
    for (;;) {
        if (node == NULL) {
            break;
        }
        nodeNext = node->Next;
        item = (tItem*)node->Data;
        do {
            if (item->key->len != keyLen) {
                break;
            }
            if (strcmp((char*)item->key->buf, key) != 0) {
                break;
            }

            if (node != TZListGetHeader(gList)) {
                // 将节点放在队首,加速下次查询
                TZListBreakLink(gList, node);
                TZListPrepend(gList, node);
            }
            return (intptr_t)node;
        } while (0);

        if (item->expirationTime != 0 && item->expirationTime != (uint64_t)PLATSA_NEVER_EXPIRE && 
            now > item->expirationTime) {
            PlatsaDeleteByOid((intptr_t)node);
        }
        node = nodeNext;
    }
    return 0;
}

// PlatsaSet 设置键对应的值.如果键不存在,会新建键
// 返回的是oid,如果为0表示设置失败
intptr_t PlatsaSet(char* key, uint8_t* bytes, int size) {
    TZBufferDynamic* newValueBuffer = (TZBufferDynamic*)TZMalloc(gMid, (int)sizeof(TZBufferDynamic) + size);
    if (newValueBuffer == NULL) {
        return 0;
    }
    memcpy(newValueBuffer->buf, bytes, (size_t)size);
    newValueBuffer->len = size;

    TZListNode* node = NULL;
    tItem* item = NULL;
    intptr_t oid = PlatsaGetOid(key);
    if (oid == 0) {
        node = createNode();
        if (node == NULL) {
            TZFree(newValueBuffer);
            return 0;
        }
        item = (tItem*)node->Data;

        int keyLen = (int)strlen(key) + 1;
        item->key = (TZBufferDynamic*)TZMalloc(gMid, (int)sizeof(TZBufferDynamic) + keyLen);
        if (item->key == NULL) {
            TZFree(newValueBuffer);
            return 0;
        }
        memcpy(item->key->buf, key, (size_t)keyLen);
        item->key->len = keyLen;
        
        TZListAppend(gList, node);
    } else {
        node = (TZListNode*)oid;
        item = (tItem*)node->Data;
        TZFree(item->value);
    }

    item->value = newValueBuffer;
    return (intptr_t)node;
}

static TZListNode* createNode(void) {
    TZListNode* node = TZListCreateNode(gList);
    if (node == NULL) {
        return NULL;
    }
    node->Data = TZMalloc(gMid, sizeof(tItem));
    if (node->Data == NULL) {
        TZFree(node);
        return NULL;
    }
    node->Size = sizeof(tItem);
    return node;
}

// PlatsaSetByOid 设置oid对应的值
bool PlatsaSetByOid(intptr_t oid, uint8_t* bytes, int size) {
    if (oid == 0) {
        return false;
    }

    TZBufferDynamic* newValueBuffer = (TZBufferDynamic*)TZMalloc(gMid, (int)sizeof(TZBufferDynamic) + size);
    if (newValueBuffer == NULL) {
        return false;
    }
    memcpy(newValueBuffer->buf, bytes, (size_t)size);
    newValueBuffer->len = size;

    TZListNode* node = (TZListNode*)oid;
    tItem* item = (tItem*)node->Data;
    TZFree(item->value);
    item->value = newValueBuffer;
    return true;
}

// PlatsaGet 获取值
// 返回如果为NULL,表示获取失败.值的长度保存在valueLen中
// 如果不需要保存值的长度,valueLen可传入NULL
// 注意:修改返回的值是不安全的,可能会引发内存泄漏
uint8_t* PlatsaGet(char* key, int* valueLen) {
    return PlatsaGetByOid(PlatsaGetOid(key), valueLen);
}

// PlatsaGetByOid 根据oid获取值
// 返回如果为NULL,表示获取失败.值的长度保存在valueLen中
// 如果不需要保存值的长度,valueLen可传入NULL
// 注意:修改返回的值是不安全的,可能会引发内存泄漏
uint8_t* PlatsaGetByOid(intptr_t oid, int* valueLen) {
    if (oid == 0) {
        return NULL;
    }
    TZListNode* node = (TZListNode*)oid;
    tItem* item = (tItem*)node->Data;
    if (valueLen != NULL) {
        *valueLen = item->value->len;
    }
    return item->value->buf;
}

// PlatsaDelete 删除key对应的记录
void PlatsaDelete(char* key) {
    PlatsaDeleteByOid(PlatsaGetOid(key));
}

// PlatsaDeleteByOid 删除key对应的记录
void PlatsaDeleteByOid(intptr_t oid) {
    if (oid == 0) {
        return;
    }
    TZListNode* node = (TZListNode*)oid;
    tItem* item = (tItem*)node->Data;
    TZFree(item->key);
    TZFree(item->value);
    TZListRemove(gList, node);
}

// PlatsaIsExistKey 判断key是否存在
bool PlatsaIsExistKey(char* key) {
    intptr_t oid = PlatsaGetOid(key);
    return oid != 0;
}

// PlatsaRenameKey 重命名key
bool PlatsaRenameKey(char* oldKey, char* newKey) {
    return PlatsaRenameKeyByOid(PlatsaGetOid(oldKey), newKey);
}

// PlatsaRenameKey 重命名key
bool PlatsaRenameKeyByOid(intptr_t oid, char* newKey) {
    int newKeyLen = (int)strlen(newKey) + 1;
    if (newKeyLen > PLATSA_KEY_LEN_MAX) {
        return false;
    }
    if (PlatsaIsExistKey(newKey)) {
        return false;
    }

    if (oid == 0) {
        return NULL;
    }
    TZBufferDynamic* newKeyBuffer = (TZBufferDynamic*)TZMalloc(gMid, (int)sizeof(TZBufferDynamic) + newKeyLen);
    if (newKeyBuffer == NULL) {
        return NULL;
    }
    memcpy(newKeyBuffer->buf, newKey, (size_t)newKeyLen);
    newKeyBuffer->len = newKeyLen;

    TZListNode* node = (TZListNode*)oid;
    tItem* item = (tItem*)node->Data;
    TZFree(item->key);
    item->key = newKeyBuffer;
    return true;
}

// PlatsaSetExipirationTime 设置过期时间
// expirationTime:单位:ms.当前时刻之后经过expirationTime毫秒过期
// 如果过期时间设置为0,表示不会过期,且可以持久化存储
// 如果过期时间设置为PLATSA_NEVER_EXPIRE,表示不会过期,但不可以持久化存储
void PlatsaSetExipirationTime(char* key, int expirationTime) {
    PlatsaSetExipirationTimeByOid(PlatsaGetOid(key), expirationTime);
}

// PlatsaSetExipirationTimeByOid 设置过期时间
// expirationTime:单位:ms.当前时刻之后经过expirationTime毫秒过期
// 如果过期时间设置为0,表示不会过期,且可以持久化存储
// 如果过期时间设置为PLATSA_NEVER_EXPIRE,表示不会过期,但不可以持久化存储
void PlatsaSetExipirationTimeByOid(intptr_t oid, int expirationTime) {
    if (oid == 0) {
        return;
    }
    TZListNode* node = (TZListNode*)oid;
    tItem* item = (tItem*)node->Data;
    if (expirationTime == PLATSA_NEVER_EXPIRE) {
        item->expirationTime = (uint64_t)PLATSA_NEVER_EXPIRE;
    } else {
        item->expirationTime = TZTimeGet() + (uint64_t)expirationTime * 1000;
    }
}

// PlatsaGetUsedSize 获取已经使用的内存字节数
// 0表示获取失败
int PlatsaGetUsedSize(void) {
    TZMallocUser* user = TZMallocGetUser(gMid);
    if (user == NULL) {
        return 0;
    }
    return (int)user->Used;
}

// PlatsaSave 持久化数据到磁盘
// primaryAddr是主要存储区,地址必须不为PLATSA_INVALID_ADDR
// standbyAddr是备份存储区,为PLATSA_INVALID_ADDR表示无备份存储区
// size是存储区字节数
// 成功返回true,失败返回false
bool PlatsaSave(uint32_t primaryAddr, uint32_t standbyAddr, int size) {
    if (primaryAddr == PLATSA_INVALID_ADDR) {
        return false;
    }
    
    int usedSize = PlatsaGetUsedSize();
    if (usedSize == 0) {
        return false;
    }
    
    tSave* save = (tSave*)TZMalloc(gMid, (int)sizeof(tSave) + usedSize);
    if (save == NULL) {
        return false;
    }
    save->magic = MAGIC_WORD;
    
    TZListNode* node = TZListGetHeader(gList);
    tItem* item = NULL;
    for (;;) {
        if (node == NULL) {
            break;
        }
        item = (tItem*)node->Data;
        // 有过期时间的节点不用持久化存储
        if (item->expirationTime != 0) {
            continue;
        }
        memcpy(save->bytes + save->size, (uint8_t*)item, sizeof(tItem));
        save->size += sizeof(tItem);
        memcpy(save->bytes + save->size, (uint8_t*)item->key, sizeof(TZBufferDynamic));
        save->size += sizeof(TZBufferDynamic);
        memcpy(save->bytes + save->size, item->key->buf, (size_t)item->key->len);
        save->size += (uint32_t)item->key->len;
        memcpy(save->bytes + save->size, (uint8_t*)item->value, sizeof(TZBufferDynamic));
        save->size += sizeof(TZBufferDynamic);
        memcpy(save->bytes + save->size, item->value->buf, (size_t)item->value->len);
        save->size += (uint32_t)item->value->len;

        node = node->Next;
    }
    if (save->size > 0) {
        save->crc16 = Crc16Checksum(save->bytes, (int)save->size);
    }
    if (save->size > (uint32_t)size) {
        TZFree(save);
        return false;
    }

    if (writeFlash(primaryAddr, size,  save) == false) {
        TZFree(save);
        return false;
    }
    if (standbyAddr != PLATSA_INVALID_ADDR) {
        (void)writeFlash(standbyAddr, size, save);
    }
    TZFree(save);
    return true;
}

static bool writeFlash(uint32_t addr, int size, tSave* save) {
    intptr_t handle = TZFlashOpen(addr, size, TZFLASH_WRITE_ONLY);
    if (handle == 0) {
        return false;
    }

    int needWriteSize = (int)sizeof(tSave) + (int)save->size;
    int ret = TZFlashWrite(handle, (uint8_t*)save, needWriteSize);
    TZFlashClose(handle);
    return ret >= needWriteSize;
}

// PlatsaRecovery 从磁盘恢复数据
// primaryAddr是主要存储区,地址必须不为PLATSA_INVALID_ADDR
// standbyAddr是备份存储区,为PLATSA_INVALID_ADDR表示无备份存储区
// size是存储区字节数
// 成功返回true,失败返回false
bool PlatsaRecovery(uint32_t primaryAddr, uint32_t standbyAddr, int size) {
    if (primaryAddr == PLATSA_INVALID_ADDR) {
        return false;
    }
    if (readFlash(primaryAddr, size)) {
        return true;
    }
    if (standbyAddr == PLATSA_INVALID_ADDR) {
        return false;
    }
    return readFlash(standbyAddr, size);
}

static bool readFlash(uint32_t addr, int size) {
    intptr_t handle = TZFlashOpen(addr, size, TZFLASH_READ_ONLY);
    if (handle == 0) {
        return false;
    }

    uint16_t crcCalc = 0;
    uint8_t* buf = NULL;
    tSave header;
    bool ret = false;
    if (TZFlashRead(handle, (uint8_t*)&header, sizeof(tSave)) != sizeof(tSave)) {
        goto CLOSE_FLASH;
    }
    if (header.magic != MAGIC_WORD || (int)header.size > size) {
        goto CLOSE_FLASH;
    }
    if (header.size == 0) {
        ret = true;
        goto CLOSE_FLASH;
    }
    
    buf = (uint8_t*)TZMalloc(gMid, (int)header.size);
    if (buf == NULL) {
        goto CLOSE_FLASH;
    }
    if (TZFlashRead(handle, buf, (int)header.size) != (int)header.size) {
        goto FREE_BUF;
    }
    crcCalc = Crc16Checksum(buf, (int)header.size);
    if (crcCalc != header.crc16) {
        goto FREE_BUF;
    }

    ret = recovery(buf, (int)header.size);
    goto FREE_BUF;

FREE_BUF:
    TZFree(buf);
CLOSE_FLASH:
    TZFlashClose(handle);
    return ret;
}

static bool recovery(uint8_t* bytes, int size) {
    int offset = 0;
    uint8_t* keyData = NULL;
    uint8_t* valueData = NULL;
    int valueSize = 0;
    tItem* item = NULL;

    for (;;) {
        item = (tItem*)(bytes + offset);
        offset += sizeof(tItem);

        item->key = (TZBufferDynamic*)(bytes + offset);
        offset += sizeof(TZBufferDynamic);
        keyData = bytes + offset;
        offset += item->key->len;

        item->value = (TZBufferDynamic*)(bytes + offset);
        offset += sizeof(TZBufferDynamic);
        valueData = bytes + offset;
        offset += item->value->len;
        valueSize = item->value->len;
        if (PlatsaSet((char*)keyData, valueData, valueSize) == false) {
            return false;
        }

        if (offset >= size) {
            break;
        }
    }
    return true;
}
