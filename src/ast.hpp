#pragma once

#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <string.h>

using namespace std;

extern string koopa_ret_str;
extern int sym_no;

class RetVal
{
    bool is_number;
    int number;
    bool is_str;
    string str;

public:
    RetVal() : is_number(false), number(0), is_str(false), str("") {}
    RetVal(int n) : is_number(true), number(n), is_str(false), str("") {}
    RetVal(string id) : is_number(false), number(0), is_str(true), str(id) {}
    bool isNumber() { return is_number; }
    int getVal()
    {
        assert(is_number);
        return number;
    }
    bool isStr() { return is_str; }
    string getStr()
    {
        assert(is_str);
        return str;
    }
};

extern unordered_map<string, string> op2ir;

enum SYM_TYPE
{
    _CONST,
    _VAR,
    _FUNC,
    CONST_ARRAY,
    VAR_ARRAY
};
enum DATA_TYPE
{
    _INT,
    _VOID
};

class Symbol
{
    SYM_TYPE symbol_type;
    DATA_TYPE data_type;
    int val;
    string name;

public:
    Symbol() : symbol_type(_CONST), data_type(_INT), val(0), name("") {}
    Symbol(SYM_TYPE s_type, DATA_TYPE d_type, int v, string n) : symbol_type(s_type), data_type(d_type), val(v), name(n) {}
    bool isConst() { return symbol_type == _CONST; }
    SYM_TYPE getSymbolType() { return symbol_type; }
    DATA_TYPE getDataType() { return data_type; }
    int getVal() { return val; }
    string getName() { return name; }
};

class SymbolTable
{
    unordered_map<string, Symbol> table;
    bool for_alloc_param = false;
    unordered_map<string, vector<int>> array_shape_table;

public:
    SymbolTable() : for_alloc_param(false) {}
    SymbolTable(bool is_for_alloc) : for_alloc_param(is_for_alloc) {}
    void insert(string id, int val, SYM_TYPE s_type, DATA_TYPE d_type, string name);
    void insert(string id, vector<int> shape, SYM_TYPE s_type, DATA_TYPE d_type, string name);
    bool exist(string id);
    Symbol get(string id);
    vector<int> getArray(string id);
    void common() { for_alloc_param = false; }
    bool forAllocParam() { return for_alloc_param; }

    void debug()
    {
        cout << "Symbol Table:\n";
        for (auto it : table)
        {
            cout << it.first << " " << it.second.getVal() << " " << it.second.getName() << endl;
        }
    }
};

class SymbolTableStack
{
private:
    // 下一个可用的变量dep_label;
    int dep_label_no = 0;
    vector<SymbolTable> stk;

public:
    SymbolTableStack();
    // 增加一个符号表;
    bool inc();
    // 删除一个符号表;
    void dec();
    void incParam();
    string insert(string id, int val, SYM_TYPE s_type, DATA_TYPE d_type);
    string insert(string id, vector<int> shape, SYM_TYPE s_type, DATA_TYPE d_type);
    bool exist(string id);
    Symbol get(string id);
    vector<int> getArray(string id);
    Symbol getFromGlobal(string id);
    int getDepLabelNo() { return dep_label_no; }
};

// 所有 AST 的基类
class BaseAST
{
public:
    int derive_type;
    virtual ~BaseAST() = default;

    virtual RetVal Dump() const = 0;

    virtual int Cal() const = 0;
};
class StartSymbolAST : public BaseAST
{
public:
    unique_ptr<BaseAST> compunit;
    RetVal Dump() const override;
    int Cal() const override { return 0; }
};

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST
{

    enum TYPE
    {
        FUNCDEF,
        DECL
    };

public:
    // 用智能指针管理对象
    optional<unique_ptr<BaseAST>> compunit;
    unique_ptr<BaseAST> funcdef_decl;
    CompUnitAST(unique_ptr<BaseAST> &c, unique_ptr<BaseAST> &_, int type) : funcdef_decl(move(_))
    {
        compunit = make_optional<unique_ptr<BaseAST>>(move(c));
        if (type == 0)
            derive_type = FUNCDEF;
        else
            derive_type = DECL;
    }

    CompUnitAST(unique_ptr<BaseAST> &_, int type) : funcdef_decl(move(_))
    {
        if (type == 0)
            derive_type = FUNCDEF;
        else
            derive_type = DECL;
    }

    RetVal Dump() const override;

    int Cal() const override { return 0; }
};

class FuncFParamsAST;

// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST
{
public:
    string functype;
    string ident;
    optional<unique_ptr<FuncFParamsAST>> params;
    unique_ptr<BaseAST> block;

    RetVal Dump() const override;
    int Cal() const override { return 0; }
};

class FuncFParamAST;

class FuncFParamsAST : public BaseAST
{

public:
    vector<unique_ptr<FuncFParamAST>> funcfparams;

    RetVal Dump() const override;
    int Cal() const override { return 0; }

    RetVal Alloc() const;
};

class FuncFParamAST : public BaseAST
{
    enum TYPE
    {
        NUMBER,
        ARRAY
    };

public:
    string btype;
    string ident;
    vector<unique_ptr<BaseAST>> constexps;

    FuncFParamAST(int type)
    {
        if (type == 0)
            derive_type = NUMBER;
        else
            derive_type = ARRAY;
    }

    RetVal Dump() const override;
    int Cal() const override
    {
        return 0;
    }
    RetVal Alloc() const;
};

class BlockAST : public BaseAST
{
public:
    vector<unique_ptr<BaseAST>> blockitems;

    RetVal Dump() const override;
    int Cal() const override { return 0; }
};

class BlockItemAST : public BaseAST
{
    enum TYPE
    {
        DECL,
        STMT
    };

public:
    unique_ptr<BaseAST> ds;

    BlockItemAST(unique_ptr<BaseAST> &_ds, bool is_decl) : ds(move(_ds))
    {
        if (is_decl)
            derive_type = DECL;
        else
            derive_type = STMT;
    }

    RetVal Dump() const override;
    int Cal() const override { return 0; }
};

class LValAST : public BaseAST
{
    enum TYPE
    {
        NUMBER,
        ARRAY
    };

public:
    string ident;
    vector<unique_ptr<BaseAST>> exprs;
    LValAST(int type)
    {
        if (type == 0)
            derive_type = NUMBER;
        else
            derive_type = ARRAY;
    }

    RetVal Dump() const override;
    int Cal() const override;
};

class StmtAST : public BaseAST
{
    enum TYPE
    {
        ASSIGN,
        RETURN,
        EXPR,
        BLOCK
    };

public:
    unique_ptr<BaseAST> lval;
    unique_ptr<BaseAST> rexpr;
    optional<unique_ptr<BaseAST>>
        expr;
    unique_ptr<BaseAST> block;

    StmtAST(unique_ptr<BaseAST> &_lval, unique_ptr<BaseAST> &_expr) : lval(move(_lval)), rexpr(move(_expr))
    {
        derive_type = ASSIGN;
    }
    // type: RETURN 1, EXPR 2, BLOCK 3
    StmtAST(unique_ptr<BaseAST> &_, int type)
    {
        if (type == 1)
        {
            expr = make_optional(move(_));
            derive_type = RETURN;
        }
        else if (type == 2)
        {
            expr = make_optional(move(_));
            derive_type = EXPR;
        }
        else if (type == 3)
        {
            block = move(_);
            derive_type = BLOCK;
        }
        else
            assert(0);
    }
    // type: RETURN 1, EXPR 2
    StmtAST(int type)
    {
        if (type == 1)
            derive_type = RETURN;
        else if (type == 2)
            derive_type = EXPR;
        else
            assert(0);
    }

    RetVal Dump() const override;
    int Cal() const override { return 0; }
};

class IfStmtAST : public BaseAST
{
    enum TYPE
    {
        NOELSE,
        ELSE
    };

public:
    unique_ptr<BaseAST> expr;
    unique_ptr<BaseAST> ifstmt;
    unique_ptr<BaseAST> elsestmt;

    IfStmtAST(unique_ptr<BaseAST> &_expr, unique_ptr<BaseAST> &_if) : expr(move(_expr)), ifstmt(move(_if)) { derive_type = NOELSE; }
    IfStmtAST(unique_ptr<BaseAST> &_expr, unique_ptr<BaseAST> &_if, unique_ptr<BaseAST> &_else) : expr(move(_expr)), ifstmt(move(_if)), elsestmt(move(_else)) { derive_type = ELSE; }

    RetVal Dump() const override;

    int Cal() const override { return 0; }
};

class WhileStmtAST : public BaseAST
{
public:
    unique_ptr<BaseAST> expr;
    unique_ptr<BaseAST> whilestmt;

    WhileStmtAST(unique_ptr<BaseAST> &_expr, unique_ptr<BaseAST> &_while) : expr(move(_expr)), whilestmt(move(_while)) {}

    RetVal Dump() const override;

    int Cal() const override { return 0; }
};

class BrConStmtAST : public BaseAST
{
    enum TYPE
    {
        BREAK,
        CONTINUE
    };

public:
    BrConStmtAST(int type)
    {
        if (type == 0)
            derive_type = BREAK;
        else
            derive_type = CONTINUE;
    }

    RetVal Dump() const override;

    int Cal() const override { return 0; }
};

class ExprAST : public BaseAST
{
public:
    unique_ptr<BaseAST> lorexp;

    RetVal Dump() const override;

    int Cal() const override;
};

class UnaryExpAST : public BaseAST
{
    enum TYPE
    {
        PRIMARYEXP,
        UNARYEXP
    };

public:
    unique_ptr<BaseAST> primaryexp;
    string op;
    unique_ptr<BaseAST> unaryexp;

    UnaryExpAST(unique_ptr<BaseAST> &primary) : primaryexp(move(primary)) { derive_type = PRIMARYEXP; }
    UnaryExpAST(string *_op, unique_ptr<BaseAST> &unary) : op(*move(_op)), unaryexp(move(unary)) { derive_type = UNARYEXP; }

    RetVal Dump() const override;
    int Cal() const override;
};

class FuncRParamsAST;

class FuncUnaryExpAST : public BaseAST
{

public:
    string ident;
    optional<unique_ptr<FuncRParamsAST>> params;
    RetVal Dump() const override;

    int Cal() const override { return 0; }
};

class FuncRParamsAST : public BaseAST
{
public:
    vector<unique_ptr<BaseAST>> funcrparams;

    RetVal Dump() const override;
    int Cal() const override { return 0; }

    vector<string> Alloc() const;
};

class PrimaryExpAST : public BaseAST
{
    enum TYPE
    {
        EXPR,
        LVAL,
        NUMBER
    };

public:
    unique_ptr<BaseAST> expr_lval;

    int number;

    PrimaryExpAST(unique_ptr<BaseAST> &_el, bool is_lval) : expr_lval(move(_el))
    {
        if (!is_lval)
            derive_type = EXPR;
        else
            derive_type = LVAL;
    }

    PrimaryExpAST(int number) : number(number) { derive_type = NUMBER; }

    RetVal Dump() const override;
    int Cal() const override;
};

class MulExpAST : public BaseAST
{
    enum TYPE
    {
        UNARYEXP, // MulExp::= UnaryExp
        MULEXP    // MulExp::= MulExp ("*" | "/" | "%") UnaryExp;
    };

public:
    unique_ptr<BaseAST> mulexp;
    string op;
    unique_ptr<BaseAST> unaryexp;

    MulExpAST(unique_ptr<BaseAST> &unary) : unaryexp(move(unary)) { derive_type = UNARYEXP; }
    MulExpAST(unique_ptr<BaseAST> &mul, string *_op, unique_ptr<BaseAST> &unary) : mulexp(move(mul)), op(*move(_op)), unaryexp(move(unary)) { derive_type = MULEXP; }

    RetVal Dump() const override;
    int Cal() const override;
};

class AddExpAST : public BaseAST
{
    enum TYPE
    {
        MULEXP, // AddExp::= MulExp;
        ADDEXP  // AddExp::= AddExp ("+" | "-") MulExp;
    };

public:
    unique_ptr<BaseAST> addexp;
    string op;
    unique_ptr<BaseAST> mulexp;

    AddExpAST(unique_ptr<BaseAST> &mul) : mulexp(move(mul)) { derive_type = MULEXP; }
    AddExpAST(unique_ptr<BaseAST> &add, string *_op, unique_ptr<BaseAST> &mul) : addexp(move(add)), op(*move(_op)), mulexp(move(mul)) { derive_type = ADDEXP; }

    RetVal Dump() const override;
    int Cal() const override;
};

class RelExpAST : public BaseAST
{
    enum TYPE
    {
        ADDEXP, // RelExp::= AddExp;
        RELEXP  // RelExp::= RelExp ("<" | ">" | "<=" | ">=") AddExp;
    };

public:
    unique_ptr<BaseAST> relexp;
    string op;
    unique_ptr<BaseAST> addexp;

    RelExpAST(unique_ptr<BaseAST> &add) : addexp(move(add)) { derive_type = ADDEXP; }
    RelExpAST(unique_ptr<BaseAST> &rel, string *_op, unique_ptr<BaseAST> &add) : relexp(move(rel)), op(*move(_op)), addexp(move(add)) { derive_type = RELEXP; }

    RetVal Dump() const override;
    int Cal() const override;
};

class EqExpAST : public BaseAST
{
    enum TYPE
    {
        RELEXP, // EqExp::= RelExp
        EQEXP   // EqExp::= EqExp ("==" | "!=") RelExp;
    };

public:
    unique_ptr<BaseAST> eqexp;
    string op;
    unique_ptr<BaseAST> relexp;

    EqExpAST(unique_ptr<BaseAST> &rel) : relexp(move(rel)) { derive_type = RELEXP; }
    EqExpAST(unique_ptr<BaseAST> &eq, string *_op, unique_ptr<BaseAST> &rel) : eqexp(move(eq)), op(*move(_op)), relexp(move(rel)) { derive_type = EQEXP; }

    RetVal Dump() const override;
    int Cal() const override;
};

class LAndExpAST : public BaseAST
{
    enum TYPE
    {
        EQEXP,  // LAndExp::= EqExp;
        LANDEXP // LAndExp::= LAndExp "&&" EqExp;
    };

public:
    unique_ptr<BaseAST> landexp;
    string op;
    unique_ptr<BaseAST> eqexp;

    LAndExpAST(unique_ptr<BaseAST> &eq) : eqexp(move(eq)) { derive_type = EQEXP; }
    LAndExpAST(unique_ptr<BaseAST> &land, string *_op, unique_ptr<BaseAST> &eq) : landexp(move(land)), op(*move(_op)), eqexp(move(eq)) { derive_type = LANDEXP; }

    RetVal Dump() const override;
    int Cal() const override;
};

class LOrExpAST : public BaseAST
{
    enum TYPE
    {
        LANDEXP, // LOrExp::= LAndExp;
        LOREXP   // LOrExp::= LOrExp "||" LAndExp;
    };

public:
    unique_ptr<BaseAST> lorexp;
    string op;
    unique_ptr<BaseAST> landexp;

    LOrExpAST(unique_ptr<BaseAST> &land) : landexp(move(land)) { derive_type = LANDEXP; }
    LOrExpAST(unique_ptr<BaseAST> &lor, string *_op, unique_ptr<BaseAST> &land) : lorexp(move(lor)), op(*move(_op)), landexp(move(land)) { derive_type = LOREXP; }

    RetVal Dump() const override;
    int Cal() const override;
};

class DeclAST : public BaseAST
{
    enum
    {
        CONSTDECL,
        VARDECL
    };

public:
    unique_ptr<BaseAST> cvdecl;

    DeclAST(unique_ptr<BaseAST> &cv, bool is_const) : cvdecl(move(cv))
    {
        if (is_const)
            derive_type = CONSTDECL;
        else
            derive_type = VARDECL;
    }
    RetVal Dump() const override;
    int Cal() const override { return 0; }
};

class ConstDeclAST : public BaseAST
{
public:
    string btype;
    vector<unique_ptr<BaseAST>> constdefs;

    RetVal Dump() const override;
    int Cal() const override { return 0; }
};

class VarDeclAST : public BaseAST
{
public:
    string btype;
    vector<unique_ptr<BaseAST>> vardefs;

    RetVal Dump() const override;
    int Cal() const override { return 0; }
};

class ConstInitValAST;

class ConstDefAST : public BaseAST
{
    enum TYPE
    {
        NUMBER,
        ARRAY
    };

public:
    string ident;
    vector<unique_ptr<BaseAST>> constexps;
    unique_ptr<ConstInitValAST> constinitval;

    ConstDefAST(int type)
    {
        if (type == 0)
            derive_type = NUMBER;
        else
            derive_type = ARRAY;
    }

    RetVal Dump() const override;
    int Cal() const override { return 0; }
};

class InitValAST;

class VarDefAST : public BaseAST
{
    enum TYPE
    {
        NUMBER,
        ARRAY
    };

public:
    string ident;
    vector<unique_ptr<BaseAST>> constexps;
    optional<unique_ptr<InitValAST>> initval;

    VarDefAST(string *id, int type) : ident(*move(id))
    {
        if (type == 0)
            derive_type = NUMBER;
        else
            derive_type = ARRAY;
    }
    VarDefAST(string *id, unique_ptr<InitValAST> &init, int type) : ident(*move(id)), initval(make_optional(move(init)))
    {
        if (type == 0)
            derive_type = NUMBER;
        else
            derive_type = ARRAY;
    }
    RetVal Dump() const override;
    int Cal() const override { return 0; }
};

class ConstInitValAST : public BaseAST
{
    enum TYPE
    {
        NUMBER,
        ARRAY
    };

public:
    unique_ptr<BaseAST> constexp;
    vector<unique_ptr<ConstInitValAST>> constinitvals;

    ConstInitValAST(int type)
    {

        if (type == 0)
            derive_type = NUMBER;
        else
            derive_type = ARRAY;
    }

    RetVal Dump() const override;
    int Cal() const override;
    vector<int> Init(vector<int> shape) const;
};

class InitValAST : public BaseAST
{
    enum TYPE
    {
        NUMBER,
        ARRAY
    };

public:
    unique_ptr<BaseAST> expr;
    vector<unique_ptr<InitValAST>> initvals;

    InitValAST(int type)
    {

        if (type == 0)
            derive_type = NUMBER;
        else
            derive_type = ARRAY;
    }

    RetVal Dump() const override;
    int Cal() const override;
    vector<int> Init(vector<int> shape) const;
};

class ConstExpAST : public BaseAST
{
public:
    unique_ptr<BaseAST> expr;

    RetVal Dump() const override;
    int Cal() const override;
};
