//long str相关操作的实现

#include <string.h>

#include "treedil_db.h"

namespace treedil
{

/*
long str按block size被分裂到各个节点，存在TreeNode的data域中
存放格式为：1字节实际长度+数据
TreeNode的size域存放本节点下的字符串总长
这个存储方式，理论上每个block可以有任意不超过block_size的part，且允许空part，但是目前的实现中除了最后一个block，前面都是填满的
是否支持间断存储不同长度的part，以及是否允许空part则看以后需求而定，在现在的实现中先尽量兼容这些情况
*/

void DbImp::get_long_str_part(uint32_t block_idx, volatile const uint8_t *&s, uint8_t &s_len)
{
    TreeNode *node = get_tree_node(block_idx);
    s = node->data + 1;
    s_len = node->data[0];
}

RetCode DbImp::new_long_str(const uint8_t *s, uint32_t s_len, uint32_t parent_block_idx, volatile uint32_t &handler)
{
    //先计算需要多少个block
    uint32_t str_part_max_len = (uint32_t)(db_info->block_size - sizeof(TreeNode) - 1);
    uint32_t need_block_count = s_len / str_part_max_len + (s_len % str_part_max_len == 0 ? 0 : 1);
    if (need_block_count == 0)
    {
        //空串也需要至少一个block
        need_block_count = 1;
    }

    if (need_block_count > get_free_block_count())
    {
        //空间不足
        return RET_NOT_ENOUGH_SPACE;
    }

    //连续申请节点并拷贝内容、构建long str tree
    volatile uint32_t tmp_handler = BLOCK_IDX_NIL; //初始化tree的临时handler
    uint32_t parent_of_next_node = parent_block_idx; //下一个节点的parent
    volatile uint32_t *insert_pos = &tmp_handler; //插入点
    for (uint32_t i = 0; i < need_block_count; ++ i)
    {
        uint32_t block_idx = alloc_block(); //上面判断过空间，必然不会nil
        TreeNode *node = get_tree_node(block_idx);
        node->init_empty_node(TREE_NODE_TYPE_STR, parent_of_next_node);

        //确定拷贝的位置和长度
        uint32_t start_pos = i * str_part_max_len; //根据上面的计算，start_pos不可能大于s_len
        uint32_t copy_len = s_len - start_pos;
        if (copy_len > str_part_max_len)
        {
            copy_len = str_part_max_len;
        }

        //拷贝数据
        node->data[0] = (uint8_t)copy_len;
        memcpy((void *)(node->data + 1), s + start_pos, copy_len);
        node->size = copy_len;

        //插入节点、调整size并保持平衡
        *insert_pos = block_idx;
        adjust_size_after_insert(block_idx);
        rotate_inserted_block_node(block_idx, &tmp_handler);

        //继续下一段part，总是插到最后
        parent_of_next_node = block_idx;
        insert_pos = &node->right;
    }
    handler = tmp_handler; //最后再修改handler，防止handler内容本身也作为s输入的情况

    return RET_SUCCESS;
}

uint32_t DbImp::long_str_locate_sub_str_by_start_idx(uint32_t block_idx, int64_t &start)
{
    while (block_idx != BLOCK_IDX_NIL)
    {
        //获取左右子树的size，并计算当前节点的字符串长度size
        TreeNode *node = get_tree_node(block_idx);
        uint32_t left_size = node->left == BLOCK_IDX_NIL ? 0 : get_tree_node(node->left)->size;
        uint32_t right_size = node->right == BLOCK_IDX_NIL ? 0 : get_tree_node(node->right)->size;
        uint32_t block_str_size = node->size - left_size - right_size;

        if (start < left_size)
        {
            //在左子树中，继续查找，start不变
            block_idx = node->left;
        }
        else if (start < left_size + block_str_size)
        {
            //就在当前节点中，调整start后返回
            start -= left_size;
            return block_idx;
        }
        else
        {
            //在右子树中，调整start后继续查找
            start -= left_size + block_str_size;
            block_idx = node->right;
        }
    }

    return BLOCK_IDX_NIL;
}

RetCode DbImp::long_str_append(volatile uint32_t &handler, const uint8_t *s, uint32_t s_len)
{
    //先计算需要多少个block
    uint32_t str_part_max_len = (uint32_t)(db_info->block_size - sizeof(TreeNode) - 1);
    uint32_t need_block_count = s_len / str_part_max_len + (s_len % str_part_max_len == 0 ? 0 : 1);

    if (need_block_count > get_free_block_count())
    {
        //空间不足
        return RET_NOT_ENOUGH_SPACE;
    }

    uint32_t left_len = s_len;

    //找到最后一个block
    uint32_t last_block_idx = get_last_block_node(handler);
    TreeNode *last_node = get_tree_node(last_block_idx);
    if (last_node->data[0] < str_part_max_len)
    {
        //这个block还有剩余空间，先append一部分
        uint32_t copy_len = str_part_max_len - last_node->data[0];
        if (copy_len > s_len)
        {
            copy_len = s_len;
        }
        memcpy((void *)(last_node->data + 1 + last_node->data[0]), s, copy_len);
        last_node->data[0] = (uint8_t)(last_node->data[0] + copy_len);
        adjust_size(last_block_idx, copy_len);

        left_len -= copy_len;
    }

    //追加block，拷贝剩余的s
    volatile uint32_t *insert_pos = &last_node->right; //插入点
    while (left_len > 0)
    {
        uint32_t block_idx = alloc_block(); //上面判断过空间，必然不会nil
        TreeNode *node = get_tree_node(block_idx);
        node->init_empty_node(TREE_NODE_TYPE_STR, last_block_idx);

        //确定拷贝的位置和长度
        uint32_t start_pos = s_len - left_len;
        uint32_t copy_len = left_len < str_part_max_len ? left_len : str_part_max_len;

        //拷贝数据
        node->data[0] = (uint8_t)copy_len;
        memcpy((void *)(node->data + 1), s + start_pos, copy_len);
        node->size = copy_len;

        //插入节点、调整size并保持平衡
        *insert_pos = block_idx;
        adjust_size_after_insert(block_idx);
        rotate_inserted_block_node(block_idx, &handler);

        //继续下一段part，总是插到最后
        last_block_idx = block_idx;
        insert_pos = &node->right;

        left_len -= copy_len;
    }

    return RET_SUCCESS;
}

}
