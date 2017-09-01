//直接存取db的kv级别的元素的接口实现

#include <string.h>

#include "treedil_db.h"

namespace treedil
{

/*
db_map的实现较为特殊，和数据结构map有点不一样
每个kv占用一个完整的block，不做压缩存储，主要是为了这一级存储结构的实现方便，以及将来可以扩展一些针对每个key的管理信息
结构：TreeNode+Kv+key+value紧凑存储
其中Head中的三个len，如果为TREE_OBJ_TAG则表示引用了外部对象，此时后面存放的将是一个uint_32的block_idx（四字节）
*/

//一个特殊的长度，标识此数据引用了外部的tree对象（自定义信息或key的话当然只能是外部str）
static const uint8_t TREE_OBJ_TAG = 0xFF;

#pragma pack(push, 1)

//dbmap的头信息结构，位于TreeNode的data域
struct Kv
{
    //获取key和value的offset
    void get_offset(size_t &key_start, size_t &value_start) const
    {
        key_start = 0;
        value_start = key_start + (key_len == TREE_OBJ_TAG ? sizeof(uint32_t) : key_len);
    }

    void get_space_info(const DbInfo *db_info, size_t &key_cost, size_t &value_cost, size_t &block_space_total, size_t &key_space_max,
                        size_t &value_space_max) const
    {
        key_cost = key_len == TREE_OBJ_TAG ? sizeof(uint32_t) : key_len;
        value_cost = value_len == TREE_OBJ_TAG ? sizeof(uint32_t) : value_len;
        block_space_total = (size_t)db_info->block_size - sizeof(TreeNode) - sizeof(Kv);
        key_space_max = block_space_total - value_cost;
        value_space_max = block_space_total - key_cost;
    }

    //初始化value为str的情况，设置三个长度
    void init_str_value(uint32_t block_size, uint32_t key_len_arg, uint32_t value_len_arg)
    {
        uint32_t data_size_max = (uint32_t)(block_size - sizeof(TreeNode) - sizeof(Kv)); //block内部最大可用空间

        uint32_t left_size = data_size_max;

        if (key_len_arg > (uint32_t)(left_size - sizeof(uint32_t))) //剩余空间需要减掉给后面字段保留的uint32_t，下同
        {
            //key存放不下，需要做成外部链接
            key_len = TREE_OBJ_TAG;
            left_size -= (uint32_t)sizeof(uint32_t);
        }
        else
        {
            //可以直接存
            key_len = (uint8_t)key_len_arg;
            left_size -= key_len;
        }

        if (value_len_arg > left_size)
        {
            //value存放不下，需要做成外部链接
            value_len = TREE_OBJ_TAG;
            left_size -= (uint32_t)sizeof(uint32_t);
        }
        else
        {
            //可以直接存
            value_len = (uint8_t)value_len_arg;
            left_size -= value_len;
        }
    }

    //初始化value为外部链接的tree的情况，设置三个长度
    void init_tree_value(uint32_t block_size, uint32_t key_len_arg)
    {
        //trick：假装value是一个长度为sizeof(uint32_t)的字符串，调用init_str_value后再改value_len
        init_str_value(block_size, key_len_arg, (uint32_t)sizeof(uint32_t));
        value_len = TREE_OBJ_TAG;
    }

    volatile uint8_t key_len; //key长度
    volatile uint8_t value_len; //value长度

    volatile uint8_t data[0]; //数据区开始
};

#pragma pack(pop)

RetCode DbImp::kv_get_count(uint32_t &count)
{
    if (db_info->db_map == BLOCK_IDX_NIL)
    {
        count = 0;
        return RET_SUCCESS;
    }

    TreeNode *node = get_tree_node(db_info->db_map);
    count = node->size;

    return RET_SUCCESS;
}

RetCode DbImp::kv_get_type(const char *key, uint32_t key_len, ObjType &type)
{
    uint32_t block_idx = find_in_db_map((const uint8_t *)key, key_len, NULL, NULL);
    if (block_idx == BLOCK_IDX_NIL)
    {
        return RET_KEY_NOT_EXIST;
    }

    TreeNode *node = get_tree_node(block_idx);
    Kv *kv = (Kv *)node->data;
    size_t key_start, value_start;
    kv->get_offset(key_start, value_start);
    if (kv->value_len != TREE_OBJ_TAG)
    {
        type = OBJ_TYPE_STR;
        return RET_SUCCESS;
    }

    TreeNode *obj_root_node = get_tree_node(*(volatile uint32_t *)(kv->data + value_start));
    switch (obj_root_node->type)
    {
        case TREE_NODE_TYPE_STR:
        {
            type = OBJ_TYPE_STR;
            break;
        }
        case TREE_NODE_TYPE_LIST:
        {
            type = OBJ_TYPE_LIST;
            break;
        }
        case TREE_NODE_TYPE_MAP:
        {
            type = OBJ_TYPE_MAP;
            break;
        }
        default:
        {
            return RET_SPACE_CORRUPTED;
        }
    }

    return RET_SUCCESS;
}

RetCode DbImp::kv_del(const char *key, uint32_t key_len)
{
    uint32_t block_idx = find_in_db_map((const uint8_t *)key, key_len, NULL, NULL);
    if (block_idx == BLOCK_IDX_NIL)
    {
        return RET_KEY_NOT_EXIST;
    }

    //找到了，先释放可能存在的外部数据结构
    TreeNode *node = get_tree_node(block_idx);
    Kv *kv = (Kv *)node->data;
    size_t key_start, value_start;
    kv->get_offset(key_start, value_start);
    if (kv->key_len == TREE_OBJ_TAG)
    {
        free_tree_obj(*(volatile uint32_t *)(kv->data + key_start));
    }
    if (kv->value_len == TREE_OBJ_TAG)
    {
        free_tree_obj(*(volatile uint32_t *)(kv->data + value_start));
    }

    //然后干掉这个节点，从db map摘除并释放
    del_and_free_block_node(block_idx, &db_info->db_map);

    set_modify_time();

    return RET_SUCCESS;
}

RetCode DbImp::kv_move(const char *key, uint32_t key_len, const char *new_key, uint32_t new_key_len)
{
    //先检查目标是否存在
    if (find_in_db_map((const uint8_t *)new_key, new_key_len, NULL, NULL) != BLOCK_IDX_NIL)
    {
        return RET_MOVE_TARGET_EXIST;
    }

    uint32_t block_idx = find_in_db_map((const uint8_t *)key, key_len, NULL, NULL);
    if (block_idx == BLOCK_IDX_NIL)
    {
        return RET_KEY_NOT_EXIST;
    }

    //摘除节点
    del_block_node(block_idx, &db_info->db_map);

    //计算key的可用空间等信息
    TreeNode *node = get_tree_node(block_idx);
    Kv *kv = (Kv *)node->data;
    size_t key_start, value_start;
    kv->get_offset(key_start, value_start);
    size_t key_cost, value_cost, block_space_total, key_space_max, value_space_max;
    kv->get_space_info(db_info, key_cost, value_cost, block_space_total, key_space_max, value_space_max);

    //不动value的布局，直接改key
    uint8_t new_data[256];
    size_t new_data_len = 0;
    //存新key，分情况进行
    uint8_t new_kv_key_len;
    if (new_key_len <= key_space_max)
    {
        //新key可以直接存下
        new_kv_key_len = (uint8_t)new_key_len;
        memcpy(new_data + new_data_len, new_key, new_key_len);
        new_data_len += new_key_len;
    }
    else
    {
        //新key必须做外部链接
        new_kv_key_len = TREE_OBJ_TAG;
        //新建一个str tree obj
        RetCode ret = new_long_str((const uint8_t *)new_key, new_key_len, block_idx, *(volatile uint32_t *)(new_data + new_data_len));
        if (ret != RET_SUCCESS)
        {
            return ret;
        }
        new_data_len += sizeof(uint32_t);
    }
    //再拷贝value数据
    memcpy((void *)(new_data + new_data_len), (const void *)(kv->data + value_start), value_cost);
    new_data_len += value_cost;

    //准备写回数据，如果老key是外部结构，则释放掉
    if (kv->key_len == TREE_OBJ_TAG)
    {
        free_tree_obj(*(volatile uint32_t *)(kv->data + key_start));
    }

    //写回数据
    memcpy((void *)kv->data, (const void *)new_data, new_data_len);
    kv->key_len = new_kv_key_len;

    //插入节点到db map
    volatile uint32_t *insert_handler;
    uint32_t parent_block_idx;
    find_in_db_map((const uint8_t *)new_key, new_key_len, &insert_handler, &parent_block_idx); //这里必然返回nil，除非bug了
    //挂载到插入点，上面del node过程已经保证了这时候left=right=nil，且size=1，type也没变，treap_weight没必要变，只要建立连接、调整size即可
    *insert_handler = block_idx;
    node->parent = parent_block_idx;
    adjust_size_after_insert(block_idx);
    //旋转保持平衡
    rotate_inserted_block_node(block_idx, &db_info->db_map);

    set_modify_time();

    return RET_SUCCESS;
}

RetCode DbImp::kv_scan(uint32_t idx, uint32_t need_count, std::vector<std::string> &keys)
{
    uint32_t block_idx = locate_in_db_map(idx);
    if (block_idx == BLOCK_IDX_NIL)
    {
        return RET_OUT_OF_RANGE;
    }

    //遍历获取key
    keys.clear();
    while (keys.size() < need_count)
    {
        TreeNode *node = get_tree_node(block_idx);
        Kv *kv = (Kv *)node->data;
        size_t key_start, value_start;
        kv->get_offset(key_start, value_start);

        std::string s;
        RetCode ret = get_db_str_sub_str(kv->data + key_start, kv->key_len, 0, -1, s);
        if (ret != RET_SUCCESS)
        {
            return ret;
        }
        keys.push_back(s);

        //继续下一个kv
        block_idx = get_block_node_succ(block_idx);
        if (block_idx == BLOCK_IDX_NIL)
        {
            return RET_SUCCESS;
        }
    }

    return RET_SUCCESS;
}

RetCode DbImp::str_len(const char *key, uint32_t key_len, uint32_t &value_len)
{
    uint32_t block_idx = find_in_db_map((const uint8_t *)key, key_len, NULL, NULL);
    if (block_idx == BLOCK_IDX_NIL)
    {
        return RET_KEY_NOT_EXIST;
    }

    TreeNode *node = get_tree_node(block_idx);
    Kv *kv = (Kv *)node->data;
    size_t key_start, value_start;
    kv->get_offset(key_start, value_start);

    if (kv->value_len != TREE_OBJ_TAG)
    {
        //短str
        value_len = kv->value_len;
        return RET_SUCCESS;
    }

    //外部对象，先检查类型
    uint32_t obj_root_block_idx = *(volatile const uint32_t *)(kv->data + value_start);
    TreeNode *obj_root_node = get_tree_node(obj_root_block_idx);
    if (obj_root_node->type != TREE_NODE_TYPE_STR)
    {
        //非str
        return RET_TYPE_ERR;
    }
    value_len = obj_root_node->size;

    return RET_SUCCESS;
}

RetCode DbImp::str_set(const char *key, uint32_t key_len, const char *value, uint32_t value_len)
{
    volatile uint32_t *insert_handler;
    uint32_t parent_block_idx;
    uint32_t block_idx = find_in_db_map((const uint8_t *)key, key_len, &insert_handler, &parent_block_idx);
    if (block_idx == BLOCK_IDX_NIL)
    {
        //不存在，新增一个
        block_idx = alloc_block();
        if (block_idx == BLOCK_IDX_NIL)
        {
            return RET_NOT_ENOUGH_SPACE;
        }
        TreeNode *node = get_tree_node(block_idx);
        node->init_empty_node(TREE_NODE_TYPE_DB_MAP, parent_block_idx);
        node->size = 1;
        Kv *kv = (Kv *)node->data;
        kv->init_str_value(db_info->block_size, key_len, value_len);
        size_t key_start, value_start;
        kv->get_offset(key_start, value_start);
        if (kv->key_len == TREE_OBJ_TAG)
        {
            //key需要外部链接
            RetCode ret = new_long_str((const uint8_t *)key, key_len, block_idx, *(volatile uint32_t *)(kv->data + key_start));
            if (ret != RET_SUCCESS)
            {
                free_block_node(block_idx);
                return ret;
            }
        }
        else
        {
            //直接存key
            memcpy((void *)(kv->data + key_start), key, key_len);
        }
        if (kv->value_len == TREE_OBJ_TAG)
        {
            //value需要外部链接
            RetCode ret = new_long_str((const uint8_t *)value, value_len, block_idx, *(volatile uint32_t *)(kv->data + value_start));
            if (ret != RET_SUCCESS)
            {
                if (kv->key_len == TREE_OBJ_TAG)
                {
                    free_tree_obj(*(volatile uint32_t *)(kv->data + key_start));
                }
                free_block_node(block_idx);
                return ret;
            }
        }
        else
        {
            //直接存value
            memcpy((void *)(kv->data + value_start), value, value_len);
        }
        //插入block节点、调整size并保持平衡
        *insert_handler = block_idx;
        adjust_size_after_insert(block_idx);
        rotate_inserted_block_node(block_idx, &db_info->db_map);

        set_modify_time();

        return RET_SUCCESS;
    }

    TreeNode *node = get_tree_node(block_idx);
    Kv *kv = (Kv *)node->data;
    size_t key_start, value_start;
    kv->get_offset(key_start, value_start);
    size_t key_cost, value_cost, block_space_total, key_space_max, value_space_max;
    kv->get_space_info(db_info, key_cost, value_cost, block_space_total, key_space_max, value_space_max);

    //由于是最后一个字段，所以不用动key，直接在原处改value就可以了
    //保存一下老的外部value链接，供后面删除用
    uint32_t old_value_tree = kv->value_len == TREE_OBJ_TAG ? *(volatile uint32_t *)(kv->data + value_start) : BLOCK_IDX_NIL;
    if (value_len <= value_space_max)
    {
        //新value可以直接存下
        memcpy((void *)(kv->data + value_start), value, value_len);
        kv->value_len = (uint8_t)value_len;
    }
    else
    {
        //需要外部链接新value
        RetCode ret = new_long_str((const uint8_t *)value, value_len, block_idx, *(volatile uint32_t *)(kv->data + value_start));
        if (ret != RET_SUCCESS)
        {
            return ret;
        }
        kv->value_len = TREE_OBJ_TAG;
    }

    //释放掉老的外部value
    if (old_value_tree != BLOCK_IDX_NIL)
    {
        free_tree_obj(old_value_tree);
    }

    set_modify_time();

    return RET_SUCCESS;
}

RetCode DbImp::str_get(const char *key, uint32_t key_len, std::string &value)
{
    uint32_t block_idx = find_in_db_map((const uint8_t *)key, key_len, NULL, NULL);
    if (block_idx == BLOCK_IDX_NIL)
    {
        return RET_KEY_NOT_EXIST;
    }

    TreeNode *node = get_tree_node(block_idx);
    Kv *kv = (Kv *)node->data;
    size_t key_start, value_start;
    kv->get_offset(key_start, value_start);

    return get_db_str_sub_str(kv->data + value_start, kv->value_len, 0, -1, value);
}

RetCode DbImp::str_append(const char *key, uint32_t key_len, const char *value, uint32_t value_len)
{
    uint32_t block_idx = find_in_db_map((const uint8_t *)key, key_len, NULL, NULL);
    if (block_idx == BLOCK_IDX_NIL)
    {
        //不存在，走str_set流程
        return str_set(key, key_len, value, value_len);
    }

    TreeNode *node = get_tree_node(block_idx);
    Kv *kv = (Kv *)node->data;
    size_t key_start, value_start;
    kv->get_offset(key_start, value_start);
    size_t key_cost, value_cost, block_space_total, key_space_max, value_space_max;
    kv->get_space_info(db_info, key_cost, value_cost, block_space_total, key_space_max, value_space_max);

    if (kv->value_len != TREE_OBJ_TAG)
    {
        //直接存的短串，若能原地append就直接做
        if ((size_t)kv->value_len + value_len <= value_space_max)
        {
            memcpy((void *)(kv->data + value_start + kv->value_len), value, value_len);
            kv->value_len = (uint8_t)(kv->value_len + value_len);
            set_modify_time();
            return RET_SUCCESS;
        }
        //append后会超长，先转化为long str后再做
        volatile uint32_t handler; //临时handler，避免在new_long_str的参数传入中value域同时作为输入和输出
        RetCode ret = new_long_str(const_cast<const uint8_t *>(kv->data + value_start), kv->value_len, block_idx, handler);
        if (ret != RET_SUCCESS)
        {
            return ret;
        }
        kv->value_len = TREE_OBJ_TAG;
        *(volatile uint32_t *)(kv->data + value_start) = handler;
    }
    else
    {
        //本来就是外部对象，检查是否为long str
        uint32_t obj_root_block_idx = *(volatile const uint32_t *)(kv->data + value_start);
        TreeNode *obj_root_node = get_tree_node(obj_root_block_idx);
        if (obj_root_node->type != TREE_NODE_TYPE_STR)
        {
            return RET_TYPE_ERR;
        }
    }

    //对long str做append
    RetCode ret = long_str_append(*(volatile uint32_t *)(kv->data + value_start), (const uint8_t *)value, value_len);
    if (ret != RET_SUCCESS)
    {
        return ret;
    }

    set_modify_time();

    return RET_SUCCESS;
}

RetCode DbImp::str_set_sub_str(const char *key, uint32_t key_len, uint32_t offset, const char *value, uint32_t value_len)
{
    //todo
}

RetCode DbImp::str_get_sub_str(const char *key, uint32_t key_len, int64_t start, int64_t end, std::string &value)
{
    uint32_t block_idx = find_in_db_map((const uint8_t *)key, key_len, NULL, NULL);
    if (block_idx == BLOCK_IDX_NIL)
    {
        return RET_KEY_NOT_EXIST;
    }

    TreeNode *node = get_tree_node(block_idx);
    Kv *kv = (Kv *)node->data;
    size_t key_start, value_start;
    kv->get_offset(key_start, value_start);

    return get_db_str_sub_str(kv->data + value_start, kv->value_len, start, end, value);
}

uint32_t DbImp::find_in_db_map(const uint8_t *key, uint32_t key_len, volatile uint32_t **insert_handler, uint32_t *parent_block_idx)
{
    //简单起见，查找过程中随时更新insert_handler和parent_block_idx，查找成功则重置

    uint32_t block_idx = db_info->db_map;
    if (insert_handler != NULL)
    {
        *insert_handler = &db_info->db_map;
        *parent_block_idx = BLOCK_IDX_NIL;
    }

    while (block_idx != BLOCK_IDX_NIL)
    {
        TreeNode *node = get_tree_node(block_idx);
        Kv *kv = (Kv *)node->data;
        size_t key_start, value_start;
        kv->get_offset(key_start, value_start);

        int cmp_ret = compare_with_db_str(key, key_len, kv->data + key_start, kv->key_len);
        if (cmp_ret == 0)
        {
            //找到，重置insert_handler和parent_block_idx，安全一点
            if (insert_handler != NULL)
            {
                *insert_handler = NULL;
                *parent_block_idx = BLOCK_IDX_NIL;
            }
            return block_idx;
        }
        //到子树中去找
        if (insert_handler != NULL)
        {
            *insert_handler = cmp_ret < 0 ? &node->left : &node->right;
            *parent_block_idx = block_idx;
        }
        block_idx = cmp_ret < 0 ? node->left : node->right;
    }

    return BLOCK_IDX_NIL;
}

uint32_t DbImp::locate_in_db_map(uint32_t idx)
{
    uint32_t block_idx = db_info->db_map;
    while (block_idx != BLOCK_IDX_NIL)
    {
        TreeNode *node = get_tree_node(block_idx);
        uint32_t left_size = node->left == BLOCK_IDX_NIL ? 0 : get_tree_node(node->left)->size;
        if (idx == left_size)
        {
            //当前节点就是
            return block_idx;
        }
        if (idx < left_size)
        {
            //在左子树中，继续查找
            block_idx = node->left;
        }
        else
        {
            //在右子树中，继续查找，需要调整新idx
            block_idx = node->right;
            idx -= left_size + 1;
        }
    }

    return BLOCK_IDX_NIL;
}

int DbImp::compare_with_db_str(const uint8_t *s, uint32_t s_len, volatile const uint8_t *db_s, uint8_t db_s_len)
{
    if (db_s_len == TREE_OBJ_TAG)
    {
        //外部存的长串，迭代遍历串比较
        uint32_t iter_block_idx = get_first_block_node(*(volatile const uint32_t *)db_s);
        uint32_t pos = 0;
        for (;;)
        {
            //先获取当前part的str
            volatile const uint8_t *str_part;
            uint8_t str_part_len;
            get_long_str_part(iter_block_idx, str_part, str_part_len);

            //对本part的str开始比对，自然跳过空part
            for (size_t i = 0; i < str_part_len; ++ i)
            {
                if (pos >= s_len)
                {
                    //s已经比完，但是db_s还有数据，返回小于
                    return -1;
                }
                if (s[pos] < str_part[i])
                {
                    return -1;
                }
                if (s[pos] > str_part[i])
                {
                    return 1;
                }
                ++ pos;
            }

            //继续下一个part
            iter_block_idx = get_block_node_succ(iter_block_idx);
            if (iter_block_idx == BLOCK_IDX_NIL)
            {
                //db_s已经比完，检查s是否还有数据，有则返回大于，否则等于
                return pos < s_len ? 1 : 0;
            }
        }
    }

    //直接存的短串
    int ret = memcmp(s, (const void *)db_s, s_len < db_s_len ? s_len : db_s_len);
    if (ret != 0)
    {
        return ret;
    }
    if (s_len < db_s_len)
    {
        return -1;
    }
    if (s_len > db_s_len)
    {
        return 1;
    }
    return 0;
}

static void adjust_sub_str_start_end_pos(uint32_t s_len, int64_t &start, int64_t &end, uint32_t &ss_len)
{
    //计算负索引
    if (start < 0)
    {
        start += s_len;
    }
    if (end < 0)
    {
        end += s_len;
    }

    //调整非法值
    if (start < 0)
    {
        start = 0;
    }
    if (end >= s_len)
    {
        end = s_len - 1;
    }

    ss_len = start > end ? 0 : (uint32_t)(end - start + 1);
}

RetCode DbImp::get_db_str_sub_str(volatile const uint8_t *db_s, uint8_t db_s_len, int64_t start, int64_t end, std::string &s)
{
    s.clear();

    if (db_s_len == TREE_OBJ_TAG)
    {
        //外部对象，先检查类型
        uint32_t obj_root_block_idx = *(volatile const uint32_t *)db_s;
        TreeNode *obj_root_node = get_tree_node(obj_root_block_idx);
        if (obj_root_node->type != TREE_NODE_TYPE_STR)
        {
            //非str
            return RET_TYPE_ERR;
        }

        uint32_t ss_len;
        adjust_sub_str_start_end_pos(obj_root_node->size, start, end, ss_len);

        uint32_t iter_block_idx = long_str_locate_sub_str_by_start_idx(obj_root_block_idx, start);
        while (iter_block_idx != BLOCK_IDX_NIL && s.size() < ss_len)
        {
            size_t left_len = ss_len - s.size();

            volatile const uint8_t *str_part;
            uint8_t str_part_len;
            get_long_str_part(iter_block_idx, str_part, str_part_len);

            //计算本part中需要拷贝的长度
            size_t copy_len = str_part_len - (size_t)start;
            if (copy_len > left_len)
            {
                copy_len = left_len;
            }

            s.append((const char *)(str_part + start), copy_len);

            //继续下一个part
            iter_block_idx = get_block_node_succ(iter_block_idx);
            start = 0;
        }

        return RET_SUCCESS;
    }

    //直接存的短串
    uint32_t ss_len;
    adjust_sub_str_start_end_pos(db_s_len, start, end, ss_len);
    if (ss_len > 0)
    {
        s.assign((const char *)db_s + start, (size_t)ss_len);
    }

    return RET_SUCCESS;
}

}
