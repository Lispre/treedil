#include <time.h>
#include <math.h>

#include "treedil_util.h"

namespace treedil
{

//BKDR hash算法的改进版
uint64_t str_hash(const uint8_t *s, size_t len)
{
    uint64_t h = 1;
    for (size_t i = 0; i < len; ++ i)
    {
        h = (h + s[i]) * 1000003ULL;
    }
    return h;
}

//Wichman-Hill的随机数算法
double wh_random()
{
    static bool rand_seed_inited = false;
    static uint64_t rand_seed_x, rand_seed_y, rand_seed_z;

    if (!rand_seed_inited)
    {
        //第一次调用初始化种子
        uint64_t now = (uint64_t)time(NULL);
        uint64_t a = now * now;

        rand_seed_x = a % 30268;
        a /= 30268;
        rand_seed_y = a % 30306;
        a /= 30306;
        rand_seed_z = a % 30322;
        a /= 30322;
        ++ rand_seed_x;
        ++ rand_seed_y;
        ++ rand_seed_z;

        rand_seed_inited = true;
    }

    rand_seed_x = rand_seed_x * 171 % 30269;
    rand_seed_y = rand_seed_y * 172 % 30307;
    rand_seed_z = rand_seed_z * 170 % 30323;

    return fmod((double)rand_seed_x / 30269.0 + (double)rand_seed_y / 30307.0 + (double)rand_seed_z / 30323.0, 1.0);
}

}
