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

    //反复找left节点即可
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

    //反复找right节点即可
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
        //有左子树，则前驱就是左子树最后节点
        return get_last_block_node(node->left);
    }

    for (;;)
    {
        if (node->parent == BLOCK_IDX_NIL)
        {
            //到了根了，而且是直属db的树根
            return BLOCK_IDX_NIL;
        }

        TreeNode *parent_node = get_tree_node(node->parent);

        if (parent_node->right == block_idx)
        {
            //是右子树，父节点就是前驱
            return node->parent;
        }

        if (parent_node->left != block_idx)
        {
            //有parent但是不是其子树，属于包含关系，即已经到了结构的根节点
            return BLOCK_IDX_NIL;
        }

        //是左子树，向上迭代
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

    //处理逻辑和get_block_node_prev对称，注释省略

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
    //如果要一边遍历一边释放，则必须走删除节点流程才能保证拿到后继，比较麻烦
    //所以先遍历所有node并对node内容（其中引用的外部对象）进行释放处理，同时记录到列表中，最后做释放处理

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
                //long str的block不包含外部引用，不做处理
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

    //假设这个node的treap_weight无限大，将其转到底部
    for (;;)
    {
        if (node->left == BLOCK_IDX_NIL && node->right == BLOCK_IDX_NIL)
        {
            //到底了
            break;
        }
        if (node->left == BLOCK_IDX_NIL)
        {
            //左转下降一层
            rotate_left(block_idx, root_handler);
        }
        else if (node->right == BLOCK_IDX_NIL)
        {
            //右转下降一层
            rotate_right(block_idx, root_handler);
        }
        else
        {
            //比较二者的weight决定旋转方向
            TreeNode *left_node = get_tree_node(node->left);
            TreeNode *right_node = get_tree_node(node->right);
            left_node->treap_weight < right_node->treap_weight ? rotate_right(block_idx, root_handler) : rotate_left(block_idx, root_handler);
        }
    }

    //已经转到底部了，先向上调整size域
    uint32_t bi = block_idx;
    TreeNode *nd = get_tree_node(bi);
    while (nd->parent != BLOCK_IDX_NIL)
    {
        TreeNode *pnd = get_tree_node(nd->parent);
        if (pnd->left != bi && pnd->right != bi)
        {
            //到顶了
            break;
        }
        pnd->size -= node->size; //路径中节点的size都减少需要删除节点的size

        //向上继续
        bi = nd->parent;
        nd = pnd;
    }

    //再删除节点，但是节点本身需要保留，作为一个自由节点
    volatile uint32_t *handler = get_node_handler(block_idx, root_handler);
    *handler = BLOCK_IDX_NIL;
    node->parent = BLOCK_IDX_NIL;
}

void DbImp::free_block_node(uint32_t block_idx)
{
    //所有调用这个函数的地方应该保证此block_node已经被从所在的tree删除，或正在进行整个tree的删除

    //设置此node为free类型的一个新node
    TreeNode *node = get_tree_node(block_idx);
    node->init_empty_node(TREE_NODE_TYPE_FREE, BLOCK_IDX_NIL);
    node->size = 1; //free tree的size是指子树的block数量

    if (db_info->free_block_tree == BLOCK_IDX_NIL)
    {
        //直接作为free_block_tree的根
        db_info->free_block_tree = block_idx;
        return;
    }

    //按block_idx大小插入free_tree
    uint32_t bi = db_info->free_block_tree;
    for (;;)
    {
        TreeNode *nd = get_tree_node(bi);
        if (bi > block_idx)
        {
            //需要插入到左子树中
            if (nd->left == BLOCK_IDX_NIL)
            {
                //到底了，直接插
                node->parent = bi;
                nd->left = block_idx;
                //跳出去，还有统一的后续处理
                break;
            }
            bi = nd->left;
        }
        else
        {
            //需要插入到右子树中，代码和上面对称，注释省略
            if (nd->right == BLOCK_IDX_NIL)
            {
                node->parent = bi;
                nd->right = block_idx;
                break;
            }
            bi = nd->right;
        }
    }

    //调整size并向上旋转到适当位置
    adjust_size_after_insert(block_idx);
    rotate_inserted_block_node(block_idx, &db_info->free_block_tree);

    //调整block使用情况
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
        //到顶了，用root_handler
        return root_handler;
    }

    TreeNode *parent_node = get_tree_node(node->parent);
    if (parent_node->left == block_idx)
    {
        //是左子树
        return &parent_node->left;
    }
    if (parent_node->right == block_idx)
    {
        //是右子树
        return &parent_node->right;
    }

    //包含关系，到顶了
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
            //到顶了
            return;
        }
        pnd->size += node->size; //路径中节点的size都增加新插入节点的size

        //向上继续
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
            //到顶了
            return;
        }

        TreeNode *pnd = get_tree_node(nd->parent);
        if (pnd->left != bi && pnd->right != bi)
        {
            //到顶了
            return;
        }

        //向上继续
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
            //到顶了
            return;
        }
        TreeNode *parent_node = get_tree_node(node->parent);
        if (parent_node->treap_weight <= node->treap_weight)
        {
            //没必要再向上了
            return;
        }
        if (parent_node->left == block_idx)
        {
            //左子树，右转
            rotate_right(node->parent, root_handler);
        }
        else if (parent_node->right == block_idx)
        {
            //右子树，左转
            rotate_left(node->parent, root_handler);
        }
        else
        {
            //非子树，包含关系，也是到顶了
            return;
        }
    }
}

void DbImp::rotate_left(uint32_t block_idx, volatile uint32_t *root_handler)
{
    //以block_idx为子树的根，做左转，调用者需保证右孩子不为空

    TreeNode *node = get_tree_node(block_idx);

    volatile uint32_t *handler = get_node_handler(block_idx, root_handler); //先拿到node的handler
    uint32_t handler_idx = node->parent; //保存一下handler所在的block索引，也就是node的父节点

    uint32_t right_idx = node->right; //保存一下右子树的节点索引
    TreeNode *right_node = get_tree_node(right_idx); //获取右子树的节点
    uint32_t right_left_idx = right_node->left; //保存一下右子树的左子树节点索引
    TreeNode *right_left_node = right_left_idx == BLOCK_IDX_NIL ? NULL : get_tree_node(right_left_idx); //获取右子树的左子树节点，若有的话

    //建立handler和右子树节点的链接
    *handler = right_idx;
    right_node->parent = handler_idx;

    //重构node和右子树节点的链接
    right_node->left = block_idx;
    node->parent = right_idx;

    //把右子树的左子树节点挂载到node的右子树
    node->right = right_left_idx;
    if (right_left_node != NULL)
    {
        right_left_node->parent = block_idx;
    }

    //旋转也要调整size
    uint32_t node_non_right_size = node->size - right_node->size; //node本身和左子树的size和
    right_node->size = node->size; //右子树节点成为新子树根，直接管辖原子树所有节点，所以直接赋值
    //原子树根node变成了新的左子树，保留自己左子树、自身的size，并增加新挂载的右子树size
    node->size = node_non_right_size + (right_left_node == NULL ? 0 : right_left_node->size);
    //right_left_node只是做了个迁移，没有影响子树结构，不做调整
}

void DbImp::rotate_right(uint32_t block_idx, volatile uint32_t *root_handler)
{
    //以block_idx为子树的根，做右转，调用者需保证左孩子不为空

    //过程和rotate_left对称，省略注释

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
    //优先从free tree中申请
    if (db_info->free_block_tree != BLOCK_IDX_NIL)
    {
        //申请最靠前的节点，从free tree摘除，调整使用信息并返回
        uint32_t block_idx = get_first_block_node(db_info->free_block_tree);
        del_block_node(block_idx, &db_info->free_block_tree);
        ++ db_info->used_block_count;
        return block_idx;
    }

    //free tree为nil，从reached指针位置申请，并向后扩展reached
    if (db_info->reached_block_count < db_info->block_count)
    {
        uint32_t block_idx = db_info->reached_block_count;
        ++ db_info->reached_block_count;
        ++ db_info->used_block_count;
        return block_idx;
    }

    //没空间了
    return BLOCK_IDX_NIL;
}

}
