#include "ast.hpp"

unordered_map<string, string> op2ir =
    {
        {"+", "add"},
        {"-", "sub"},
        {"*", "mul"},
        {"/", "div"},
        {"%", "mod"},
        {"<", "lt"},
        {">", "gt"},
        {"<=", "le"},
        {">=", "ge"},
        {"==", "eq"},
        {"!=", "ne"}};

int sym_no = 0;      // 下一个可用的临时变量的编号;
int if_label_no = 0; // 下一个可用的if_label的编号;

int cur_basic_block = 0; // 用于判断当前程序块是否已经生成了br, jump或ret指令;
unordered_map<int, bool> is_end;

int while_label_no = 0;   // 下一个可用的while_label的编号;
int cur_while_level = -1; // 现在所处位置的while_label编号;
unordered_map<int, int> while_parent;

int cur_scope = 0; // 现在所处的作用域;
unordered_map<int, int> scope_parent;

string cur_func_type;

void SymbolTable::insert(string id, int val, SYM_TYPE s_type, DATA_TYPE d_type, string name)
{
    // cout << "insert:" << id << ", " << val << ", " << is_const << endl;
    assert(table.find(id) == table.end());
    table[id] = Symbol(s_type, d_type, val, name);
    // debug();
}

void SymbolTable::insert(string id, vector<int> shape, SYM_TYPE s_type, DATA_TYPE d_type, string name)
{
    if (table.find(id) != table.end())
    {
        cerr << "---Symbol " << id << " has already been defined---" << endl;
        assert(0);
    }
    assert(array_shape_table.find(id) == array_shape_table.end());
    table[id] = Symbol(s_type, d_type, 0, name);
    array_shape_table[id] = shape;
}

bool SymbolTable::exist(string id)
{
    return table.find(id) != table.end();
}

Symbol SymbolTable::get(string id)
{
    cerr << "//! get:" << id << endl;
    assert(table.find(id) != table.end());

    return table[id];
}
vector<int> SymbolTable::getArray(string id)
{
    cerr << "//! get array shape:" << id << endl;
    assert(array_shape_table.find(id) != array_shape_table.end());

    return array_shape_table[id];
}

SymbolTableStack::SymbolTableStack()
{
    stk.push_back(SymbolTable());

    cerr << "//! construct symbol table stack\n";
}

bool SymbolTableStack::inc()
{
    assert(!stk.empty());
    if (stk.back().forAllocParam())
    {
        cerr << "//! symbol table common\n";
        stk.back().common();
        return true;
    }
    else
    {
        cerr << "//! symbol table inc, ";
        stk.push_back(SymbolTable());
        dep_label_no++;
        cerr << "now " << stk.size() << endl;
        return false;
    }
}

void SymbolTableStack::dec()
{
    cerr << "//! symbol table dec, ";
    assert(!stk.empty());
    stk.pop_back();
    // 这里dep_lable_no不能减一, 否则重名;

    cerr << "now " << stk.size() << endl;
}

void SymbolTableStack::incParam()
{
    cerr << "//! symbol table inc param, ";
    stk.push_back(SymbolTable(1));
    dep_label_no++;
    cerr << "now " << stk.size() << endl;
}

string SymbolTableStack::insert(string id, int val, SYM_TYPE s_type, DATA_TYPE d_type)
{
    assert(!stk.empty());
    cerr << "//! insert " << id << endl;

    string name = id + "_" + to_string(cur_scope);

    if (s_type == _FUNC)
        name = id;
    stk.back().insert(id, val, s_type, d_type, name);

    return name;
}

string SymbolTableStack::insert(string id, vector<int> shape, SYM_TYPE s_type, DATA_TYPE d_type)
{
    assert(!stk.empty());
    cerr << "//! insert array " << id << endl;

    string name = id + "_" + to_string(cur_scope);

    if (s_type == _FUNC)
        name = id;
    stk.back().insert(id, shape, s_type, d_type, name);

    return name;
}

bool SymbolTableStack::exist(string id)
{
    for (auto it = stk.rbegin(); it != stk.rend(); it++)
    {
        if (it->exist(id))
            return true;
    }
    return false;
}

Symbol SymbolTableStack::get(string id)
{
    for (auto it = stk.rbegin(); it != stk.rend(); it++)
    {
        if (it->exist(id))
            return it->get(id);
    }
    cerr << "---Undefined symbol: " << id << "---" << endl;
    assert(0);
}
vector<int> SymbolTableStack::getArray(string id)
{
    for (auto it = stk.rbegin(); it != stk.rend(); it++)
    {
        if (it->exist(id))
            return it->getArray(id);
    }
    assert(0);
}
Symbol SymbolTableStack::getFromGlobal(string id)
{
    return stk.front().get(id);
}

SymbolTableStack symbol_table;

// 两个操作数都是数字;
string _generate_b(int l, string op, int r)
{
    // assert(op2ir.find(op) != op2ir.end());
    return "\t%" + to_string(sym_no) + " = " + op2ir[op] + " " + to_string(l) + ", " + to_string(r) + "\n";
}

// 左操作数是数字;
string _generate_l(int l, string op, int sno_r)
{
    return "\t%" + to_string(sym_no) + " = " + op2ir[op] + " " + to_string(l) + ", %" + to_string(sno_r) + "\n";
}

// 右操作数是数字;
string _generate_r(int sno_l, string op, int r)
{
    return "\t%" + to_string(sym_no) + " = " + op2ir[op] + " %" + to_string(sno_l) + ", " + to_string(r) + "\n";
}

// 两个操作数都不是数字;
string _generate_n(int sno_l, string op, int sno_r)
{
    return "\t%" + to_string(sym_no) + " = " + op2ir[op] + " %" + to_string(sno_l) + ", %" + to_string(sno_r) + "\n";
}

string getArrayInitVal(vector<int> *vec, int s_pos, vector<int> shape)
{
    string ret = "{";
    int n = shape[0];
    vector<int> _shape(shape.begin() + 1, shape.end());
    if (_shape.empty())
    {

        bool first = true;
        for (int i = 0; i < n; ++i)
        {
            if (!first)
                ret += ", ";
            ret += to_string((*vec)[i + s_pos]);
            first = false;
        }
    }
    else
    {
        int width = 1;
        for (auto s : _shape)
            width *= s;

        bool first = true;
        for (int i = 0; i < n; ++i)
        {
            if (!first)
                ret += ", ";
            ret += getArrayInitVal(vec, s_pos + i * width, _shape);
            first = false;
        }
    }
    ret += "}";
    return ret;
}

string localArrayInit(string name, vector<int> *vec, int s_pos, vector<int> shape)
{
    cerr << "//! localarrayinit\n";
    string ret;
    int n = shape[0];
    vector<int> _shape(shape.begin() + 1, shape.end());
    int width = 1;
    for (auto s : _shape)
        width *= s;

    for (int i = 0; i < n; ++i)
    {
        ret += "\t%" + to_string(sym_no) + " = getelemptr " + name + ", " + to_string(i) + "\n";
        sym_no++;

        if (_shape.empty())
            ret += "\tstore " + to_string((*vec)[i + s_pos]) + ", %" + to_string(sym_no - 1) + "\n";
        else
            ret += localArrayInit("%" + to_string(sym_no - 1), vec, s_pos + i * width, _shape);
    }

    return ret;
}
RetVal StartSymbolAST::Dump() const
{
    koopa_ret_str += "decl @getint(): i32\n";
    koopa_ret_str += "decl @getch(): i32\n";
    koopa_ret_str += "decl @getarray(*i32): i32\n";
    koopa_ret_str += "decl @putint(i32)\n";
    koopa_ret_str += "decl @putch(i32)\n";
    koopa_ret_str += "decl @putarray(i32, *i32)\n";
    koopa_ret_str += "decl @starttime()\n";
    koopa_ret_str += "decl @stoptime()\n\n";

    symbol_table.insert("getint", 0, _FUNC, _INT);
    symbol_table.insert("getch", 0, _FUNC, _INT);
    symbol_table.insert("getarray", 0, _FUNC, _INT);
    symbol_table.insert("putint", 0, _FUNC, _VOID);
    symbol_table.insert("putch", 0, _FUNC, _VOID);
    symbol_table.insert("putarray", 0, _FUNC, _VOID);
    symbol_table.insert("starttime", 0, _FUNC, _VOID);
    symbol_table.insert("stoptime", 0, _FUNC, _VOID);

    return compunit->Dump();
}

RetVal CompUnitAST::Dump() const
{
    cerr << "//! compunit\n";
    if (compunit)
        (*compunit)->Dump();
    funcdef_decl->Dump();
    return RetVal();
}

RetVal FuncDefAST::Dump() const
{
    cerr << "//! funcdef, cur scope: " << cur_scope << endl;

    sym_no = 0;

    cur_func_type = functype;

    if (functype == "int")
        symbol_table.insert(ident, 0, _FUNC, _INT);
    else if (functype == "void")
        symbol_table.insert(ident, 0, _FUNC, _VOID);
    else
        assert(0);

    symbol_table.incParam();

    scope_parent[symbol_table.getDepLabelNo()] = cur_scope;
    cur_scope = symbol_table.getDepLabelNo();

    koopa_ret_str += ("fun @" + ident + "( ");

    if (params)
        (*params)->Dump();

    koopa_ret_str += " )";

    if (functype == "int")
        koopa_ret_str += ": i32";

    cerr << "//! entry of func:" << ident << endl;

    koopa_ret_str += " {\n%entry :\n ";

    cur_basic_block++;
    is_end[cur_basic_block] = false;

    if (params)
        (*params)->Alloc();

    block->Dump();

    if (functype == "void" && !is_end[cur_basic_block])
        koopa_ret_str += "\tret\n";

    if (functype == "int" && !is_end[cur_basic_block])
        koopa_ret_str += "\tret 0\n";

    is_end[cur_basic_block] = true;

    koopa_ret_str += "}\n";

    cur_scope = scope_parent[cur_scope];

    cerr << "//! func def end, cur scope: " << cur_scope << endl;

    return RetVal();
}

RetVal FuncFParamsAST::Dump() const
{

    bool first = true;
    for (auto &param : funcfparams)
    {
        if (!first)
            koopa_ret_str += ", ";
        param->Dump();
        first = false;
    }

    return RetVal();
}

RetVal FuncFParamsAST::Alloc() const
{
    for (auto &param : funcfparams)
    {
        param->Alloc();
    }
    return RetVal();
}

RetVal FuncFParamAST::Dump() const
{
    if (derive_type == NUMBER)
    {
        koopa_ret_str += "@" + ident;
        if (btype == "int")
            koopa_ret_str += ": i32";
        else
            assert(0);
        return RetVal();
    }
    koopa_ret_str += "@" + ident + ": *";
    string res;
    if (btype == "int")
        res += "i32";
    else
        assert(0);
    for (auto &constexp : constexps)
    {
        res = "[" + res + ", " + to_string(constexp->Cal()) + "]";
    }
    koopa_ret_str += res;
    return RetVal();
}

RetVal FuncFParamAST::Alloc() const
{
    if (derive_type == NUMBER)
    {
        koopa_ret_str += "\t@" + ident + "_" + to_string(cur_scope) + " = alloc i32\n";

        koopa_ret_str += "\tstore @" + ident + ", @" + ident + "_" + to_string(cur_scope) + "\n";

        symbol_table.insert(ident, 0, _VAR, _INT);
    }
    else
    {
        vector<int> origin_shape;
        vector<int> padding_shape;
        padding_shape.push_back(-1);

        for (auto &constexp : constexps)
        {
            origin_shape.push_back(constexp->Cal());
        }

        for (int l : origin_shape)
            padding_shape.push_back(l);

        string name = symbol_table.insert(ident, padding_shape, VAR_ARRAY, _INT);

        string res = "i32";
        for (int i = origin_shape.size() - 1; i >= 0; --i)
        {
            res = "[" + res + ", " + to_string(origin_shape[i]) + "]";
        }

        koopa_ret_str += " \t@" + name + " = alloc *" + res + "\n";
        koopa_ret_str += "\tstore @" + ident + ", @" + name + "\n";
    }

    return RetVal();
}

RetVal BlockAST::Dump() const
{
    bool common = symbol_table.inc();
    cerr << "//! block begin, cur_scope: " << cur_scope << endl;
    if (!common)
    {
        scope_parent[symbol_table.getDepLabelNo()] = cur_scope;
        cur_scope = symbol_table.getDepLabelNo();
    }

    is_end[cur_basic_block] = false;
    for (auto &blockitem : blockitems)
        blockitem->Dump();
    symbol_table.dec();
    cur_scope = scope_parent[cur_scope];
    cerr << "//! block end, cur_scope: " << cur_scope << endl;
    return RetVal();
}

RetVal BlockItemAST::Dump() const
{
    if (is_end[cur_basic_block])
        return RetVal();
    return ds->Dump();
}

RetVal StmtAST::Dump() const
{
    if (derive_type == ASSIGN)
    {
        string name = lval->Dump().getStr();

        RetVal ret_val = rexpr->Dump();

        if (ret_val.isNumber())
        {
            koopa_ret_str += "\tstore " + to_string(ret_val.getVal()) + ", " + name + "\n";
        }
        else
        {
            koopa_ret_str += "\tstore %" + to_string(sym_no - 1) + ", " + name + "\n";
        }

        return RetVal();
    }
    else if (derive_type == RETURN)
    {
        if (is_end[cur_basic_block])
            return RetVal();

        if (expr)
        {
            RetVal ret_val = (*expr)->Dump();
            // symbol_table.debug();
            if (ret_val.isNumber())
            {
                koopa_ret_str += "\tret " + to_string(ret_val.getVal()) + "\n";
            }
            else
            {

                koopa_ret_str += "\tret %" + to_string(sym_no - 1) + "\n";
            }
        }
        else
        {
            if (cur_func_type == "void")
                koopa_ret_str += "\tret\n";
            else if (cur_func_type == "int")
                koopa_ret_str += "\tret 0\n";
            else
                assert(0);
        }
        is_end[cur_basic_block] = true;
        return RetVal();
    }
    else if (derive_type == EXPR)
    {
        if (expr)
            (*expr)->Dump();
        return RetVal();
    }
    else
    {
        return block->Dump();
    }
    return RetVal();
}

RetVal IfStmtAST::Dump() const
{
    if (is_end[cur_basic_block])
        return RetVal();

    int cur_if_label_no = if_label_no;
    if_label_no++;

    RetVal ret_val = expr->Dump();

    if (ret_val.isNumber())
    {
        koopa_ret_str += "\tbr " + to_string(ret_val.getVal()) + ", %then_" + to_string(cur_if_label_no);
        if (derive_type == NOELSE)
            koopa_ret_str += ", %end_";
        else
            koopa_ret_str += ", %else_";
        koopa_ret_str += to_string(cur_if_label_no) + "\n";
    }
    else
    {
        koopa_ret_str += "\tbr %" + to_string(sym_no - 1) + ", %then_" + to_string(cur_if_label_no);
        if (derive_type == NOELSE)
            koopa_ret_str += ", %end_";
        else
            koopa_ret_str += ", %else_";
        koopa_ret_str += to_string(cur_if_label_no) + "\n";
    }

    koopa_ret_str += "\n%then_" + to_string(cur_if_label_no) + ":\n";

    cur_basic_block++;
    is_end[cur_basic_block] = false;

    ifstmt->Dump();
    if (!is_end[cur_basic_block])
        koopa_ret_str += "\tjump %end_" + to_string(cur_if_label_no) + "\n";

    if (derive_type == ELSE)
    {
        cur_basic_block++;
        is_end[cur_basic_block] = false;

        koopa_ret_str += "\n%else_" + to_string(cur_if_label_no) + ":\n";
        elsestmt->Dump();

        if (!is_end[cur_basic_block])
            koopa_ret_str += "\tjump %end_" + to_string(cur_if_label_no) + "\n";
    }

    koopa_ret_str += "\n%end_" + to_string(cur_if_label_no) + ":\n";

    cur_basic_block++;
    is_end[cur_basic_block] = false;

    return RetVal();
}

RetVal WhileStmtAST::Dump() const
{

    while_label_no++;

    while_parent[while_label_no] = cur_while_level;

    cur_while_level = while_label_no;

    if (!is_end[cur_basic_block])
        koopa_ret_str += "\tjump %while_entry_" + to_string(cur_while_level) + "\n";

    koopa_ret_str += "\n%while_entry_" + to_string(cur_while_level) + ":\n";

    cur_basic_block++;
    is_end[cur_basic_block] = false;

    RetVal ret_val = expr->Dump();

    if (ret_val.isNumber())
    {
        koopa_ret_str += "\tbr " + to_string(ret_val.getVal()) + ", %while_body_" + to_string(cur_while_level);

        koopa_ret_str += ", %while_end_";
        koopa_ret_str += to_string(cur_while_level) + "\n";
    }
    else
    {
        koopa_ret_str += "\tbr %" + to_string(sym_no - 1) + ", %while_body_" + to_string(cur_while_level);

        koopa_ret_str += ", %while_end_";
        koopa_ret_str += to_string(cur_while_level) + "\n";
    }

    koopa_ret_str += "\n%while_body_" + to_string(cur_while_level) + ":\n";

    cur_basic_block++;
    is_end[cur_basic_block] = false;

    whilestmt->Dump();

    if (!is_end[cur_basic_block])
        koopa_ret_str += "\tjump %while_entry_" + to_string(cur_while_level) + "\n";

    koopa_ret_str += "\n%while_end_" + to_string(cur_while_level) + ":\n";

    cur_basic_block++;
    is_end[cur_basic_block] = false;

    cur_while_level = while_parent[cur_while_level];

    return RetVal();
}

RetVal BrConStmtAST::Dump() const
{
    if (is_end[cur_basic_block])
        return RetVal();

    if (derive_type == BREAK)
    {
        koopa_ret_str += "\tjump %while_end_" + to_string(cur_while_level) + "\n";
    }
    else
    {
        koopa_ret_str += "\tjump %while_entry_" + to_string(cur_while_level) + "\n";
    }

    is_end[cur_basic_block] = true;

    return RetVal();
}

RetVal ExprAST::Dump() const
{
    return lorexp->Dump();
}

RetVal UnaryExpAST::Dump() const
{
    if (derive_type == PRIMARYEXP)
        return primaryexp->Dump();
    else
    {
        RetVal ret_val = unaryexp->Dump();
        if (ret_val.isNumber())
        {
            if (op == "!")
            {
                koopa_ret_str += ("\t%" + to_string(sym_no) + " = eq 0, " + to_string(ret_val.getVal()) + "\n");
                sym_no++;
                return RetVal();
            }
            else if (op == "-")
            {
                koopa_ret_str += ("\t%" + to_string(sym_no) + " = sub 0, " + to_string(ret_val.getVal()) + "\n");
                sym_no++;
                return RetVal();
            }
            else if (op == "+")
            {
                return RetVal(ret_val.getVal());
            }
        }
        else
        {
            if (op == "-")
            {
                koopa_ret_str += ("\t%" + to_string(sym_no) + " = " + "sub 0, %" + to_string(sym_no - 1) + "\n");
                sym_no++;
                return RetVal();
            }
            else if (op == "+")
            {
                return RetVal();
            }
            else if (op == "!")
            {
                koopa_ret_str += ("\t%" + to_string(sym_no) + " = " + "eq 0, %" + to_string(sym_no - 1) + "\n");
                sym_no++;
                return RetVal();
            }
        }
        return RetVal();
    }
}

RetVal FuncUnaryExpAST::Dump() const
{
    cerr << "//! func unary: " << ident << endl;

    vector<string> params_s;

    if (params)
        params_s = (*params)->Alloc();

    Symbol func_s = symbol_table.getFromGlobal(ident);

    cerr << "//! get func symbol\n";

    assert(func_s.getSymbolType() == _FUNC);
    if (func_s.getDataType() == _INT)
    {
        koopa_ret_str += "\t%" + to_string(sym_no) + " = call @" + func_s.getName() + "(";
        sym_no++;
    }
    else
    {
        koopa_ret_str += "\tcall @" + func_s.getName() + "(";
    }

    bool first = true;
    for (auto para : params_s)
    {
        if (!first)
            koopa_ret_str += ", ";
        koopa_ret_str += para;
        first = false;
    }

    koopa_ret_str += ")\n";

    return RetVal();
}

RetVal FuncRParamsAST::Dump() const
{
    for (auto &param : funcrparams)
    {
        param->Dump();
    }
    return RetVal();
}

vector<string> FuncRParamsAST::Alloc() const
{
    vector<string> ret_vec;
    for (auto &param : funcrparams)
    {
        RetVal ret_val = param->Dump();
        if (ret_val.isNumber())
            ret_vec.push_back(to_string(ret_val.getVal()));
        else
        {
            ret_vec.push_back("%" + to_string(sym_no - 1));
        }
    }
    return ret_vec;
}

RetVal LValAST::Dump() const
{
    if (derive_type == NUMBER)
    {
        Symbol lval_s = symbol_table.get(ident);
        if (lval_s.isConst())
            return RetVal(lval_s.getVal());
        else if (lval_s.getSymbolType() == _VAR)
        {
            return RetVal("@" + lval_s.getName());
        }
        else
        {

            if (symbol_table.getArray(ident)[0] == -1)
            {
                koopa_ret_str += "\t%" + to_string(sym_no) + " = load @" + lval_s.getName() + "\n";
                sym_no++;
                return RetVal();
            }
            koopa_ret_str += "\t%" + to_string(sym_no) + " = getelemptr @" + lval_s.getName() + ", 0\n";
            sym_no++;

            return RetVal();
        }
    }
    else
    {
        vector<string> idx;

        for (auto &e : exprs)
        {
            RetVal ret_val = e->Dump();
            if (ret_val.isNumber())
                idx.push_back(to_string(ret_val.getVal()));
            else
            {
                idx.push_back("%" + to_string(sym_no - 1));
            }
        }

        vector<int> shape = symbol_table.getArray(ident);
        Symbol lval_s = symbol_table.get(ident);

        if (!shape.empty() && shape[0] == -1)
        {
            vector<int> _shape(shape.begin() + 1, shape.end());

            koopa_ret_str += "\t%" + to_string(sym_no) + " = load @" + lval_s.getName() + "\n";
            sym_no++;
            koopa_ret_str += "\t%" + to_string(sym_no) + " = getptr %" + to_string(sym_no - 1) + ", " + idx[0] + "\n";
            sym_no++;
            int i = 1;
            int n = idx.size();
            while (i < n)
            {
                koopa_ret_str += "\t%" + to_string(sym_no) + " = getelemptr %" + to_string(sym_no - 1) + ", " + idx[i] + "\n";
                i++;
                sym_no++;
            }
        }
        else
        {
            koopa_ret_str += "\t%" + to_string(sym_no) + " = getelemptr @" + lval_s.getName() + ", " + idx[0] + "\n";
            sym_no++;
            int i = 1;
            int n = idx.size();
            while (i < n)
            {
                koopa_ret_str += "\t%" + to_string(sym_no) + " = getelemptr %" + to_string(sym_no - 1) + ", " + idx[i] + "\n";
                i++;
                sym_no++;
            }
        }

        if (idx.size() < shape.size())
        {
            koopa_ret_str += "\t%" + to_string(sym_no) + " = getelemptr %" + to_string(sym_no - 1) + ", 0\n";
            sym_no++;
            return RetVal();
        }
        return RetVal("%" + to_string(sym_no - 1));
    }
}

RetVal PrimaryExpAST::Dump() const
{
    if (derive_type == EXPR)
        return expr_lval->Dump();
    else if (derive_type == LVAL)
    {
        RetVal lval_s = expr_lval->Dump();
        if (lval_s.isStr())
        {
            koopa_ret_str += "\t%" + to_string(sym_no) + " = load " + lval_s.getStr() + "\n";
            sym_no++;
        }
        return lval_s;
    }
    else
        return RetVal(number);
}

RetVal MulExpAST::Dump() const
{
    if (derive_type == UNARYEXP)
        return unaryexp->Dump();
    else
    {
        RetVal lret_val = mulexp->Dump();
        int sno_l = sym_no - 1;

        RetVal rret_val = unaryexp->Dump();
        int sno_r = sym_no - 1;

        assert(op == "*" || op == "/" || op == "%");

        if (lret_val.isNumber() && rret_val.isNumber())
        {
            koopa_ret_str += _generate_b(lret_val.getVal(), op, rret_val.getVal());
            sym_no++;
        }
        else if (lret_val.isNumber())
        {
            koopa_ret_str += _generate_l(lret_val.getVal(), op, sno_r);
            sym_no++;
        }
        else if (rret_val.isNumber())
        {
            koopa_ret_str += _generate_r(sno_l, op, rret_val.getVal());
            sym_no++;
        }
        else
        {
            koopa_ret_str += _generate_n(sno_l, op, sno_r);
            sym_no++;
        }
        return RetVal();
    }
}

RetVal AddExpAST::Dump() const
{
    if (derive_type == MULEXP)
        return mulexp->Dump();
    else
    {
        RetVal lret_val = addexp->Dump();
        int sno_l = sym_no - 1;

        RetVal rret_val = mulexp->Dump();
        int sno_r = sym_no - 1;

        assert(op == "+" || op == "-");

        if (lret_val.isNumber() && rret_val.isNumber())
        {
            koopa_ret_str += _generate_b(lret_val.getVal(), op, rret_val.getVal());
            sym_no++;
        }
        else if (lret_val.isNumber())
        {
            koopa_ret_str += _generate_l(lret_val.getVal(), op, sno_r);
            sym_no++;
        }
        else if (rret_val.isNumber())
        {
            koopa_ret_str += _generate_r(sno_l, op, rret_val.getVal());
            sym_no++;
        }
        else
        {
            koopa_ret_str += _generate_n(sno_l, op, sno_r);
            sym_no++;
        }
        return RetVal();
    }
}

RetVal RelExpAST::Dump() const
{
    if (derive_type == ADDEXP)
        return addexp->Dump();
    else
    {
        RetVal lret_val = relexp->Dump();
        int sno_l = sym_no - 1;

        RetVal rret_val = addexp->Dump();
        int sno_r = sym_no - 1;

        assert(op == "<" || op == ">" || op == "<=" || op == ">=");

        if (lret_val.isNumber() && rret_val.isNumber())
        {
            koopa_ret_str += _generate_b(lret_val.getVal(), op, rret_val.getVal());
            sym_no++;
        }
        else if (lret_val.isNumber())
        {
            koopa_ret_str += _generate_l(lret_val.getVal(), op, sno_r);
            sym_no++;
        }
        else if (rret_val.isNumber())
        {
            koopa_ret_str += _generate_r(sno_l, op, rret_val.getVal());
            sym_no++;
        }
        else
        {
            koopa_ret_str += _generate_n(sno_l, op, sno_r);
            sym_no++;
        }
        return RetVal();
    }
}

RetVal EqExpAST::Dump() const
{
    if (derive_type == RELEXP)
        return relexp->Dump();
    else
    {
        RetVal lret_val = eqexp->Dump();
        int sno_l = sym_no - 1;

        RetVal rret_val = relexp->Dump();
        int sno_r = sym_no - 1;

        assert(op == "==" || op == "!=");

        if (lret_val.isNumber() && rret_val.isNumber())
        {
            koopa_ret_str += _generate_b(lret_val.getVal(), op, rret_val.getVal());
            sym_no++;
        }
        else if (lret_val.isNumber())
        {
            koopa_ret_str += _generate_l(lret_val.getVal(), op, sno_r);
            sym_no++;
        }
        else if (rret_val.isNumber())
        {
            koopa_ret_str += _generate_r(sno_l, op, rret_val.getVal());
            sym_no++;
        }
        else
        {
            koopa_ret_str += _generate_n(sno_l, op, sno_r);
            sym_no++;
        }
        return RetVal();
    }
}

RetVal LAndExpAST::Dump() const
{
    if (derive_type == EQEXP)
        return eqexp->Dump();

    int cur_if_label_no = if_label_no;
    if_label_no++;

    koopa_ret_str += "\t@landresult_" + to_string(cur_if_label_no) + " = alloc i32" + "\n";
    koopa_ret_str += "\tstore 0, @landresult_" + to_string(cur_if_label_no) + "\n";

    RetVal lret_val = landexp->Dump();
    int sno_l = sym_no - 1;

    if (lret_val.isNumber())
    {
        koopa_ret_str += ("\t%" + to_string(sym_no) + " = ne 0, " + to_string(lret_val.getVal()) + "\n");
        sym_no++;
    }
    else
    {
        koopa_ret_str += ("\t%" + to_string(sym_no) + " = ne 0, %" + to_string(sno_l) + "\n");
        sym_no++;
    }
    koopa_ret_str += "\tbr %" + to_string(sym_no - 1) + ", %then_" + to_string(cur_if_label_no) + ", %end_" + to_string(cur_if_label_no) + "\n";

    koopa_ret_str += "\n%then_" + to_string(cur_if_label_no) + ":\n"; // lhs==1, else lhs==0, result=0;

    RetVal rret_val = eqexp->Dump();
    int sno_r = sym_no - 1;

    if (rret_val.isNumber())
    {
        koopa_ret_str += ("\t%" + to_string(sym_no) + " = ne 0, " + to_string(rret_val.getVal()) + "\n");
        sym_no++;
    }
    else
    {
        koopa_ret_str += ("\t%" + to_string(sym_no) + " = ne 0, %" + to_string(sno_r) + "\n");
        sym_no++;
    }

    koopa_ret_str += "\tstore %" + to_string(sym_no - 1) + ", @landresult_" + to_string(cur_if_label_no) + "\n"; // result=rhs;

    koopa_ret_str += "\tjump %end_" + to_string(cur_if_label_no) + "\n";

    koopa_ret_str += "\n%end_" + to_string(cur_if_label_no) + ":\n";

    koopa_ret_str += "\t%" + to_string(sym_no) + " = load @landresult_" + to_string(cur_if_label_no) + "\n";

    sym_no++;

    return RetVal();
}

RetVal LOrExpAST::Dump() const
{
    if (derive_type == LANDEXP)
        return landexp->Dump();

    int cur_if_label_no = if_label_no;
    if_label_no++;

    koopa_ret_str += "\t@lorresult_" + to_string(cur_if_label_no) + " = alloc i32" + "\n";
    koopa_ret_str += "\tstore 1, @lorresult_" + to_string(cur_if_label_no) + "\n";

    RetVal lret_val = lorexp->Dump();
    int sno_l = sym_no - 1;

    if (lret_val.isNumber())
    {
        koopa_ret_str += ("\t%" + to_string(sym_no) + " = eq 0, " + to_string(lret_val.getVal()) + "\n");
        sym_no++;
    }
    else
    {
        koopa_ret_str += ("\t%" + to_string(sym_no) + " = eq 0, %" + to_string(sno_l) + "\n");
        sym_no++;
    }

    koopa_ret_str += "\tbr %" + to_string(sym_no - 1) + ", %then_" + to_string(cur_if_label_no) + ", %end_" + to_string(cur_if_label_no) + "\n";

    koopa_ret_str += "\n%then_" + to_string(cur_if_label_no) + ":\n"; // lhs=0, continue to rhs;

    RetVal rret_val = landexp->Dump();
    int sno_r = sym_no - 1;

    if (rret_val.isNumber())
    {
        koopa_ret_str += ("\t%" + to_string(sym_no) + " = ne 0, " + to_string(rret_val.getVal()) + "\n");
        sym_no++;
    }
    else
    {
        koopa_ret_str += ("\t%" + to_string(sym_no) + " = ne 0, %" + to_string(sno_r) + "\n");
        sym_no++;
    }

    koopa_ret_str += "\tstore %" + to_string(sym_no - 1) + ", @lorresult_" + to_string(cur_if_label_no) + "\n"; // result=rhs;

    koopa_ret_str += "\tjump %end_" + to_string(cur_if_label_no) + "\n";

    koopa_ret_str += "\n%end_" + to_string(cur_if_label_no) + ":\n";

    koopa_ret_str += "\t%" + to_string(sym_no) + " = load @lorresult_" + to_string(cur_if_label_no) + "\n";

    sym_no++;

    return RetVal();
}

RetVal DeclAST::Dump() const
{
    return cvdecl->Dump();
}

RetVal ConstDeclAST::Dump() const
{
    for (auto &constdef : constdefs)
        constdef->Dump();
    return RetVal();
}

RetVal ConstDefAST::Dump() const
{
    if (derive_type == NUMBER)
    {
        RetVal ret_val = constinitval->Dump();
        symbol_table.insert(ident, ret_val.getVal(), _CONST, _INT);
    }
    // printf("insert: %s, %d\n", ident.c_str(), ret_val.getVal());
    // symbol_table.debug();
    else
    {
        vector<int> shape;
        for (auto &constexp : constexps)
        {
            shape.push_back(constexp->Cal());
        }

        string name = symbol_table.insert(ident, shape, CONST_ARRAY, _INT);

        if (cur_scope == 0)
        {
            koopa_ret_str += "global @" + name + " = alloc ";

            for (auto it = shape.begin(); it != shape.end(); it++)
            {
                koopa_ret_str += "[";
            }
            koopa_ret_str += "i32";
            for (auto it = shape.rbegin(); it != shape.rend(); it++)
            {
                koopa_ret_str += ", " + to_string(*it) + "]";
            }
            koopa_ret_str += ", ";

            auto vec = constinitval->Init(shape);
            koopa_ret_str += getArrayInitVal(&vec, 0, shape);
            koopa_ret_str += "\n";
        }
        else
        {
            koopa_ret_str += "\t@" + name + " = alloc ";
            for (auto it = shape.begin(); it != shape.end(); it++)
            {
                koopa_ret_str += "[";
            }
            koopa_ret_str += "i32";
            for (auto it = shape.rbegin(); it != shape.rend(); it++)
            {
                koopa_ret_str += ", " + to_string(*it) + "]";
            }
            koopa_ret_str += "\n";

            auto vec = constinitval->Init(shape);
            koopa_ret_str += localArrayInit("@" + name, &vec, 0, shape);
        }
    }
    return RetVal();
}

RetVal ConstInitValAST::Dump() const
{
    if (derive_type == NUMBER)
    {
        int val = constexp->Cal();
        return RetVal(val);
    }
    else
        return RetVal();
}

vector<int> ConstInitValAST::Init(vector<int> shape) const
{
    vector<int> vec;
    int n = shape.size();

    vector<int> width(shape);

    for (int i = n - 2; i >= 0; --i)
    {
        width[i] *= width[i + 1];
    }

    int pos = 0;
    for (auto &constinitval : constinitvals)
    {
        if (constinitval->derive_type == NUMBER)
        {
            vec.push_back(constinitval->Cal());
            pos++;
        }
        else
        {
            int i = 0;

            if (pos)
            {
                for (i = n - 1; i >= 0; --i)
                {
                    if (pos % width[i])
                        break;
                }
            }
            ++i;

            auto ret_vec = constinitval->Init(vector<int>(shape.begin() + i, shape.end()));
            vec.insert(vec.end(), ret_vec.begin(), ret_vec.end());
            int sz = vec.size();

            pos += width[i];

            int fill_cnt = pos - sz;
            while (fill_cnt--)
                vec.push_back(0);

            assert(vec.size() == pos);
        }

        if (pos >= width[0])
            break;
    }
    for (; pos < width[0]; ++pos)
    {
        vec.push_back(0);
    }
    return vec;
}

RetVal VarDeclAST::Dump() const
{
    for (auto &vardef : vardefs)
        vardef->Dump();
    return RetVal();
}

RetVal VarDefAST::Dump() const
{
    if (derive_type == NUMBER)
    {
        cerr << "//! vardef: " << ident << endl;
        string name = symbol_table.insert(ident, 0, _VAR, _INT);
        if (cur_scope != 0)
        {

            koopa_ret_str += "\t@" + name + " = alloc i32\n";
            if (initval)
            {

                RetVal ret_val = (*initval)->Dump();

                if (ret_val.isNumber())
                {
                    koopa_ret_str += "\tstore " + to_string(ret_val.getVal()) + ", @" + name + "\n";
                }
                else
                {
                    koopa_ret_str += "\tstore %" + to_string(sym_no - 1) + ", @" + name + "\n";
                }
            }
        }
        else
        {
            koopa_ret_str += "global @" + name + " = alloc i32, ";

            if (initval)
            {
                RetVal ret_val = (*initval)->Dump();
                koopa_ret_str += to_string(ret_val.getVal()) + "\n";
            }
            else
                koopa_ret_str += "zeroinit\n";
        }
    }
    else
    {
        cerr << "//! var array def: " << ident << endl;
        vector<int> shape;
        for (auto &constexp : constexps)
        {
            shape.push_back(constexp->Cal());
        }

        string name = symbol_table.insert(ident, shape, VAR_ARRAY, _INT);

        if (cur_scope == 0)
        {
            koopa_ret_str += "global @" + name + " = alloc ";

            for (auto it = shape.begin(); it != shape.end(); it++)
            {
                koopa_ret_str += "[";
            }
            koopa_ret_str += "i32";
            for (auto it = shape.rbegin(); it != shape.rend(); it++)
            {
                koopa_ret_str += ", " + to_string(*it) + "]";
            }
            koopa_ret_str += ", ";
            if (initval)
            {
                auto vec = (*initval)->Init(shape);
                koopa_ret_str += getArrayInitVal(&vec, 0, shape);
            }
            else
                koopa_ret_str += "zeroinit";
            koopa_ret_str += "\n";
        }
        else
        {

            koopa_ret_str += "\t@" + name + " = alloc ";
            for (auto it = shape.begin(); it != shape.end(); it++)
            {
                koopa_ret_str += "[";
            }
            koopa_ret_str += "i32";
            for (auto it = shape.rbegin(); it != shape.rend(); it++)
            {
                koopa_ret_str += ", " + to_string(*it) + "]";
            }
            koopa_ret_str += "\n";

            if (initval)
            {
                auto vec = (*initval)->Init(shape);
                for (auto ele : vec)
                    cerr << ele << " ";
                cerr << endl;
                koopa_ret_str += localArrayInit("@" + name, &vec, 0, shape);
            }
        }
    }

    return RetVal();
}
vector<int> InitValAST::Init(vector<int> shape) const
{
    vector<int> vec;
    int n = shape.size();

    vector<int> width(shape);

    for (int i = n - 2; i >= 0; --i)
    {
        width[i] *= width[i + 1];
    }

    int pos = 0;
    for (auto &initval : initvals)
    {
        if (initval->derive_type == NUMBER)
        {
            vec.push_back(initval->Cal());
            pos++;
        }
        else
        {
            int i = 0;

            if (pos)
            {
                for (i = n - 1; i >= 0; --i)
                {
                    if (pos % width[i])
                        break;
                }
            }
            ++i;

            auto ret_vec = initval->Init(vector<int>(shape.begin() + i, shape.end()));
            vec.insert(vec.end(), ret_vec.begin(), ret_vec.end());
            int sz = vec.size();

            pos += width[i];

            int fill_cnt = pos - sz;
            while (fill_cnt--)
                vec.push_back(0);

            assert(vec.size() == pos);
        }

        if (pos >= width[0])
            break;
    }
    for (; pos < width[0]; ++pos)
    {
        vec.push_back(0);
    }
    return vec;
}

RetVal InitValAST::Dump() const
{
    return expr->Dump();
}

RetVal ConstExpAST::Dump() const
{
    return expr->Dump();
}

int ExprAST::Cal() const
{
    cerr << "exprcal\n";
    return lorexp->Cal();
}

int UnaryExpAST::Cal() const
{
    cerr << "unarycal\n";
    if (derive_type == PRIMARYEXP)
        return primaryexp->Cal();

    if (op == "+")
        return unaryexp->Cal();
    else if (op == "-")
        return -unaryexp->Cal();
    else
        return !unaryexp->Cal();
}
int LValAST::Cal() const
{
    cerr << "lvalcal\n";
    return symbol_table.get(ident).getVal();
}

int PrimaryExpAST::Cal() const
{
    cerr << "primarycal\n";
    if (derive_type == NUMBER)
    {
        cerr << "pri num " << number << endl;
        return number;
    }
    else
        return expr_lval->Cal();
}

int MulExpAST::Cal() const
{
    cerr << "mulcal\n";
    if (derive_type == UNARYEXP)
        return unaryexp->Cal();

    if (op == "*")
        return mulexp->Cal() * unaryexp->Cal();
    else if (op == "/")
        return mulexp->Cal() / unaryexp->Cal();
    else
        return mulexp->Cal() % unaryexp->Cal();
}

int AddExpAST::Cal() const
{
    cerr << "addcal\n";
    if (derive_type == MULEXP)
        return mulexp->Cal();
    if (op == "+")
        return addexp->Cal() + mulexp->Cal();
    else
        return addexp->Cal() - mulexp->Cal();
}

int RelExpAST::Cal() const
{
    cerr << "relcal\n";
    if (derive_type == ADDEXP)
        return addexp->Cal();

    if (op == "<")
        return relexp->Cal() < addexp->Cal();
    else if (op == ">")
        return relexp->Cal() > addexp->Cal();
    else if (op == "<=")
        return relexp->Cal() <= addexp->Cal();
    else
        return relexp->Cal() >= addexp->Cal();
}

int EqExpAST::Cal() const
{
    cerr << "eqcal\n";
    if (derive_type == RELEXP)
        return relexp->Cal();

    if (op == "==")
        return eqexp->Cal() == relexp->Cal();
    else
        return eqexp->Cal() != relexp->Cal();
}

int LAndExpAST::Cal() const
{
    cerr << "landcal\n";
    if (derive_type == EQEXP)
        return eqexp->Cal();

    return landexp->Cal() && eqexp->Cal();
}

int LOrExpAST::Cal() const
{
    cerr << "lorcal\n";
    if (derive_type == LANDEXP)
        return landexp->Cal();

    return lorexp->Cal() || landexp->Cal();
}

int ConstExpAST::Cal() const
{
    return expr->Cal();
}
int InitValAST::Cal() const
{
    if (derive_type == NUMBER)
        return expr->Cal();
    else
        return 0;
}

int ConstInitValAST::Cal() const
{
    if (derive_type == NUMBER)
        return constexp->Cal();
    else
        return 0;
}
