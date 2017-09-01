//一些通用的函数或功能

#ifndef TREEDIL_UTIL_H
#define TREEDIL_UTIL_H

#include <stdint.h>

namespace treedil
{

//计算字符串hash值
uint64_t str_hash(const uint8_t *s, size_t len);

//生成一个在范围[0, 1)的随机浮点数
double wh_random();

}

#endif
