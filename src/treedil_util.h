//һЩͨ�õĺ�������

#ifndef TREEDIL_UTIL_H
#define TREEDIL_UTIL_H

#include <stdint.h>

namespace treedil
{

//�����ַ���hashֵ
uint64_t str_hash(const uint8_t *s, size_t len);

//����һ���ڷ�Χ[0, 1)�����������
double wh_random();

}

#endif
