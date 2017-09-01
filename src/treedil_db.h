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

//db��Ϣ�������spaceͷ��
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

    //��̬��Ϣ
    volatile uint32_t magic; //magicͷ
    volatile uint32_t create_time; //db����ʱ���
    volatile uint8_t serial_no[32]; //������ɵ����к�
    volatile uint8_t block_size;

    volatile uint64_t check_sum; //��ǰ�澲̬��Ϣ����õ���check_sum

    //��̬��Ϣ
    volatile uint32_t block_count;
    volatile uint32_t modify_time; //����޸�ʱ��
    volatile uint32_t used_block_count; //��ʹ�õ�block_count
    volatile uint32_t reached_block_count; //˳��ռ�õ�block_count
    volatile uint32_t free_block_tree; //����block�γɵ�tree
    //reached_block_count = used_block_count + free_list��block����
    volatile uint32_t db_map; //ָ��db_map
};

#pragma pack(pop)

//��Db�ӿڵ�ʵ�֣�����db��Ҫ��������Ƚ϶࣬��˷���ʵ�ַ��ڶ��Դ�ļ��У������Ƕ�ʵ����treedil_db.cpp
class DbImp : public Db
{
public:
    //����������þ�̬�ӿ�
    static DbImp *create_db(uint8_t *space, size_t space_size, uint8_t block_size, uint32_t block_count);
    static DbImp *attach_db(uint8_t *space, size_t space_size);

    void detach();

    //����Ϊ�̳���Db�еĽӿڣ���ϸע�Ͳο�Db�Ķ���

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

    //��db map��key������key���ڵĽڵ�idx���Ҳ����򷵻�nil
    //������nil��insert_handler��ΪNULL����*insert_handler��*parent_block_idx�ֱ��Ų������ص�λ���Լ����ڵ�idx
    //ע��insert_handler����ΪNULL����parent_block_idxҲ����ΪNULL�������ز���nil��������������ָ���λ�õ�����������
    uint32_t find_in_db_map(const uint8_t *key, uint32_t key_len, volatile uint32_t **insert_handler, uint32_t *parent_block_idx);
    //�����±�������λdb_map�е�Ԫ�أ����ؽڵ�idx���Ҳ����򷵻�nil
    uint32_t locate_in_db_map(uint32_t idx);

    int compare_with_db_str(const uint8_t *s, uint32_t s_len, volatile const uint8_t *db_s, uint8_t db_s_len);
    RetCode get_db_str_sub_str(volatile const uint8_t *db_s, uint8_t db_s_len, int64_t start, int64_t end, std::string &s);

    //��һ��long str��part�л�ȡ����ָ��ͳ���
    void get_long_str_part(uint32_t block_idx, volatile const uint8_t *&s, uint8_t &s_len);
    //�½�һ��long str����������handler����long str��parentΪparent_block_idx��ע�⣬����������뱣֤�ڳ����ʱ�򲻻����handler��ԭ������
    RetCode new_long_str(const uint8_t *s, uint32_t s_len, uint32_t parent_block_idx, volatile uint32_t &handler);
    //��һ��long str�и���startָ����λ�ò��Ҷ�Ӧ��block������start����Ϊ��Ӧblock�е���Կ�ʼλ��
    uint32_t long_str_locate_sub_str_by_start_idx(uint32_t block_idx, int64_t &start);
    //��handler���õ�long strĩβappend str
    RetCode long_str_append(volatile uint32_t &handler, const uint8_t *s, uint32_t s_len);

    //��block_idxΪ���������ҵ��ʼ�����Ľڵ�
    uint32_t get_first_block_node(uint32_t block_idx);
    uint32_t get_last_block_node(uint32_t block_idx);
    //��block_idx������ǰ������
    uint32_t get_block_node_prev(uint32_t block_idx);
    uint32_t get_block_node_succ(uint32_t block_idx);
    //�ͷ�tree����
    void free_tree_obj(uint32_t block_idx);
    //������ɾ���ڵ� & �ͷŽڵ� & ɾ���ͷ���ϲ���
    void del_block_node(uint32_t block_idx, volatile uint32_t *root_handler);
    void free_block_node(uint32_t block_idx);
    void del_and_free_block_node(uint32_t block_idx, volatile uint32_t *root_handler);
    //��ȡnode handler����parent����������left��right���ָ�룬���Ѿ��Ǹ��ڵ㣬��ʹ��ָ����root_handler
    volatile uint32_t *get_node_handler(uint32_t block_idx, volatile uint32_t *root_handler);
    //�ڲ���ڵ�󣬴Ӳ���Ľڵ㣨��Ȼ���������Ҷ�ӣ����ϵ���path�нڵ��size��
    void adjust_size_after_insert(uint32_t block_idx);
    //ֱ�ӵ����ڵ�size���������ڵ�����ϵ�path�����нڵ㣬������ʽΪ+size�����µ������ڼ�һ������ǿתuint32_t�Ľ���������޷��Żػ����ԣ�
    void adjust_size(uint32_t block_idx, uint32_t size);
    //��ת��ع���
    void rotate_inserted_block_node(uint32_t block_idx, volatile uint32_t *root_handler); //�����뵽�ײ��Ľڵ�������ת���ʵ�λ��
    void rotate_left(uint32_t block_idx, volatile uint32_t *root_handler); //����
    void rotate_right(uint32_t block_idx, volatile uint32_t *root_handler); //����
    //����block��ؽӿ�
    uint32_t get_free_block_count()
    {
        //��ȡ��ǰʣ����õ�block����
        return db_info->block_count - db_info->used_block_count;
    }
    uint32_t alloc_block(); //����һ��block������idx��ʧ�ܷ���nil

    DbInfo *db_info;
    uint8_t *block_start;
};

}

#endif
