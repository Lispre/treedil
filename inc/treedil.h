#ifndef TREEDIL_H
#define TREEDIL_H

#include <stdlib.h>
#include <stdint.h>

#include <vector>
#include <string>

namespace treedil
{

//db信息所占用大小，超出DbInfo结构体大小的填0，为将来做扩展
static const size_t DB_INFO_SPACE_SIZE = 1024;

//block size和count的最小值
//每个block会有一个tree node head且可能浪费一部分，所以需要规定最小值，在head占用空间之外能保留足够比例的数据区
//数量最小值理论上可以是0，不过留一些余地可以在开发中避免一些边界情况
static const uint8_t BLOCK_SIZE_MIN = 100;
static const uint32_t BLOCK_COUNT_MIN = 10;

//接口的返回值错误码
enum RetCode
{
    RET_SUCCESS = 0,            //成功
    RET_KEY_NOT_EXIST = 1,      //要找的key不存在
    RET_TYPE_ERR = 2,           //数据类型错误
    RET_RESULT_TOO_BIG = 3,     //返回结果过大，无法存放
    RET_NOT_ENOUGH_SPACE = 4,   //空间不足
    RET_SPACE_CORRUPTED = 5,    //数据空间被破坏
    RET_MOVE_TARGET_EXIST = 6,  //move操作的目标存在（db map和map的操作中，目标key已存在）
    RET_OUT_OF_RANGE = 7,       //根据下标找元素的时候越界了
};

//对象的类型
enum ObjType
{
    OBJ_TYPE_STR = 1,
    OBJ_TYPE_LIST = 2,
    OBJ_TYPE_MAP = 3,
};

//操作db的接口对象
class Db
{
public:
    virtual ~Db()
    {
    }

    //db控制类接口--------------------------------

    //--------------------------------------------

    //key级别的操作接口---------------------------

    //获取db中kv的数量
    virtual RetCode kv_get_count(uint32_t &count) = 0;

    //获取一个kv的value的obj type
    virtual RetCode kv_get_type(const char *key, uint32_t key_len, ObjType &type) = 0;
    virtual RetCode kv_get_type(const std::string &key, ObjType &type) = 0;

    //删除一个kv
    virtual RetCode kv_del(const char *key, uint32_t key_len) = 0;
    virtual RetCode kv_del(const std::string &key) = 0;

    //更改一个kv的key
    virtual RetCode kv_move(const char *key, uint32_t key_len, const char *new_key, uint32_t new_key_len) = 0;
    virtual RetCode kv_move(const std::string &key, const std::string &new_key) = 0;

    //从下标开始遍历取出key列表，最多取出need_count个
    virtual RetCode kv_scan(uint32_t idx, uint32_t need_count, std::vector<std::string> &keys) = 0;

    //--------------------------------------------

    //string类型操作接口--------------------------

    //获取一个string类型的长度
    virtual RetCode str_len(const char *key, uint32_t key_len, uint32_t &value_len) = 0;
    virtual RetCode str_len(const std::string &key, uint32_t &value_len) = 0;

    //设置一个string类型的kv，若key不存在则新增，若存在则覆盖原数据（注意：直接覆盖，无视原类型）
    virtual RetCode str_set(const char *key, uint32_t key_len, const char *value, uint32_t value_len) = 0;
    virtual RetCode str_set(const std::string &key, const std::string &value) = 0;

    //读取一个string类型的kv
    virtual RetCode str_get(const char *key, uint32_t key_len, std::string &value) = 0;
    virtual RetCode str_get(const std::string &key, std::string &value) = 0;

    //向一个string类型的kv追加字符串，若key不存在则视为空串（相当于等同str_set）
    virtual RetCode str_append(const char *key, uint32_t key_len, const char *value, uint32_t value_len) = 0;
    virtual RetCode str_append(const std::string &key, const std::string &value) = 0;

    //设置一个string类型的kv的子串内容，若key不存在则视为空串，若子串offset超过已有字符串长度则空洞部分补为\0
    virtual RetCode str_set_sub_str(const char *key, uint32_t key_len, uint32_t offset, const char *value, uint32_t value_len) = 0;
    virtual RetCode str_set_sub_str(const std::string &key, uint32_t offset, const std::string &value) = 0;

    //读取一个string类型的kv的子串，若key不存在则视为空串，子串范围为[start, end]，若起止位置为负数则表示倒数第x个字符，非负则表示下标索引
    //首先处理start和end为负参数的情况，之后若start>end，则返回空串；若start位置小于串首，则修正为串首；若end大于串尾，则修正为串尾
    virtual RetCode str_get_sub_str(const char *key, uint32_t key_len, int64_t start, int64_t end, std::string &value) = 0;
    virtual RetCode str_get_sub_str(const std::string &key, int64_t start, int64_t end, std::string &value) = 0;

    //--------------------------------------------
};

//创建db并初始化为空，db space的大小应该=DB_INFO_SPACE_SIZE+block_size*block_count，block_size和block_count应该不小于上面定义的最小值
Db *create_db(char *space, size_t space_size, uint8_t block_size, uint32_t block_count);

//挂载一个已存在的db，会对info做校验
Db *attach_db(char *space, size_t space_size);

//脱离space、销毁db、设置参数db为NULL，若传入的Db对象不是由本库生成则返回错误
int detach_db(Db *&db);

}

#endif
