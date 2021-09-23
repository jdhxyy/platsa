// Copyright 2020-2021 The jdh99 Authors. All rights reserved.
// ��ֵ���ݿ�
// Authors: jdh99 <jdh821@163.com>
// ��¼(record)��ʽ:
//   oid|key|value
//   oid:object id.ÿ����¼��Ψһid
//   key:��ֵ,����Ψһ,�������ַ���
//   value:�����������ʽ�ֽ���
// ��¼�й������Ե�����,���Զ�������ڽڵ�.����ʱ��Ϊ0��ʾ�������
// �־û��洢��ͬʱ������,ֻ��洢������ڵĽڵ�
// ���Ҫʹ�ó־û��洢�Ĺ���,���뱣֤����ʹ�ó���һ����ڴ�,
// ��Ϊ�־û��洢���л��ƻ��Ƚ�����ȫ�����յĴ洢��һ���������ڴ���

#ifndef PLATSA_H
#define PLATSA_H

#include <stdint.h>
#include <stdbool.h>

// �key�ֽ���
#define PLATSA_KEY_LEN_MAX 512

// ������ڽڵ�ļ��.��λ:s
#define PLATSA_INTERVAL_CLEAR_EXPIRATION_NODE 10

// ��Ч��ַ
#define PLATSA_INVALID_ADDR 0xffffffff

// ��������.�������Գ־û��洢
#define PLATSA_NEVER_EXPIRE -1

// PlatsaLoad ģ������
// mid��tzmalloc���ڴ�id
bool PlatsaLoad(int mid);

// PlatsaGetOid ��ȡOID
// ����0��ʾʧ��
intptr_t PlatsaGetOid(char* key);

// PlatsaSet ���ü���Ӧ��ֵ.�����������,���½���
// ���ص���oid,���Ϊ0��ʾ����ʧ��
intptr_t PlatsaSet(char* key, uint8_t* bytes, int size);

// PlatsaSetByOid ����oid��Ӧ��ֵ
bool PlatsaSetByOid(intptr_t oid, uint8_t* bytes, int size);

// PlatsaGet ��ȡֵ
// �������ΪNULL,��ʾ��ȡʧ��.ֵ�ĳ��ȱ�����valueLen��
// �������Ҫ����ֵ�ĳ���,valueLen�ɴ���NULL
// ע��:�޸ķ��ص�ֵ�ǲ���ȫ��,���ܻ������ڴ�й©
uint8_t* PlatsaGet(char* key, int* valueLen);

// PlatsaGetByOid ����oid��ȡֵ
// �������ΪNULL,��ʾ��ȡʧ��.ֵ�ĳ��ȱ�����valueLen��
// �������Ҫ����ֵ�ĳ���,valueLen�ɴ���NULL
// ע��:�޸ķ��ص�ֵ�ǲ���ȫ��,���ܻ������ڴ�й©
uint8_t* PlatsaGetByOid(intptr_t oid, int* valueLen);

// PlatsaDelete ɾ��key��Ӧ�ļ�¼
void PlatsaDelete(char* key);

// PlatsaDeleteByOid ɾ��key��Ӧ�ļ�¼
void PlatsaDeleteByOid(intptr_t oid);

// PlatsaIsExistKey �ж�key�Ƿ����
bool PlatsaIsExistKey(char* key);

// PlatsaRenameKey ������key
bool PlatsaRenameKey(char* oldKey, char* newKey);

// PlatsaRenameKey ������key
bool PlatsaRenameKeyByOid(intptr_t oid, char* newKey);

// PlatsaSetExipirationTime ���ù���ʱ��
// expirationTime:��λ:ms.��ǰʱ��֮�󾭹�expirationTime�������
// �������ʱ������Ϊ0,��ʾ�������,�ҿ��Գ־û��洢
// �������ʱ������ΪPLATSA_NEVER_EXPIRE,��ʾ�������,�������Գ־û��洢
void PlatsaSetExipirationTime(char* key, int expirationTime);

// PlatsaSetExipirationTimeByOid ���ù���ʱ��
// expirationTime:��λ:ms.��ǰʱ��֮�󾭹�expirationTime�������
// �������ʱ������Ϊ0,��ʾ�������,�ҿ��Գ־û��洢
// �������ʱ������ΪPLATSA_NEVER_EXPIRE,��ʾ�������,�������Գ־û��洢
void PlatsaSetExipirationTimeByOid(intptr_t oid, int expirationTime);

// PlatsaGetUsedSize ��ȡ�Ѿ�ʹ�õ��ڴ��ֽ���
// 0��ʾ��ȡʧ��
int PlatsaGetUsedSize(void);

// PlatsaSave �־û����ݵ�����
// primaryAddr����Ҫ�洢��,��ַ���벻ΪPLATSA_INVALID_ADDR
// standbyAddr�Ǳ��ݴ洢��,ΪPLATSA_INVALID_ADDR��ʾ�ޱ��ݴ洢��
// size�Ǵ洢���ֽ���
// �ɹ�����true,ʧ�ܷ���false
bool PlatsaSave(uint32_t primaryAddr, uint32_t standbyAddr, int size);

// PlatsaRecovery �Ӵ��ָ̻�����
// primaryAddr����Ҫ�洢��,��ַ���벻ΪPLATSA_INVALID_ADDR
// standbyAddr�Ǳ��ݴ洢��,ΪPLATSA_INVALID_ADDR��ʾ�ޱ��ݴ洢��
// size�Ǵ洢���ֽ���
// �ɹ�����true,ʧ�ܷ���false
bool PlatsaRecovery(uint32_t primaryAddr, uint32_t standbyAddr, int size);

#endif
