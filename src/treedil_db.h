#ifndef TREEDIL_DB_H
#define TREEDIL_DB_H

#include <stdlib.h>
#include <time.h>

#include "treedil.h"

#include "treedil_util.h"
#include "treedil_tree.h"

namespace treedil
{

static const uint32_t DB_INFO_MAGIC = 0x20170425;

#pragma pack(push, 1)

//db信息，存放在space头部
struct DbInfo
{
    uint64_t calc_check_sum() const
    {
        return str_hash((const uint8_t *)this, (const uint8_t *)&check_sum - (const uint8_t *)this);
    }

    void set_check_sum()
    {
        check_sum = calc_check_sum();
    }

    bool valid_check_sum() const
    {
        return check_sum == calc_check_sum();
    }

    bool is_valid(size_t space_size) const
    {
        if (magic != DB_INFO_MAGIC || block_size < BLOCK_SIZE_MIN || !valid_check_sum() || block_count < BLOCK_COUNT_MIN ||
            create_time < modify_time || reached_block_count > block_count || used_block_count > block_count || free_block_tree >= block_count)
        {
            return false;
        }

        if (space_size != DB_INFO_SPACE_SIZE + (size_t)block_size * block_count)
        {
            return false;
        }

        return true;
    }

    //静态信息
    volatile uint32_t magic; //magic头
    volatile uint32_t create_time; //db创建时间戳
    volatile uint8_t serial_no[32]; //随机生成的序列号
    volatile uint8_t block_size;

    volatile uint64_t check_sum; //由前面静态信息计算得到的check_sum

    //动态信息
    volatile uint32_t block_count;
    volatile uint32_t modify_time; //最后修改时间
    volatile uint32_t used_block_count; //已使用的block_count
    volatile uint32_t reached_block_count; //顺序占用的block_count
    volatile uint32_t free_block_tree; //空闲block形成的tree
    //reached_block_count = used_block_count + free_list的block数量
    volatile uint32_t db_map; //指向db_map
};

#pragma pack(pop)

//对Db接口的实现，由于db层要做的事情比较多，因此方法实现分在多个源文件中，并不是都实现在treedil_db.cpp
class DbImp : public Db
{
public:
    //创建类操作用静态接口
    static DbImp *create_db(uint8_t *space, size_t space_size, uint8_t block_size, uint32_t block_count);
    static DbImp *attach_db(uint8_t *space, size_t space_size);

    void detach();

    //以下为继承自Db中的接口，详细注释参考Db的定义

    virtual RetCode kv_get_count(uint32_t &count);

    virtual RetCode kv_get_type(const char *key, uint32_t key_len, ObjType &type);
    virtual RetCode kv_get_type(const std::string &key, ObjType &type)
    {
        return kv_get_type(key.data(), (uint32_t)key.size(), type);
    }

    virtual RetCode kv_del(const char *key, uint32_t key_len);
    virtual RetCode kv_del(const std::string &key)
    {
        return kv_del(key.data(), (uint32_t)key.size());
    }

    virtual RetCode kv_move(const char *key, uint32_t key_len, const char *new_key, uint32_t new_key_len);
    virtual RetCode kv_move(const std::string &key, const std::string &new_key)
    {
        return kv_move(key.data(), (uint32_t)key.size(), new_key.data(), (uint32_t)new_key.size());
    }

    virtual RetCode kv_scan(uint32_t idx, uint32_t need_count, std::vector<std::string> &keys);

    virtual RetCode str_len(const char *key, uint32_t key_len, uint32_t &value_len);
    virtual RetCode str_len(const std::string &key, uint32_t &value_len)
    {
        return str_len(key.data(), (uint32_t)key.size(), value_len);
    }

    virtual RetCode str_set(const char *key, uint32_t key_len, const char *value, uint32_t value_len);
    virtual RetCode str_set(const std::string &key, const std::string &value)
    {
        return str_set(key.data(), (uint32_t)key.size(), value.data(), (uint32_t)value.size());
    }

    virtual RetCode str_get(const char *key, uint32_t key_len, std::string &value);
    virtual RetCode str_get(const std::string &key, std::string &value)
    {
        return str_get(key.data(), (uint32_t)key.size(), value);
    }

    virtual RetCode str_append(const char *key, uint32_t key_len, const char *value, uint32_t value_len);
    virtual RetCode str_append(const std::string &key, const std::string &value)
    {
        return str_append(key.data(), (uint32_t)key.size(), value.data(), (uint32_t)value.size());
    }

    virtual RetCode str_set_sub_str(const char *key, uint32_t key_len, uint32_t offset, const char *value, uint32_t value_len);
    virtual RetCode str_set_sub_str(const std::string &key, uint32_t offset, const std::string &value)
    {
        return str_set_sub_str(key.data(), (uint32_t)key.size(), offset, value.data(), (uint32_t)value.size());
    }

    virtual RetCode str_get_sub_str(const char *key, uint32_t key_len, int64_t start, int64_t end, std::string &value);
    virtual RetCode str_get_sub_str(const std::string &key, int64_t start, int64_t end, std::string &value)
    {
        return str_get_sub_str(key.data(), (uint32_t)key.size(), start, end, value);
    }

private:
    DbImp();

    void set_modify_time()
    {
        db_info->modify_time = (uint32_t)time(NULL);
    }

    TreeNode *get_tree_node(uint32_t block_idx)
    {
        return (TreeNode *)(block_start + (size_t)block_idx * db_info->block_size);
    }

    //从db map找key，返回key所在的节点idx，找不到则返回nil
    //若返回nil且insert_handler不为NULL，则*insert_handler和*parent_block_idx分别存放插入后挂载的位置以及父节点idx
    //注意insert_handler若不为NULL，则parent_block_idx也不能为NULL，若返回不是nil，则这两个变量指向的位置的内容无意义
    uint32_t find_in_db_map(const uint8_t *key, uint32_t key_len, volatile uint32_t **insert_handler, uint32_t *parent_block_idx);
    //根据下标索引定位db_map中的元素，返回节点idx，找不到则返回nil
    uint32_t locate_in_db_map(uint32_t idx);

    int compare_with_db_str(const uint8_t *s, uint32_t s_len, volatile const uint8_t *db_s, uint8_t db_s_len);
    RetCode get_db_str_sub_str(volatile const uint8_t *db_s, uint8_t db_s_len, int64_t start, int64_t end, std::string &s);

    //从一个long str的part中获取内容指针和长度
    void get_long_str_part(uint32_t block_idx, volatile const uint8_t *&s, uint8_t &s_len);
    //新建一个long str，并挂载在handler处，long str的parent为parent_block_idx，注意，这个函数必须保证在出错的时候不会更改handler的原有内容
    RetCode new_long_str(const uint8_t *s, uint32_t s_len, uint32_t parent_block_idx, volatile uint32_t &handler);
    //在一个long str中根据start指定的位置查找对应的block，并将start调整为对应block中的相对开始位置
    uint32_t long_str_locate_sub_str_by_start_idx(uint32_t block_idx, int64_t &start);
    //在handler引用的long str末尾append str
    RetCode long_str_append(volatile uint32_t &handler, const uint8_t *s, uint32_t s_len);

    //从block_idx为根的树中找到最开始或最后的节点
    uint32_t get_first_block_node(uint32_t block_idx);
    uint32_t get_last_block_node(uint32_t block_idx);
    //从block_idx出发找前驱或后继
    uint32_t get_block_node_prev(uint32_t block_idx);
    uint32_t get_block_node_succ(uint32_t block_idx);
    //释放tree对象
    void free_tree_obj(uint32_t block_idx);
    //从树中删除节点 & 释放节点 & 删除释放组合操作
    void del_block_node(uint32_t block_idx, volatile uint32_t *root_handler);
    void free_block_node(uint32_t block_idx);
    void del_and_free_block_node(uint32_t block_idx, volatile uint32_t *root_handler);
    //获取node handler，即parent中引用它的left或right域的指针，如已经是根节点，则使用指定的root_handler
    volatile uint32_t *get_node_handler(uint32_t block_idx, volatile uint32_t *root_handler);
    //在插入节点后，从插入的节点（必然是最下面的叶子）向上调整path中节点的size域
    void adjust_size_after_insert(uint32_t block_idx);
    //直接调整节点size，包括本节点和向上的path的所有节点，调整方式为+size（向下调整等于加一个负数强转uint32_t的结果，利用无符号回环特性）
    void adjust_size(uint32_t block_idx, uint32_t size);
    //旋转相关过程
    void rotate_inserted_block_node(uint32_t block_idx, volatile uint32_t *root_handler); //将插入到底部的节点向上旋转到适当位置
    void rotate_left(uint32_t block_idx, volatile uint32_t *root_handler); //左旋
    void rotate_right(uint32_t block_idx, volatile uint32_t *root_handler); //右旋
    //申请block相关接口
    uint32_t get_free_block_count()
    {
        //获取当前剩余可用的block数量
        return db_info->block_count - db_info->used_block_count;
    }
    uint32_t alloc_block(); //申请一个block，返回idx，失败返回nil

    DbInfo *db_info;
    uint8_t *block_start;
};

}

#endif
