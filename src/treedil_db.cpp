#include <string.h>
#include <time.h>

#include "treedil.h"

#include "treedil_util.h"
#include "treedil_db.h"

namespace treedil
{

DbImp *DbImp::create_db(uint8_t *space, size_t space_size, uint8_t block_size, uint32_t block_count)
{
    //检查参数的合法性
    if (space == NULL || block_size < BLOCK_SIZE_MIN || block_count < BLOCK_COUNT_MIN ||
        space_size != DB_INFO_SPACE_SIZE + (size_t)block_size * block_count)
    {
        return NULL;
    }

    //创建接口对象并初始化对象
    DbImp *db_imp = new DbImp();

    db_imp->db_info = (DbInfo *)space;
    db_imp->block_start = space + DB_INFO_SPACE_SIZE;

    //初始化info
    DbInfo *db_info = db_imp->db_info;

    //先将info区清零
    memset(db_info, 0, DB_INFO_SPACE_SIZE);

    //设置静态信息的字段
    db_info->magic = DB_INFO_MAGIC;
    db_info->create_time = (uint32_t)time(NULL);
    for (size_t i = 0; i < sizeof(db_info->serial_no); ++ i)
    {
        db_info->serial_no[i] = (uint8_t)(wh_random() * 256);
    }
    db_info->block_size = block_size;

    //计算check_sum
    db_info->set_check_sum();

    //初始化动态信息字段
    db_info->block_count = block_count;
    db_info->modify_time = (uint32_t)time(NULL);
    db_info->used_block_count = 0;
    db_info->reached_block_count = 0;
    db_info->free_block_tree = BLOCK_IDX_NIL;
    db_info->db_map = BLOCK_IDX_NIL;

    return db_imp;
}

DbImp *DbImp::attach_db(uint8_t *space, size_t space_size)
{
    //检查参数合法性
    if (space == NULL || space_size < DB_INFO_SPACE_SIZE + (size_t)BLOCK_SIZE_MIN * BLOCK_COUNT_MIN)
    {
        return NULL;
    }

    //创建接口对象并校验db信息
    DbImp *db_imp = new DbImp();

    db_imp->db_info = (DbInfo *)space;
    db_imp->block_start = space + DB_INFO_SPACE_SIZE;

    if (!db_imp->db_info->is_valid(space_size))
    {
        delete db_imp;
        return NULL;
    }

    return db_imp;
}

void DbImp::detach()
{
    delete this;
}

DbImp::DbImp()
{
    db_info = NULL;
    block_start = NULL;
}

}
