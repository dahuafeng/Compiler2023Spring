#include "code_gen.hpp"

koopa_raw_function_t cur_func;

int S, R, A;
int S_;

unordered_map<koopa_raw_binary_op_t, string> op2riscv{
    /// Greater than.
    {KOOPA_RBO_GT, "sgt"},
    /// Less than.
    {KOOPA_RBO_LT, "slt"},
    /// Addition.
    {KOOPA_RBO_ADD, "add"},
    /// Subtraction.
    {KOOPA_RBO_SUB, "sub"},
    /// Multiplication.
    {KOOPA_RBO_MUL, "mul"},
    /// Division.
    {KOOPA_RBO_DIV, "div"},
    /// Modulo.
    {KOOPA_RBO_MOD, "rem"},
    /// Bitwise AND.
    {KOOPA_RBO_AND, "and"},
    /// Bitwise OR.
    {KOOPA_RBO_OR, "or"},
    /// Bitwise XOR.
    {KOOPA_RBO_XOR, "xor"},
    /// Shift left logical.
    {KOOPA_RBO_SHL, "shl"},
    /// Shift right logical.
    {KOOPA_RBO_SHR, "shr"},
    /// Shift right arithmetic.
    {KOOPA_RBO_SAR, "sar"}};

int _cal_size(koopa_raw_type_t ty)
{
    switch (ty->tag)
    {
    case KOOPA_RTT_INT32:
        return 4;
    case KOOPA_RTT_ARRAY:
        return ty->data.array.len * _cal_size(ty->data.array.base);
    case KOOPA_RTT_POINTER:
        return 4;
    default:
        return 0;
    }
}

void VarTable::insert(const koopa_raw_value_t &value, int addr)
{
    cerr << "--!insert " << value << " at " << addr << endl;
    if (table.find(value) == table.end())
        table[value] = addr;
}

bool VarTable::exist(const koopa_raw_value_t &value)
{
    return table.find(value) != table.end();
}

int VarTable::get(const koopa_raw_value_t &value)
{
    cerr << "--!get " << value << endl;
    assert(table.find(value) != table.end());
    return table[value] + A; // 加上为传参预留的空间;
}

VarTable var_table;

void globalArrayInit(const koopa_raw_value_t &init)
{
    if (init->kind.tag == KOOPA_RVT_INTEGER)
    {
        riscv_ret_str += "\t.word ";
        Visit(init->kind.data.integer);
        riscv_ret_str += "\n";
    }
    else
    {
        auto elems = init->kind.data.aggregate.elems;
        for (size_t i = 0; i < elems.len; ++i)
        {
            auto ptr = elems.buffer[i];
            globalArrayInit(reinterpret_cast<koopa_raw_value_t>(ptr));
        }
    }
}

// 访问 raw program
void Visit(const koopa_raw_program_t &program)
{
    // 执行一些其他的必要操作

    // 访问所有全局变量
    Visit(program.values);
    // 访问所有函数
    Visit(program.funcs);
}

// 访问 raw slice
void Visit(const koopa_raw_slice_t &slice)
{
    for (size_t i = 0; i < slice.len; ++i)
    {
        auto ptr = slice.buffer[i];
        // 根据 slice 的 kind 决定将 ptr 视作何种元素
        switch (slice.kind)
        {
        case KOOPA_RSIK_FUNCTION:
            // 访问函数
            Visit(reinterpret_cast<koopa_raw_function_t>(ptr));
            break;
        case KOOPA_RSIK_BASIC_BLOCK:
            // 访问基本块
            Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
            break;
        case KOOPA_RSIK_VALUE:
            // 访问指令/全局变量
            Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
            break;
        default:
            // 我们暂时不会遇到其他内容, 于是不对其做任何处理
            assert(false);
        }
    }
}
// 访问函数
void Visit(const koopa_raw_function_t &func)
{
    if (func->bbs.len == 0)
        return;
    // 执行一些其他的必要操作
    riscv_ret_str += "\t.text\n";
    riscv_ret_str += "\t.globl " + string(func->name + 1) + "\n";
    riscv_ret_str += string(func->name + 1) + ":\n";
    // ...

    S = 0, R = 0, A = 0;
    S_ = 0;
    for (size_t i = 0; i < func->bbs.len; ++i)
    {
        auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
        for (size_t j = 0; j < bb->insts.len; ++j)
        {
            auto inst = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);
            switch (inst->kind.tag)
            {
            case KOOPA_RVT_ALLOC:
                var_table.insert(inst, S);
                S += _cal_size(inst->ty->data.pointer.base);
                break;
            case KOOPA_RVT_CALL:
                R = 4;
                A = max(A, max(0, ((int)inst->kind.data.call.args.len - 8) * 4));
            default:
                int sz = _cal_size(inst->ty);
                if (sz)
                {
                    var_table.insert(inst, S);
                    S += sz;
                }
                break;
            }
        }
    }

    S_ = S + R + A;

    if (S_ % 16)
        S_ = (S_ / 16 + 1) * 16;

    // 生成 prologue;
    if (R)
    {
        riscv_ret_str += "\tsw ra, " + to_string(-4) + "(sp)\n";
    }
    if (S_)
    {
        if (-S_ >= -2048 && -S_ <= 2047)
        {
            riscv_ret_str += "\taddi sp, sp, " + to_string(-S_) + "\n";
        }
        else
        {
            riscv_ret_str += "\tli t0, " + to_string(-S_) + "\n";

            riscv_ret_str += "\tadd sp, sp, t0\n";
        }
    }

    // 访问所有基本块
    Visit(func->bbs);
}

// 访问基本块
void Visit(const koopa_raw_basic_block_t &bb)
{
    // 执行一些其他的必要操作
    if (strcmp(bb->name + 1, "entry"))
        riscv_ret_str += string(bb->name + 1) + ":\n";
    // 访问所有指令
    Visit(bb->insts);
}

// 访问指令
void Visit(const koopa_raw_value_t &value)
{
    // 根据指令类型判断后续需要如何访问
    const auto &kind = value->kind;
    switch (kind.tag)
    {
    case KOOPA_RVT_RETURN:
        // 访问 return 指令
        Visit(kind.data.ret);
        break;
    case KOOPA_RVT_INTEGER:
        // 访问 integer 指令
        Visit(kind.data.integer);
        break;
    case KOOPA_RVT_ALLOC:
        // 访问 alloc 指令
        break;
    case KOOPA_RVT_GLOBAL_ALLOC:
        // 访问 global alloc 指令
        Visit(kind.data.global_alloc, value);
        break;
    case KOOPA_RVT_LOAD:
        // 访问 load 指令
        Visit(kind.data.load, value);
        break;
    case KOOPA_RVT_STORE:
        // 访问 store 指令
        Visit(kind.data.store);
        break;
    case KOOPA_RVT_BINARY:
        // 访问 binary 指令
        Visit(kind.data.binary, value);
        break;
    case KOOPA_RVT_BRANCH:
        // 访问 branch 指令
        Visit(kind.data.branch);
        break;
    case KOOPA_RVT_JUMP:
        // 访问 jump 指令
        Visit(kind.data.jump);
        break;
    case KOOPA_RVT_CALL:
        // 访问 call 指令
        Visit(kind.data.call, value);
        break;
    case KOOPA_RVT_GET_ELEM_PTR:
        // 访问 getelemptr 指令
        Visit(kind.data.get_elem_ptr, value);
        break;
    case KOOPA_RVT_GET_PTR:
        // 访问 getptr 指令
        Visit(kind.data.get_ptr, value);
        break;
    default:
        // 其他类型暂时遇不到
        assert(false);
    }
}

// 访问对应类型指令的函数定义略
// 视需求自行实现
// ...
void Visit(const koopa_raw_return_t &ret)
{
    if (ret.value)
    {
        koopa_raw_value_t ret_value = ret.value;
        if (ret_value->kind.tag == KOOPA_RVT_INTEGER)
        {
            riscv_ret_str += "\tli a0, ";
            Visit(ret_value->kind.data.integer);
            riscv_ret_str += "\n";
        }
        else if (ret.value->kind.tag == KOOPA_RVT_BINARY || ret.value->kind.tag == KOOPA_RVT_LOAD || ret.value->kind.tag == KOOPA_RVT_CALL)
        {
            if (var_table.get(ret_value) <= 2047 && var_table.get(ret_value) >= -2048)
                riscv_ret_str += "\tlw a0, " + to_string(var_table.get(ret_value)) + "(sp)\n";
            else
            {
                riscv_ret_str += "\tli t6, " + to_string(var_table.get(ret_value)) + "\n";
                riscv_ret_str += "\tadd t6, t6, sp\n";
                riscv_ret_str += "\tlw a0, (t6)\n";
            }
        }
        else
            assert(0);
    }

    if (S_)
    {
        if (S_ >= -2048 && S_ <= 2047)
        {
            riscv_ret_str += "\taddi sp, sp, " + to_string(S_) + "\n";
        }
        else
        {
            riscv_ret_str += "\tli t0, " + to_string(S_) + "\n";

            riscv_ret_str += "\tadd sp, sp, t0\n";
        }
    }
    if (R)
    {
        riscv_ret_str += "\tlw ra, " + to_string(-4) + "(sp)\n";
    }
    riscv_ret_str += "\tret\n\n";
}

void Visit(const koopa_raw_integer_t &integer)
{
    riscv_ret_str += to_string(integer.value);
    cerr << "--! visit int" << integer.value << endl;
}

void Visit(const koopa_raw_global_alloc_t &global_alloc, const koopa_raw_value_t &value)
{
    riscv_ret_str += "\t.data\n";
    riscv_ret_str += "\t.globl " + string(value->name + 1) + "\n";
    riscv_ret_str += string(value->name + 1) + ":\n";

    if (value->ty->data.pointer.base->tag == KOOPA_RTT_INT32)
    {
        switch (global_alloc.init->kind.tag)
        {
        case KOOPA_RVT_ZERO_INIT:
            riscv_ret_str += "\t.zero 4\n";
            break;
        case KOOPA_RVT_INTEGER:
            riscv_ret_str += "\t.word ";
            Visit(global_alloc.init->kind.data.integer);
            riscv_ret_str += "\n";
            break;
        default:
            assert(0);
            break;
        }
    }
    else // array;
    {
        switch (global_alloc.init->kind.tag)
        {
        case KOOPA_RVT_ZERO_INIT:
            riscv_ret_str += "\t.zero " + to_string(_cal_size(value->ty->data.pointer.base)) + "\n";
            break;
        case KOOPA_RVT_AGGREGATE:
            globalArrayInit(global_alloc.init);
            break;
        default:
            assert(0);
            break;
        }
    }
    riscv_ret_str += "\n";
}

void Visit(const koopa_raw_load_t &load, const koopa_raw_value_t &value)
{
    cerr << "--!load" << endl;
    switch (load.src->kind.tag)
    {
    case KOOPA_RVT_GLOBAL_ALLOC:
        riscv_ret_str += "\tla t0, " + string(load.src->name + 1) + "\n";
        riscv_ret_str += "\tlw t0, (t0)\n";
        break;
    case KOOPA_RVT_ALLOC:
        if (var_table.get(load.src) <= 2047 && var_table.get(load.src) >= -2048)
            riscv_ret_str += "\tlw t0, " + to_string(var_table.get(load.src)) + "(sp)\n";
        else
        {
            riscv_ret_str += "\tli t6, " + to_string(var_table.get(load.src)) + "\n";
            riscv_ret_str += "\tadd t6, t6, sp\n";
            riscv_ret_str += "\tlw t0, (t6)\n";
        }
        break;
    default:
        if (var_table.get(load.src) <= 2047 && var_table.get(load.src) >= -2048)
            riscv_ret_str += "\tlw t0, " + to_string(var_table.get(load.src)) + "(sp)\n";
        else
        {
            riscv_ret_str += "\tli t6, " + to_string(var_table.get(load.src)) + "\n";
            riscv_ret_str += "\tadd t6, t6, sp\n";
            riscv_ret_str += "\tlw t0, (t6)\n";
        }
        riscv_ret_str += "\tlw t0, (t0)\n";
        break;
    }
    if (var_table.get(value) <= 2047 && var_table.get(value) >= -2048)
        riscv_ret_str += "\tsw t0, " + to_string(var_table.get(value)) + "(sp)\n";
    else
    {
        riscv_ret_str += "\tli t6, " + to_string(var_table.get(value)) + "\n";
        riscv_ret_str += "\tadd t6, t6, sp\n";
        riscv_ret_str += "\tsw t0, (t6)\n";
    }
}

void Visit(const koopa_raw_store_t &store)
{
    cerr << "--!store" << endl;
    size_t param_idx = 0;
    switch (store.value->kind.tag)
    {
    case KOOPA_RVT_INTEGER:
        riscv_ret_str += "\tli t0, ";
        Visit(store.value->kind.data.integer);
        riscv_ret_str += "\n";
        switch (store.dest->kind.tag)
        {
        case KOOPA_RVT_GLOBAL_ALLOC:
            riscv_ret_str += "\tla t1, " + string(store.dest->name + 1) + "\n";
            riscv_ret_str += "\tsw t0, (t1)\n";
            break;
        case KOOPA_RVT_ALLOC:
            if (var_table.get(store.dest) <= 2047 && var_table.get(store.dest) >= -2048)
                riscv_ret_str += "\tsw t0, " + to_string(var_table.get(store.dest)) + "(sp)\n";
            else
            {
                riscv_ret_str += "\tli t6, " + to_string(var_table.get(store.dest)) + "\n";
                riscv_ret_str += "\tadd t6, t6, sp\n";
                riscv_ret_str += "\tsw t0, (t6)\n";
            }
            break;
        case KOOPA_RVT_GET_ELEM_PTR:
        case KOOPA_RVT_GET_PTR:
            if (var_table.get(store.dest) <= 2047 && var_table.get(store.dest) >= -2048)
                riscv_ret_str += "\tlw t1, " + to_string(var_table.get(store.dest)) + "(sp)\n";
            else
            {
                riscv_ret_str += "\tli t6, " + to_string(var_table.get(store.dest)) + "\n";
                riscv_ret_str += "\tadd t6, t6, sp\n";
                riscv_ret_str += "\tlw t1, (t6)\n";
            }
            riscv_ret_str += "\tsw t0, (t1)\n";
            break;
        default:
            assert(0);
        }
        break;
    case KOOPA_RVT_FUNC_ARG_REF:
        param_idx = store.value->kind.data.func_arg_ref.index;
        if (param_idx < 8)
        {
            switch (store.dest->kind.tag)
            {
            case KOOPA_RVT_GLOBAL_ALLOC:
                riscv_ret_str += "\tla t0, " + string(store.dest->name + 1) + "\n";
                riscv_ret_str += "\tsw a" + to_string(param_idx) + ", (t0)\n";
                break;
            case KOOPA_RVT_ALLOC:
                if (var_table.get(store.dest) <= 2047 && var_table.get(store.dest) >= -2048)
                    riscv_ret_str += "\tsw a" + to_string(param_idx) + ", " + to_string(var_table.get(store.dest)) + "(sp)\n";
                else
                {
                    riscv_ret_str += "\tli t6, " + to_string(var_table.get(store.dest)) + "\n";
                    riscv_ret_str += "\tadd t6, t6, sp\n";
                    riscv_ret_str += "\tsw a" + to_string(param_idx) + ", (t6)\n";
                }
                break;
            case KOOPA_RVT_GET_ELEM_PTR:
            case KOOPA_RVT_GET_PTR:
                if (var_table.get(store.dest) <= 2047 && var_table.get(store.dest) >= -2048)
                    riscv_ret_str += "\tlw t0, " + to_string(var_table.get(store.dest)) + "(sp)\n";
                else
                {
                    riscv_ret_str += "\tli t6, " + to_string(var_table.get(store.dest)) + "\n";
                    riscv_ret_str += "\tadd t6, t6, sp\n";
                    riscv_ret_str += "\tlw t0, (t6)\n";
                }
                riscv_ret_str += "\tsw a" + to_string(param_idx) + ", (t0)\n";
                break;
            default:
                assert(0);
            }
        }
        else
        {
            if ((param_idx - 8) * 4 + S_ <= 2047 && (param_idx - 8) * 4 + S_ >= -2048)
                riscv_ret_str += "\tlw t0, " + to_string((param_idx - 8) * 4 + S_) + "(sp)\n";
            else
            {
                riscv_ret_str += "\tli t6, " + to_string((param_idx - 8) * 4 + S_) + "\n";
                riscv_ret_str += "\tadd t6, t6, sp\n";
                riscv_ret_str += "\tlw t0, (t6)\n";
            }
            switch (store.dest->kind.tag)
            {
            case KOOPA_RVT_GLOBAL_ALLOC:
                riscv_ret_str += "\tla t1, " + string(store.dest->name + 1) + "\n";
                riscv_ret_str += "\tsw t0, (t1)\n";
                break;
            case KOOPA_RVT_ALLOC:
                if (var_table.get(store.dest) <= 2047 && var_table.get(store.dest) >= -2048)
                    riscv_ret_str += "\tsw t0, " + to_string(var_table.get(store.dest)) + "(sp)\n";
                else
                {
                    riscv_ret_str += "\tli t6, " + to_string(var_table.get(store.dest)) + "\n";
                    riscv_ret_str += "\tadd t6, t6, sp\n";
                    riscv_ret_str += "\tsw t0, (t6)\n";
                }
                break;
            case KOOPA_RVT_GET_ELEM_PTR:
            case KOOPA_RVT_GET_PTR:
                if (var_table.get(store.dest) <= 2047 && var_table.get(store.dest) >= -2048)
                    riscv_ret_str += "\tlw t1, " + to_string(var_table.get(store.dest)) + "(sp)\n";
                else
                {
                    riscv_ret_str += "\tli t6, " + to_string(var_table.get(store.dest)) + "\n";
                    riscv_ret_str += "\tadd t6, t6, sp\n";
                    riscv_ret_str += "\tlw t1, (t6)\n";
                }
                riscv_ret_str += "\tsw t0, (t1)\n";
                break;
            default:
                assert(0);
            }
        }
        break;
    default:
        if (var_table.get(store.value) <= 2047 && var_table.get(store.value) >= -2048)
            riscv_ret_str += "\tlw t0, " + to_string(var_table.get(store.value)) + "(sp)\n";
        else
        {
            riscv_ret_str += "\tli t6, " + to_string(var_table.get(store.value)) + "\n";
            riscv_ret_str += "\tadd t6, t6, sp\n";
            riscv_ret_str += "\tlw t0, (t6)\n";
        }
        switch (store.dest->kind.tag)
        {
        case KOOPA_RVT_GLOBAL_ALLOC:
            riscv_ret_str += "\tla t1, " + string(store.dest->name + 1) + "\n";
            riscv_ret_str += "\tsw t0, (t1)\n";
            break;
        case KOOPA_RVT_ALLOC:
            if (var_table.get(store.dest) <= 2047 && var_table.get(store.dest) >= -2048)
                riscv_ret_str += "\tsw t0, " + to_string(var_table.get(store.dest)) + "(sp)\n";
            else
            {
                riscv_ret_str += "\tli t6, " + to_string(var_table.get(store.dest)) + "\n";
                riscv_ret_str += "\tadd t6, t6, sp\n";
                riscv_ret_str += "\tsw t0, (t6)\n";
            }
            break;
        case KOOPA_RVT_GET_ELEM_PTR:
        case KOOPA_RVT_GET_PTR:
            if (var_table.get(store.dest) <= 2047 && var_table.get(store.dest) >= -2048)
                riscv_ret_str += "\tlw t1, " + to_string(var_table.get(store.dest)) + "(sp)\n";
            else
            {
                riscv_ret_str += "\tli t6, " + to_string(var_table.get(store.dest)) + "\n";
                riscv_ret_str += "\tadd t6, t6, sp\n";
                riscv_ret_str += "\tlw t1, (t6)\n";
            }
            riscv_ret_str += "\tsw t0, (t1)\n";
            break;
        default:
            assert(0);
        }
        break;
    }
}

void Visit(const koopa_raw_binary_t &binary, const koopa_raw_value_t &value)
{
    if (binary.lhs->kind.tag == KOOPA_RVT_INTEGER)
    {
        riscv_ret_str += "\tli t0, ";
        Visit(binary.lhs->kind.data.integer);
        riscv_ret_str += "\n";
    }
    else
    {
        if (var_table.get(binary.lhs) <= 2047 && var_table.get(binary.lhs) >= -2048)
            riscv_ret_str += "\tlw t0, " + to_string(var_table.get(binary.lhs)) + "(sp)\n";
        else
        {
            riscv_ret_str += "\tli t6, " + to_string(var_table.get(binary.lhs)) + "\n";
            riscv_ret_str += "\tadd t6, t6, sp\n";
            riscv_ret_str += "\tlw t0, (t6)\n";
        }
    }

    if (binary.rhs->kind.tag == KOOPA_RVT_INTEGER)
    {
        riscv_ret_str += "\tli t1, ";
        Visit(binary.rhs->kind.data.integer);
        riscv_ret_str += "\n";
    }
    else
    {
        if (var_table.get(binary.rhs) <= 2047 && var_table.get(binary.rhs) >= -2048)
            riscv_ret_str += "\tlw t1, " + to_string(var_table.get(binary.rhs)) + "(sp)\n";
        else
        {
            riscv_ret_str += "\tli t6, " + to_string(var_table.get(binary.rhs)) + "\n";
            riscv_ret_str += "\tadd t6, t6, sp\n";
            riscv_ret_str += "\tlw t1, (t6)\n";
        }
    }

    switch (binary.op)
    {
    case KOOPA_RBO_LE:
        riscv_ret_str += "\tsgt t0, t0, t1\n";
        riscv_ret_str += "\tseqz t0, t0\n";
        break;
    case KOOPA_RBO_GE:
        riscv_ret_str += "\tslt t0, t0, t1\n";
        riscv_ret_str += "\tseqz t0, t0\n";
        break;
    case KOOPA_RBO_NOT_EQ:
        riscv_ret_str += "\txor t0, t0, t1\n";
        riscv_ret_str += "\tsnez t0, t0\n";
        break;
    case KOOPA_RBO_EQ:
        riscv_ret_str += "\txor t0, t0, t1\n";
        riscv_ret_str += "\tseqz t0, t0\n";
        break;
    default:
        riscv_ret_str += "\t" + op2riscv[binary.op] + " t0, t0, t1\n";
        break;
    }
    if (var_table.get(value) <= 2047 && var_table.get(value) >= -2048)
        riscv_ret_str += "\tsw t0, " + to_string(var_table.get(value)) + "(sp)\n";
    else
    {
        riscv_ret_str += "\tli t6, " + to_string(var_table.get(value)) + "\n";
        riscv_ret_str += "\tadd t6, t6, sp\n";
        riscv_ret_str += "\tsw t0, (t6)\n";
    }
}

void Visit(const koopa_raw_branch_t &branch)
{
    if (branch.cond->kind.tag == KOOPA_RVT_INTEGER)
    {
        riscv_ret_str += "\tli t0, ";
        Visit(branch.cond->kind.data.integer);
        riscv_ret_str += "\n";
    }
    else
    {
        if (var_table.get(branch.cond) <= 2047 && var_table.get(branch.cond) >= -2048)
            riscv_ret_str += "\tlw t0, " + to_string(var_table.get(branch.cond)) + "(sp)\n";
        else
        {
            riscv_ret_str += "\tli t6, " + to_string(var_table.get(branch.cond)) + "\n";
            riscv_ret_str += "\tadd t6, t6, sp\n";
            riscv_ret_str += "\tlw t0, (t6)\n";
        }
    }

    riscv_ret_str += "\tbnez t0, " + string(branch.true_bb->name + 1) + "\n";
    riscv_ret_str += "\tj " + string(branch.false_bb->name + 1) + "\n";
}

void Visit(const koopa_raw_jump_t &jump)
{
    riscv_ret_str += "\tj " + string(jump.target->name + 1) + "\n";
}

void Visit(const koopa_raw_call_t &call, const koopa_raw_value_t &value)
{
    for (size_t i = 0; i < call.args.len; ++i)
    {
        if (i == 8)
            break;

        auto val = reinterpret_cast<koopa_raw_value_t>(call.args.buffer[i]);
        if (val->kind.tag == KOOPA_RVT_INTEGER)
        {
            riscv_ret_str += "\tli a" + to_string(i) + ", ";
            Visit(val->kind.data.integer);
            riscv_ret_str += "\n";
        }
        else
        {
            if (var_table.get(val) <= 2047 && var_table.get(val) >= -2048)
                riscv_ret_str += "\tlw a" + to_string(i) + ", " + to_string(var_table.get(val)) + "(sp)\n";
            else
            {
                riscv_ret_str += "\tli t6, " + to_string(var_table.get(val)) + "\n";
                riscv_ret_str += "\tadd t6, t6, sp\n";
                riscv_ret_str += "\tlw a" + to_string(i) + ", (t6)\n";
            }
        }
    }

    for (size_t i = 8; i < call.args.len; ++i) // 栈上传参, 已经预留好空间;
    {
        auto val = reinterpret_cast<koopa_raw_value_t>(call.args.buffer[i]);
        if (val->kind.tag == KOOPA_RVT_INTEGER)
        {
            riscv_ret_str += "\tli t0, ";
            Visit(val->kind.data.integer);
            riscv_ret_str += "\n";

            if ((i - 8) * 4 <= 2047 && (i - 8) * 4 >= -2048)
                riscv_ret_str += "\tsw t0, " + to_string((i - 8) * 4) + "(sp)\n";
            else
            {
                riscv_ret_str += "\tli t6, " + to_string((i - 8) * 4) + "\n";
                riscv_ret_str += "\tadd t6, t6, sp\n";
                riscv_ret_str += "\tsw t0, (t6)\n";
            }
        }
        else
        {
            if (var_table.get(val) <= 2047 && var_table.get(val) >= -2048)
                riscv_ret_str += "\tlw t0, " + to_string(var_table.get(val)) + "(sp)\n";
            else
            {
                riscv_ret_str += "\tli t6, " + to_string(var_table.get(val)) + "\n";
                riscv_ret_str += "\tadd t6, t6, sp\n";
                riscv_ret_str += "\tlw t0, (t6)\n";
            }

            if ((i - 8) * 4 <= 2047 && (i - 8) * 4 >= -2048)
                riscv_ret_str += "\tsw t0, " + to_string((i - 8) * 4) + "(sp)\n";
            else
            {
                riscv_ret_str += "\tli t6, " + to_string((i - 8) * 4) + "\n";
                riscv_ret_str += "\tadd t6, t6, sp\n";
                riscv_ret_str += "\tsw t0, (t6)\n";
            }
        }
    }

    riscv_ret_str += "\tcall " + string(call.callee->name + 1) + "\n";

    if (value->ty->tag != KOOPA_RTT_UNIT)
    {
        if (var_table.get(value) <= 2047 && var_table.get(value) >= -2048)
            riscv_ret_str += "\tsw a0, " + to_string(var_table.get(value)) + "(sp)\n";
        else
        {
            riscv_ret_str += "\tli t6, " + to_string(var_table.get(value)) + "\n";
            riscv_ret_str += "\tadd t6, t6, sp\n";
            riscv_ret_str += "\tsw a0, (t6)\n";
        }
    }
}

void Visit(const koopa_raw_get_elem_ptr_t &get_elem_ptr, const koopa_raw_value_t &value)
{
    int addr = 0;
    switch (get_elem_ptr.src->kind.tag)
    {
    case KOOPA_RVT_GLOBAL_ALLOC:
        riscv_ret_str += "\tla t0, " + string(get_elem_ptr.src->name + 1) + "\n";
        break;
    case KOOPA_RVT_ALLOC:
        addr = var_table.get(get_elem_ptr.src);
        if (addr <= 2047 && addr >= -2048)
        {
            riscv_ret_str += "\taddi t0, sp, " + to_string(addr) + "\n";
        }
        else
        {
            riscv_ret_str += "\tli t0, " + to_string(addr) + "\n";
            riscv_ret_str += "\tadd t0, sp, t0\n";
        }
        break;
    case KOOPA_RVT_GET_ELEM_PTR:
    case KOOPA_RVT_GET_PTR:
        cerr << "--!getelemptr here" << endl;
        if (var_table.get(get_elem_ptr.src) <= 2047 && var_table.get(get_elem_ptr.src) >= -2048)
            riscv_ret_str += "\tlw t0, " + to_string(var_table.get(get_elem_ptr.src)) + "(sp)\n";
        else
        {
            riscv_ret_str += "\tli t6, " + to_string(var_table.get(get_elem_ptr.src)) + "\n";
            riscv_ret_str += "\tadd t6, t6, sp\n";
            riscv_ret_str += "\tlw t0, (t6)\n";
        }
        break;
    default:
        assert(0);
        break;
    }

    if (get_elem_ptr.index->kind.tag == KOOPA_RVT_INTEGER)
    {
        riscv_ret_str += "\tli t1, ";
        Visit(get_elem_ptr.index->kind.data.integer);
        riscv_ret_str += "\n";
    }
    else
    {
        if (var_table.get(get_elem_ptr.index) <= 2047 && var_table.get(get_elem_ptr.index) >= -2048)
            riscv_ret_str += "\tlw t1, " + to_string(var_table.get(get_elem_ptr.index)) + "(sp)\n";
        else
        {
            riscv_ret_str += "\tli t6, " + to_string(var_table.get(get_elem_ptr.index)) + "\n";
            riscv_ret_str += "\tadd t6, t6, sp\n";
            riscv_ret_str += "\tlw t1, (t6)\n";
        }
    }

    riscv_ret_str += "\tli t2, " + to_string(_cal_size(get_elem_ptr.src->ty->data.pointer.base->data.array.base)) + "\n";

    riscv_ret_str += "\tmul t1, t1, t2\n";
    riscv_ret_str += "\tadd t0, t0, t1\n";

    if (var_table.get(value) <= 2047 && var_table.get(value) >= -2048)
        riscv_ret_str += "\tsw t0, " + to_string(var_table.get(value)) + "(sp)\n";
    else
    {
        riscv_ret_str += "\tli t6, " + to_string(var_table.get(value)) + "\n";
        riscv_ret_str += "\tadd t6, t6, sp\n";
        riscv_ret_str += "\tsw t0, (t6)\n";
    }
}

void Visit(const koopa_raw_get_ptr_t &get_ptr, const koopa_raw_value_t &value)
{
    int addr = 0;
    switch (get_ptr.src->kind.tag)
    {
    case KOOPA_RVT_GLOBAL_ALLOC:
        riscv_ret_str += "\tla t0, " + string(get_ptr.src->name + 1) + "\n";
        break;
    case KOOPA_RVT_ALLOC:
        addr = var_table.get(get_ptr.src);
        if (addr <= 2047 && addr >= -2048)
        {
            riscv_ret_str += "\taddi t0, sp, " + to_string(addr) + "\n";
        }
        else
        {
            riscv_ret_str += "\tli t0, " + to_string(addr) + "\n";
            riscv_ret_str += "\tadd t0, sp, t0\n";
        }
        break;
    case KOOPA_RVT_GET_ELEM_PTR:
    case KOOPA_RVT_GET_PTR:
    case KOOPA_RVT_LOAD:

        if (var_table.get(get_ptr.src) <= 2047 && var_table.get(get_ptr.src) >= -2048)
            riscv_ret_str += "\tlw t0, " + to_string(var_table.get(get_ptr.src)) + "(sp)\n";
        else
        {
            riscv_ret_str += "\tli t6, " + to_string(var_table.get(get_ptr.src)) + "\n";
            riscv_ret_str += "\tadd t6, t6, sp\n";
            riscv_ret_str += "\tlw t0, (t6)\n";
        }
        break;
    default:
        assert(0);
        break;
    }
    if (get_ptr.index->kind.tag == KOOPA_RVT_INTEGER)
    {
        riscv_ret_str += "\tli t1, ";
        Visit(get_ptr.index->kind.data.integer);
        riscv_ret_str += "\n";
    }
    else
    {
        if (var_table.get(get_ptr.index) <= 2047 && var_table.get(get_ptr.index) >= -2048)
            riscv_ret_str += "\tlw t1, " + to_string(var_table.get(get_ptr.index)) + "(sp)\n";
        else
        {
            riscv_ret_str += "\tli t6, " + to_string(var_table.get(get_ptr.index)) + "\n";
            riscv_ret_str += "\tadd t6, t6, sp\n";
            riscv_ret_str += "\tlw t1, (t6)\n";
        }
    }

    riscv_ret_str += "\tli t2, " + to_string(_cal_size(get_ptr.src->ty->data.pointer.base)) + "\n";
    riscv_ret_str += "\tmul t1, t1, t2\n";
    riscv_ret_str += "\tadd t0, t0, t1\n";

    if (var_table.get(value) <= 2047 && var_table.get(value) >= -2048)
        riscv_ret_str += "\tsw t0, " + to_string(var_table.get(value)) + "(sp)\n";
    else
    {
        riscv_ret_str += "\tli t6, " + to_string(var_table.get(value)) + "\n";
        riscv_ret_str += "\tadd t6, t6, sp\n";
        riscv_ret_str += "\tsw t0, (t6)\n";
    }
}