#include "koopa.h"
#include <cassert>
#include <iostream>
#include <string>
#include <unordered_map>
#include <string.h>
using namespace std;

extern string riscv_ret_str;

void Visit(const koopa_raw_program_t &program);
void Visit(const koopa_raw_slice_t &slice);
void Visit(const koopa_raw_function_t &func);
void Visit(const koopa_raw_basic_block_t &bb);
void Visit(const koopa_raw_value_t &value);

void Visit(const koopa_raw_return_t &ret);
void Visit(const koopa_raw_integer_t &integer);
void Visit(const koopa_raw_binary_t &binary, const koopa_raw_value_t &value);
void Visit(const koopa_raw_load_t &load, const koopa_raw_value_t &value);
void Visit(const koopa_raw_store_t &store);
void Visit(const koopa_raw_branch_t &branch);
void Visit(const koopa_raw_jump_t &jump);
void Visit(const koopa_raw_call_t &call, const koopa_raw_value_t &value);
void Visit(const koopa_raw_get_elem_ptr_t &get_elem_ptr, const koopa_raw_value_t &value);
void Visit(const koopa_raw_get_ptr_t &get_ptr, const koopa_raw_value_t &value);

void Visit(const koopa_raw_global_alloc_t &global_alloc, const koopa_raw_value_t &value);

class VarTable
{
    unordered_map<koopa_raw_value_t, int> table;

public:
    void insert(const koopa_raw_value_t &value, int addr);
    bool exist(const koopa_raw_value_t &value);
    int get(const koopa_raw_value_t &value);
};