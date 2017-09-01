#include <time.h>
#include <math.h>

#include <vector>

#include "treedil_db.h"

namespace treedil
{

uint32_t DbImp::get_first_block_node(uint32_t block_idx)
{
    if (block_idx == BLOCK_IDX_NIL)
    {
        return BLOCK_IDX_NIL;
    }

    //������left�ڵ㼴��
    for (;;)
    {
        TreeNode *node = get_tree_node(block_idx);
        if (node->left == BLOCK_IDX_NIL)
        {
            return block_idx;
        }
        block_idx = node->left;
    }
}

uint32_t DbImp::get_last_block_node(uint32_t block_idx)
{
    if (block_idx == BLOCK_IDX_NIL)
    {
        return BLOCK_IDX_NIL;
    }

    //������right�ڵ㼴��
    for (;;)
    {
        TreeNode *node = get_tree_node(block_idx);
        if (node->right == BLOCK_IDX_NIL)
        {
            return block_idx;
        }
        block_idx = node->right;
    }
}

uint32_t DbImp::get_block_node_prev(uint32_t block_idx)
{
    if (block_idx == BLOCK_IDX_NIL)
    {
        return BLOCK_IDX_NIL;
    }

    TreeNode *node = get_tree_node(block_idx);
    if (node->left != BLOCK_IDX_NIL)
    {
        //������������ǰ���������������ڵ�
        return get_last_block_node(node->left);
    }

    for (;;)
    {
        if (node->parent == BLOCK_IDX_NIL)
        {
            //���˸��ˣ�������ֱ��db������
            return BLOCK_IDX_NIL;
        }

        TreeNode *parent_node = get_tree_node(node->parent);

        if (parent_node->right == block_idx)
        {
            //�������������ڵ����ǰ��
            return node->parent;
        }

        if (parent_node->left != block_idx)
        {
            //��parent���ǲ��������������ڰ�����ϵ�����Ѿ����˽ṹ�ĸ��ڵ�
            return BLOCK_IDX_NIL;
        }

        //�������������ϵ���
        block_idx = node->parent;
        node = parent_node;
    }
}

uint32_t DbImp::get_block_node_succ(uint32_t block_idx)
{
    if (block_idx == BLOCK_IDX_NIL)
    {
        return BLOCK_IDX_NIL;
    }

    //�����߼���get_block_node_prev�Գƣ�ע��ʡ��

    TreeNode *node = get_tree_node(block_idx);
    if (node->right != BLOCK_IDX_NIL)
    {
        return get_first_block_node(node->right);
    }

    for (;;)
    {
        if (node->parent == BLOCK_IDX_NIL)
        {
            return BLOCK_IDX_NIL;
        }

        TreeNode *parent_node = get_tree_node(node->parent);

        if (parent_node->left == block_idx)
        {
            return node->parent;
        }

        if (parent_node->right != block_idx)
        {
            return BLOCK_IDX_NIL;
        }

        block_idx = node->parent;
        node = parent_node;
    }
}

void DbImp::free_tree_obj(uint32_t block_idx)
{
    //���Ҫһ�߱���һ���ͷţ��������ɾ���ڵ����̲��ܱ�֤�õ���̣��Ƚ��鷳
    //�����ȱ�������node����node���ݣ��������õ��ⲿ���󣩽����ͷŴ���ͬʱ��¼���б��У�������ͷŴ���

    std::vector<uint32_t> block_idx_vec;

    for (uint32_t iter_block_idx = get_first_block_node(block_idx); iter_block_idx != BLOCK_IDX_NIL;
         iter_block_idx = get_block_node_succ(iter_block_idx))
    {
        block_idx_vec.push_back(iter_block_idx);

        TreeNode *node = get_tree_node(iter_block_idx);
        switch(node->type)
        {
            case TREE_NODE_TYPE_STR:
            {
                //long str��block�������ⲿ���ã���������
                break;
            }
            case TREE_NODE_TYPE_LIST:
            {
                //todo
                break;
            }
            case TREE_NODE_TYPE_MAP:
            {
                //todo
                break;
            }
            default:
            {
                //unreachable
                return;
            }
        }
    }

    for (std::vector<uint32_t>::iterator it = block_idx_vec.begin(); it != block_idx_vec.end(); ++ it)
    {
        free_block_node(*it);
    }
}

void DbImp::del_block_node(uint32_t block_idx, volatile uint32_t *root_handler)
{
    TreeNode *node = get_tree_node(block_idx);

    //�������node��treap_weight���޴󣬽���ת���ײ�
    for (;;)
    {
        if (node->left == BLOCK_IDX_NIL && node->right == BLOCK_IDX_NIL)
        {
            //������
            break;
        }
        if (node->left == BLOCK_IDX_NIL)
        {
            //��ת�½�һ��
            rotate_left(block_idx, root_handler);
        }
        else if (node->right == BLOCK_IDX_NIL)
        {
            //��ת�½�һ��
            rotate_right(block_idx, root_handler);
        }
        else
        {
            //�Ƚ϶��ߵ�weight������ת����
            TreeNode *left_node = get_tree_node(node->left);
            TreeNode *right_node = get_tree_node(node->right);
            left_node->treap_weight < right_node->treap_weight ? rotate_right(block_idx, root_handler) : rotate_left(block_idx, root_handler);
        }
    }

    //�Ѿ�ת���ײ��ˣ������ϵ���size��
    uint32_t bi = block_idx;
    TreeNode *nd = get_tree_node(bi);
    while (nd->parent != BLOCK_IDX_NIL)
    {
        TreeNode *pnd = get_tree_node(nd->parent);
        if (pnd->left != bi && pnd->right != bi)
        {
            //������
            break;
        }
        pnd->size -= node->size; //·���нڵ��size��������Ҫɾ���ڵ��size

        //���ϼ���
        bi = nd->parent;
        nd = pnd;
    }

    //��ɾ���ڵ㣬���ǽڵ㱾����Ҫ��������Ϊһ�����ɽڵ�
    volatile uint32_t *handler = get_node_handler(block_idx, root_handler);
    *handler = BLOCK_IDX_NIL;
    node->parent = BLOCK_IDX_NIL;
}

void DbImp::free_block_node(uint32_t block_idx)
{
    //���е�����������ĵط�Ӧ�ñ�֤��block_node�Ѿ��������ڵ�treeɾ���������ڽ�������tree��ɾ��

    //���ô�nodeΪfree���͵�һ����node
    TreeNode *node = get_tree_node(block_idx);
    node->init_empty_node(TREE_NODE_TYPE_FREE, BLOCK_IDX_NIL);
    node->size = 1; //free tree��size��ָ������block����

    if (db_info->free_block_tree == BLOCK_IDX_NIL)
    {
        //ֱ����Ϊfree_block_tree�ĸ�
        db_info->free_block_tree = block_idx;
        return;
    }

    //��block_idx��С����free_tree
    uint32_t bi = db_info->free_block_tree;
    for (;;)
    {
        TreeNode *nd = get_tree_node(bi);
        if (bi > block_idx)
        {
            //��Ҫ���뵽��������
            if (nd->left == BLOCK_IDX_NIL)
            {
                //�����ˣ�ֱ�Ӳ�
                node->parent = bi;
                nd->left = block_idx;
                //����ȥ������ͳһ�ĺ�������
                break;
            }
            bi = nd->left;
        }
        else
        {
            //��Ҫ���뵽�������У����������Գƣ�ע��ʡ��
            if (nd->right == BLOCK_IDX_NIL)
            {
                node->parent = bi;
                nd->right = block_idx;
                break;
            }
            bi = nd->right;
        }
    }

    //����size��������ת���ʵ�λ��
    adjust_size_after_insert(block_idx);
    rotate_inserted_block_node(block_idx, &db_info->free_block_tree);

    //����blockʹ�����
    -- db_info->used_block_count;
}

void DbImp::del_and_free_block_node(uint32_t block_idx, volatile uint32_t *root_handler)
{
    del_block_node(block_idx, root_handler);
    free_block_node(block_idx);
}

volatile uint32_t *DbImp::get_node_handler(uint32_t block_idx, volatile uint32_t *root_handler)
{
    TreeNode *node = get_tree_node(block_idx);

    if (node->parent == BLOCK_IDX_NIL)
    {
        //�����ˣ���root_handler
        return root_handler;
    }

    TreeNode *parent_node = get_tree_node(node->parent);
    if (parent_node->left == block_idx)
    {
        //��������
        return &parent_node->left;
    }
    if (parent_node->right == block_idx)
    {
        //��������
        return &parent_node->right;
    }

    //������ϵ��������
    return root_handler;
}

void DbImp::adjust_size_after_insert(uint32_t block_idx)
{
    TreeNode *node = get_tree_node(block_idx);

    uint32_t bi = block_idx;
    TreeNode *nd = node;
    while (nd->parent != BLOCK_IDX_NIL)
    {
        TreeNode *pnd = get_tree_node(nd->parent);
        if (pnd->left != bi && pnd->right != bi)
        {
            //������
            return;
        }
        pnd->size += node->size; //·���нڵ��size�������²���ڵ��size

        //���ϼ���
        bi = nd->parent;
        nd = pnd;
    }
}

void DbImp::adjust_size(uint32_t block_idx, uint32_t size)
{
    uint32_t bi = block_idx;
    TreeNode *nd = get_tree_node(block_idx);
    for (;;)
    {
        nd->size += size;

        if (nd->parent == BLOCK_IDX_NIL)
        {
            //������
            return;
        }

        TreeNode *pnd = get_tree_node(nd->parent);
        if (pnd->left != bi && pnd->right != bi)
        {
            //������
            return;
        }

        //���ϼ���
        bi = nd->parent;
        nd = pnd;
    }
}

void DbImp::rotate_inserted_block_node(uint32_t block_idx, volatile uint32_t *root_handler)
{
    TreeNode *node = get_tree_node(block_idx);
    for (;;)
    {
        if (node->parent == BLOCK_IDX_NIL)
        {
            //������
            return;
        }
        TreeNode *parent_node = get_tree_node(node->parent);
        if (parent_node->treap_weight <= node->treap_weight)
        {
            //û��Ҫ��������
            return;
        }
        if (parent_node->left == block_idx)
        {
            //����������ת
            rotate_right(node->parent, root_handler);
        }
        else if (parent_node->right == block_idx)
        {
            //����������ת
            rotate_left(node->parent, root_handler);
        }
        else
        {
            //��������������ϵ��Ҳ�ǵ�����
            return;
        }
    }
}

void DbImp::rotate_left(uint32_t block_idx, volatile uint32_t *root_handler)
{
    //��block_idxΪ�����ĸ�������ת���������豣֤�Һ��Ӳ�Ϊ��

    TreeNode *node = get_tree_node(block_idx);

    volatile uint32_t *handler = get_node_handler(block_idx, root_handler); //���õ�node��handler
    uint32_t handler_idx = node->parent; //����һ��handler���ڵ�block������Ҳ����node�ĸ��ڵ�

    uint32_t right_idx = node->right; //����һ���������Ľڵ�����
    TreeNode *right_node = get_tree_node(right_idx); //��ȡ�������Ľڵ�
    uint32_t right_left_idx = right_node->left; //����һ�����������������ڵ�����
    TreeNode *right_left_node = right_left_idx == BLOCK_IDX_NIL ? NULL : get_tree_node(right_left_idx); //��ȡ���������������ڵ㣬���еĻ�

    //����handler���������ڵ������
    *handler = right_idx;
    right_node->parent = handler_idx;

    //�ع�node���������ڵ������
    right_node->left = block_idx;
    node->parent = right_idx;

    //�����������������ڵ���ص�node��������
    node->right = right_left_idx;
    if (right_left_node != NULL)
    {
        right_left_node->parent = block_idx;
    }

    //��תҲҪ����size
    uint32_t node_non_right_size = node->size - right_node->size; //node�������������size��
    right_node->size = node->size; //�������ڵ��Ϊ����������ֱ�ӹ�Ͻԭ�������нڵ㣬����ֱ�Ӹ�ֵ
    //ԭ������node������µ��������������Լ��������������size���������¹��ص�������size
    node->size = node_non_right_size + (right_left_node == NULL ? 0 : right_left_node->size);
    //right_left_nodeֻ�����˸�Ǩ�ƣ�û��Ӱ�������ṹ����������
}

void DbImp::rotate_right(uint32_t block_idx, volatile uint32_t *root_handler)
{
    //��block_idxΪ�����ĸ�������ת���������豣֤���Ӳ�Ϊ��

    //���̺�rotate_left�Գƣ�ʡ��ע��

    TreeNode *node = get_tree_node(block_idx);

    volatile uint32_t *handler = get_node_handler(block_idx, root_handler);
    uint32_t handler_idx = node->parent;

    uint32_t left_idx = node->left;
    TreeNode *left_node = get_tree_node(left_idx);
    uint32_t left_right_idx = left_node->right;
    TreeNode *left_right_node = left_right_idx == BLOCK_IDX_NIL ? NULL : get_tree_node(left_right_idx);

    *handler = left_idx;
    left_node->parent = handler_idx;

    left_node->right = block_idx;
    node->parent = left_idx;

    node->left = left_right_idx;
    if (left_right_node != NULL)
    {
        left_right_node->parent = block_idx;
    }

    uint32_t node_non_left_size = node->size - left_node->size;
    left_node->size = node->size;
    node->size = node_non_left_size + (left_right_node == NULL ? 0 : left_right_node->size);
}

uint32_t DbImp::alloc_block()
{
    //���ȴ�free tree������
    if (db_info->free_block_tree != BLOCK_IDX_NIL)
    {
        //�����ǰ�Ľڵ㣬��free treeժ��������ʹ����Ϣ������
        uint32_t block_idx = get_first_block_node(db_info->free_block_tree);
        del_block_node(block_idx, &db_info->free_block_tree);
        ++ db_info->used_block_count;
        return block_idx;
    }

    //free treeΪnil����reachedָ��λ�����룬�������չreached
    if (db_info->reached_block_count < db_info->block_count)
    {
        uint32_t block_idx = db_info->reached_block_count;
        ++ db_info->reached_block_count;
        ++ db_info->used_block_count;
        return block_idx;
    }

    //û�ռ���
    return BLOCK_IDX_NIL;
}

}
