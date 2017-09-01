#ifndef TREEDIL_TREE_H
#define TREEDIL_TREE_H

#include <stdint.h>

#include "treedil_util.h"

namespace treedil
{

static const uint32_t BLOCK_IDX_NIL = (uint32_t)-1; //����block������nilֵ�����������ܴﵽ���ֵ

enum TreeNodeType
{
    TREE_NODE_TYPE_FREE = 1,
    TREE_NODE_TYPE_DB_MAP = 2,
    TREE_NODE_TYPE_STR = 3,
    TREE_NODE_TYPE_LIST = 4,
    TREE_NODE_TYPE_MAP = 5,
};

#pragma pack(push, 1)

//tree�ڵ�ṹ��λ�ڸ���block�Ŀ�ͷ
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

    volatile uint8_t type; //���TreeNodeType���͵�ֵ
    volatile uint32_t parent; //��ָ��
    volatile uint32_t left; //����ָ��
    volatile uint32_t right; //�Һ���ָ��
    volatile uint32_t size; //���ڵ��µ�Ԫ��������ע���ڲ�ͬtype�¿��ܺ���������
    volatile uint32_t treap_weight; //Ŀǰ��treapʵ�֣��洢һ�������Ȩ��

    //���ݿ�ʼ��TreeNode�����ÿ��block�У�ʣ��ռ������������
    //�������Ż�����ʵ�����ͷ�Ѿ��ܴ��ˣ����ڹ������������ɸĳ�ѹ��������Ż����޸�����ʵ�ַ�ʽ�������ú�����Ļ����Խ�rb��ʶ����type�ֽڣ�
    volatile uint8_t data[0];
};

#pragma pack(pop)

}

#endif
