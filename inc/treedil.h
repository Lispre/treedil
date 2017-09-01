#ifndef TREEDIL_H
#define TREEDIL_H

#include <stdlib.h>
#include <stdint.h>

#include <vector>
#include <string>

namespace treedil
{

//db��Ϣ��ռ�ô�С������DbInfo�ṹ���С����0��Ϊ��������չ
static const size_t DB_INFO_SPACE_SIZE = 1024;

//block size��count����Сֵ
//ÿ��block����һ��tree node head�ҿ����˷�һ���֣�������Ҫ�涨��Сֵ����headռ�ÿռ�֮���ܱ����㹻������������
//������Сֵ�����Ͽ�����0��������һЩ��ؿ����ڿ����б���һЩ�߽����
static const uint8_t BLOCK_SIZE_MIN = 100;
static const uint32_t BLOCK_COUNT_MIN = 10;

//�ӿڵķ���ֵ������
enum RetCode
{
    RET_SUCCESS = 0,            //�ɹ�
    RET_KEY_NOT_EXIST = 1,      //Ҫ�ҵ�key������
    RET_TYPE_ERR = 2,           //�������ʹ���
    RET_RESULT_TOO_BIG = 3,     //���ؽ�������޷����
    RET_NOT_ENOUGH_SPACE = 4,   //�ռ䲻��
    RET_SPACE_CORRUPTED = 5,    //���ݿռ䱻�ƻ�
    RET_MOVE_TARGET_EXIST = 6,  //move������Ŀ����ڣ�db map��map�Ĳ����У�Ŀ��key�Ѵ��ڣ�
    RET_OUT_OF_RANGE = 7,       //�����±���Ԫ�ص�ʱ��Խ����
};

//���������
enum ObjType
{
    OBJ_TYPE_STR = 1,
    OBJ_TYPE_LIST = 2,
    OBJ_TYPE_MAP = 3,
};

//����db�Ľӿڶ���
class Db
{
public:
    virtual ~Db()
    {
    }

    //db������ӿ�--------------------------------

    //--------------------------------------------

    //key����Ĳ����ӿ�---------------------------

    //��ȡdb��kv������
    virtual RetCode kv_get_count(uint32_t &count) = 0;

    //��ȡһ��kv��value��obj type
    virtual RetCode kv_get_type(const char *key, uint32_t key_len, ObjType &type) = 0;
    virtual RetCode kv_get_type(const std::string &key, ObjType &type) = 0;

    //ɾ��һ��kv
    virtual RetCode kv_del(const char *key, uint32_t key_len) = 0;
    virtual RetCode kv_del(const std::string &key) = 0;

    //����һ��kv��key
    virtual RetCode kv_move(const char *key, uint32_t key_len, const char *new_key, uint32_t new_key_len) = 0;
    virtual RetCode kv_move(const std::string &key, const std::string &new_key) = 0;

    //���±꿪ʼ����ȡ��key�б����ȡ��need_count��
    virtual RetCode kv_scan(uint32_t idx, uint32_t need_count, std::vector<std::string> &keys) = 0;

    //--------------------------------------------

    //string���Ͳ����ӿ�--------------------------

    //��ȡһ��string���͵ĳ���
    virtual RetCode str_len(const char *key, uint32_t key_len, uint32_t &value_len) = 0;
    virtual RetCode str_len(const std::string &key, uint32_t &value_len) = 0;

    //����һ��string���͵�kv����key���������������������򸲸�ԭ���ݣ�ע�⣺ֱ�Ӹ��ǣ�����ԭ���ͣ�
    virtual RetCode str_set(const char *key, uint32_t key_len, const char *value, uint32_t value_len) = 0;
    virtual RetCode str_set(const std::string &key, const std::string &value) = 0;

    //��ȡһ��string���͵�kv
    virtual RetCode str_get(const char *key, uint32_t key_len, std::string &value) = 0;
    virtual RetCode str_get(const std::string &key, std::string &value) = 0;

    //��һ��string���͵�kv׷���ַ�������key����������Ϊ�մ����൱�ڵ�ͬstr_set��
    virtual RetCode str_append(const char *key, uint32_t key_len, const char *value, uint32_t value_len) = 0;
    virtual RetCode str_append(const std::string &key, const std::string &value) = 0;

    //����һ��string���͵�kv���Ӵ����ݣ���key����������Ϊ�մ������Ӵ�offset���������ַ���������ն����ֲ�Ϊ\0
    virtual RetCode str_set_sub_str(const char *key, uint32_t key_len, uint32_t offset, const char *value, uint32_t value_len) = 0;
    virtual RetCode str_set_sub_str(const std::string &key, uint32_t offset, const std::string &value) = 0;

    //��ȡһ��string���͵�kv���Ӵ�����key����������Ϊ�մ����Ӵ���ΧΪ[start, end]������ֹλ��Ϊ�������ʾ������x���ַ����Ǹ����ʾ�±�����
    //���ȴ���start��endΪ�������������֮����start>end���򷵻ؿմ�����startλ��С�ڴ��ף�������Ϊ���ף���end���ڴ�β��������Ϊ��β
    virtual RetCode str_get_sub_str(const char *key, uint32_t key_len, int64_t start, int64_t end, std::string &value) = 0;
    virtual RetCode str_get_sub_str(const std::string &key, int64_t start, int64_t end, std::string &value) = 0;

    //--------------------------------------------
};

//����db����ʼ��Ϊ�գ�db space�Ĵ�СӦ��=DB_INFO_SPACE_SIZE+block_size*block_count��block_size��block_countӦ�ò�С�����涨�����Сֵ
Db *create_db(char *space, size_t space_size, uint8_t block_size, uint32_t block_count);

//����һ���Ѵ��ڵ�db�����info��У��
Db *attach_db(char *space, size_t space_size);

//����space������db�����ò���dbΪNULL���������Db�������ɱ��������򷵻ش���
int detach_db(Db *&db);

}

#endif
