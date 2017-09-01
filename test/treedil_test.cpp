#include <stdlib.h>

#include <iostream>

#include "treedil.h"

using namespace std;

static const uint8_t BLOCK_SIZE = 100;
static const uint32_t BLOCK_COUNT = 1024 * 1024;

static char space[treedil::DB_INFO_SPACE_SIZE + BLOCK_SIZE * BLOCK_COUNT];

#define CHECK_RET(_code) do {                                                           \
treedil::RetCode ret = _code;                                                           \
if (ret != treedil::RET_SUCCESS) {                                                      \
    cout << "line[" << __LINE__ << "] '" #_code "' failed, ret: " << (int)ret << endl;  \
    exit(1);                                                                            \
}} while (false)

#define ASSERT(_expr) do {                                                  \
if (!(_expr)) {                                                             \
    cout << "line[" << __LINE__ << "] '" #_expr "' assert failed" << endl;  \
    exit(1);                                                                \
}} while (false)

static void clear_db(treedil::Db *db)
{
    cout << "start clear db" << endl;
    for (;;)
    {
        uint32_t count;
        CHECK_RET(db->kv_get_count(count));
        if (count == 0)
        {
            break;
        }

        std::vector<std::string> keys;
        CHECK_RET(db->kv_scan(0, 100, keys));
        for (std::vector<std::string>::iterator it = keys.begin(); it != keys.end(); ++ it)
        {
            CHECK_RET(db->kv_del(*it));
        }
    }
    cout << "clear db ok ------------------------------" << endl;
}

static void test_1()
{
    treedil::Db *db = treedil::create_db(space, sizeof(space), BLOCK_SIZE, BLOCK_COUNT);
    ASSERT(db != NULL);

    std::string k1 = "key_1";
    std::string v1 = "value_1";
    CHECK_RET(db->str_set(k1, v1));
    cout << "k1 set ok" << endl;

    uint32_t v1_len;
    CHECK_RET(db->str_len(k1, v1_len));
    ASSERT(v1_len == v1.size());

    std::string out_v1;
    CHECK_RET(db->str_get(k1, out_v1));
    ASSERT(v1 == out_v1);
    cout << "k1 check ok" << endl;

    std::string k2 = "key_2";
    std::string v2 = "";
    for (size_t i = 0; i < 10 * 1024; ++ i)
    {
        v2.append((size_t)1, (char)('a' + i % 26));
    }
    CHECK_RET(db->str_set(k2, v2));
    cout << "k2 set ok" << endl;

    uint32_t v2_len;
    CHECK_RET(db->str_len(k2, v2_len));
    ASSERT(v2_len == v2.size());

    std::string out_v2;
    CHECK_RET(db->str_get(k2, out_v2));
    ASSERT(v2 == out_v2);
    cout << "k2 check ok" << endl;

    std::string k0 = "key_0";
    std::string v0 = "value_0";
    CHECK_RET(db->str_set(k0, v0));
    cout << "k0 set ok" << endl;

    uint32_t count;
    CHECK_RET(db->kv_get_count(count));
    cout << count << " keys in db" << endl;

    treedil::ObjType type;
    CHECK_RET(db->kv_get_type(k0, type));
    cout << "k0 type: " << (int)type << endl;

    CHECK_RET(db->kv_del(k1));
    cout << "k1 del ok" << endl;
    CHECK_RET(db->kv_del(k2));
    cout << "k2 del ok" << endl;

    CHECK_RET(db->str_set(k1, v1));
    cout << "k1 set ok" << endl;

    std::string k3 = "key_3";
    CHECK_RET(db->kv_move(k0, k3));
    cout << "k0 -> k3 ok" << endl;

    std::string out_v0;
    CHECK_RET(db->str_get(k3, out_v0));
    ASSERT(v0 == out_v0);
    cout << "k3 check ok" << endl;

    std::vector<std::string> keys;
    CHECK_RET(db->kv_scan(0, 100, keys));
    cout << "keys:" << endl;
    for (std::vector<std::string>::iterator it = keys.begin(); it != keys.end(); ++ it)
    {
        cout << *it << endl;
    }
    cout << "---------------" << endl;

    CHECK_RET(db->kv_scan(1, 100, keys));
    cout << "keys from idx 1:" << endl;
    for (std::vector<std::string>::iterator it = keys.begin(); it != keys.end(); ++ it)
    {
        cout << *it << endl;
    }
    cout << "---------------" << endl;

    clear_db(db);

    treedil::detach_db(db);
}

static void test_2()
{
    treedil::Db *db = treedil::attach_db(space, sizeof(space));
    ASSERT(db != NULL);

    std::string k = "k";
    std::string v;
    for (size_t i = 0; i < 1000; ++ i)
    {
        char tmp_v[1024];
        snprintf(tmp_v, sizeof(tmp_v), "value_%04zu", i);
        CHECK_RET(db->str_append(k, std::string(tmp_v)));
        v.append(tmp_v);
    }
    cout << "k append ok" << endl;

    std::string out_v;
    CHECK_RET(db->str_get(k, out_v));
    ASSERT(v == out_v);
    cout << "k check ok" << endl;

    CHECK_RET(db->str_get_sub_str(k, -400, (int64_t)v.size() - 400 + 100, out_v));
    ASSERT(v.substr(v.size() - 400, 101) == out_v);

    std::string tmp_v = "range";
    //CHECK_RET(db->str_set_sub_str(k, 100, tmp_v));
    v.resize(v.size() + 100, '\0');
    v.append(tmp_v);

    CHECK_RET(db->str_set(k, v));

    CHECK_RET(db->str_get(k, out_v));
    ASSERT(v == out_v);
    cout << "k check ok" << endl;

    CHECK_RET(db->str_get_sub_str(k, -(int64_t)tmp_v.size() - 5, -(int64_t)tmp_v.size() + 2, out_v));
    ASSERT(v.substr(v.size() - tmp_v.size() - 5, 8) == out_v);

    clear_db(db);

    treedil::detach_db(db);
}

int main(int argc, char *argv[])
{
    test_1();
    test_2();
}
