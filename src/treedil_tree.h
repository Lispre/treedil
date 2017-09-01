#ifndef TREEDIL_TREE_H
#define TREEDIL_TREE_H

#include <stdint.h>

#include "treedil_util.h"

namespace treedil
{

static const uint32_t BLOCK_IDX_NIL = (uint32_t)-1; //定义block索引的nil值，索引不可能达到这个值

enum TreeNodeType
{
    TREE_NODE_TYPE_FREE = 1,
    TREE_NODE_TYPE_DB_MAP = 2,
    TREE_NODE_TYPE_STR = 3,
    TREE_NODE_TYPE_LIST = 4,
    TREE_NODE_TYPE_MAP = 5,
};

#pragma pack(push, 1)

//tree节点结构，位于各个block的开头
struct TreeNode
{
    void init_empty_node(TreeNodeType type_arg, uint32_t parent_arg)
    {
        type = type_arg;
        parent = parent_arg;
        left = BLOCK_IDX_NIL;
        right = BLOCK_IDX_NIL;
        size = 0;
        treap_weight = (uint32_t)(wh_random() * (uint32_t)-1);
    }

    volatile uint8_t type; //存放TreeNodeType类型的值
    volatile uint32_t parent; //父指针
    volatile uint32_t left; //左孩子指针
    volatile uint32_t right; //右孩子指针
    volatile uint32_t size; //本节点下的元素数量（注意在不同type下可能含义有区别）
    volatile uint32_t treap_weight; //目前用treap实现，存储一个随机的权重

    //数据开始，TreeNode存放在每个block中，剩余空间用来存放数据
    //可做的优化：其实上面的头已经很大了，存在管理开销，后续可改成压缩整数存放或者修改树的实现方式（比如用红黑树的话可以将rb标识放在type字节）
    volatile uint8_t data[0];
};

#pragma pack(pop)

}

#endif
