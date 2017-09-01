#include <string.h>
#include <time.h>

#include "treedil.h"

#include "treedil_util.h"
#include "treedil_db.h"

namespace treedil
{

DbImp *DbImp::create_db(uint8_t *space, size_t space_size, uint8_t block_size, uint32_t block_count)
{
    //�������ĺϷ���
    if (space == NULL || block_size < BLOCK_SIZE_MIN || block_count < BLOCK_COUNT_MIN ||
        space_size != DB_INFO_SPACE_SIZE + (size_t)block_size * block_count)
    {
        return NULL;
    }

    //�����ӿڶ��󲢳�ʼ������
    DbImp *db_imp = new DbImp();

    db_imp->db_info = (DbInfo *)space;
    db_imp->block_start = space + DB_INFO_SPACE_SIZE;

    //��ʼ��info
    DbInfo *db_info = db_imp->db_info;

    //�Ƚ�info������
    memset(db_info, 0, DB_INFO_SPACE_SIZE);

    //���þ�̬��Ϣ���ֶ�
    db_info->magic = DB_INFO_MAGIC;
    db_info->create_time = (uint32_t)time(NULL);
    for (size_t i = 0; i < sizeof(db_info->serial_no); ++ i)
    {
        db_info->serial_no[i] = (uint8_t)(wh_random() * 256);
    }
    db_info->block_size = block_size;

    //����check_sum
    db_info->set_check_sum();

    //��ʼ����̬��Ϣ�ֶ�
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
    //�������Ϸ���
    if (space == NULL || space_size < DB_INFO_SPACE_SIZE + (size_t)BLOCK_SIZE_MIN * BLOCK_COUNT_MIN)
    {
        return NULL;
    }

    //�����ӿڶ���У��db��Ϣ
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
