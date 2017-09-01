#include <string.h>
#include <time.h>

#include "treedil.h"

#include "treedil_db.h"

namespace treedil
{

Db *create_db(char *space, size_t space_size, uint8_t block_size, uint32_t block_count)
{
    return DbImp::create_db((uint8_t *)space, space_size, block_size, block_count);
}

Db *attach_db(char *space, size_t space_size)
{
    return DbImp::attach_db((uint8_t *)space, space_size);
}

int detach_db(Db *&db)
{
    if (db == NULL)
    {
        return 0;
    }

    DbImp *db_imp = dynamic_cast<DbImp *>(db);
    if (db_imp == NULL)
    {
        //可能是有人另外实现了Db接口
        return -1;
    }

    db_imp->detach();
    db = NULL;

    return 0;
}

}
