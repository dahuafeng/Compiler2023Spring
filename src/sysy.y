%code requires {
  #include <memory>
  #include <string>
  #include "ast.hpp"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include "ast.hpp"

// 声明 lexer 函数和错误处理函数
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

// 定义 parser 函数和错误处理函数的附加参数
// 我们需要返回一个字符串作为 AST, 所以我们把附加参数定义成字符串的智能指针
// 解析完成后, 我们要手动修改这个参数, 把它设置成解析得到的字符串
%parse-param { std::unique_ptr<BaseAST> &ast }


// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 至于为什么要用字符串指针而不直接用 string 或者 unique_ptr<string>?
// 请自行 STFW 在 union 里写一个带析构函数的类会出现什么情况
%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
  std::vector<std::unique_ptr<BaseAST>> *vec_val;
  std::vector<std::unique_ptr<FuncFParamAST>> *funcf_vec_val;
  std::vector<std::unique_ptr<ConstInitValAST>> *const_init_vec_val;
  std::vector<std::unique_ptr<InitValAST>> *init_vec_val;
}



// lexer 返回的所有 token 种类的声明
// 注意 IDENT 和 INT_CONST 会返回 token 的值, 分别对应 str_val 和 int_val
%token INT RETURN CONST IF ELSE WHILE BREAK CONTINUE VOID
%token <str_val> IDENT LEQ GEQ EQ NEQ LAND LOR
%token <int_val> INT_CONST

// 非终结符的类型定义
%type <ast_val> CompUnit FuncDef Block Stmt Expr PrimaryExp UnaryExp MulExp AddExp RelExp EqExp LAndExp LOrExp 
%type <ast_val> BlockItem LVal Decl ConstDecl ConstDef ConstInitVal VarDecl VarDef InitVal ConstExp
%type <ast_val> FuncFParams FuncFParam  FuncRParams 
%type <int_val> Number
%type <str_val> UnaryOp
%type <vec_val> BlockItemList ConstDefList VarDefList ExprList ConstExpList ArrayExpList
%type <funcf_vec_val> FuncFParamList 
%type <const_init_vec_val> ConstInitValList
%type <init_vec_val> InitValList

%%

// 开始符, CompUnit ::= FuncDef, 大括号后声明了解析完成后 parser 要做的事情
// 之前我们定义了 FuncDef 会返回一个 str_val, 也就是字符串指针
// 而 parser 一旦解析完 CompUnit, 就说明所有的 token 都被解析了, 即解析结束了
// 此时我们应该把 FuncDef 返回的结果收集起来, 作为 AST 传给调用 parser 的函数
// $1 指代规则里第一个符号的返回值, 也就是 FuncDef 的返回值
StartSymbol
  : CompUnit {
    std::cerr<<"//! start\n";
    auto start_symbol = make_unique<StartSymbolAST>();
    start_symbol->compunit = unique_ptr<BaseAST>($1);
    ast = move(start_symbol);
  }
  ;

CompUnit
  : FuncDef {
    std::cerr<<"//! compunit->funcdef\n";
    auto func_def = unique_ptr<BaseAST>($1);
    auto ast = new CompUnitAST(func_def, 0);
    $$ = ast;
  }
  | Decl {
    std::cerr<<"//! compunit->decl\n";
    auto decl = unique_ptr<BaseAST>($1);
    auto ast = new CompUnitAST(decl, 1);
    $$ = ast;
  }
  | CompUnit FuncDef {
    std::cerr<<"//! compunit->compunit funcdef\n";
    auto compunit = unique_ptr<BaseAST>($1);
    auto func_def = unique_ptr<BaseAST>($2);
    auto ast = new CompUnitAST(compunit, func_def, 0);
    $$ = ast;
  }
  | CompUnit Decl {
    std::cerr<<"//! compunit->compunit decl\n";
    auto compunit = unique_ptr<BaseAST>($1);
    auto decl = unique_ptr<BaseAST>($2);
    auto ast = new CompUnitAST(compunit, decl, 1);
    $$ = ast;
  }
  ;

// FuncDef ::= FuncType IDENT '(' ')' Block;
// 我们这里可以直接写 '(' 和 ')', 因为之前在 lexer 里已经处理了单个字符的情况
// 解析完成后, 把这些符号的结果收集起来, 然后拼成一个新的字符串, 作为结果返回
// $$ 表示非终结符的返回值, 我们可以通过给这个符号赋值的方法来返回结果
// 你可能会问, FuncType, IDENT 之类的结果已经是字符串指针了
// 为什么还要用 unique_ptr 接住它们, 然后再解引用, 把它们拼成另一个字符串指针呢
// 因为所有的字符串指针都是我们 new 出来的, new 出来的内存一定要 delete
// 否则会发生内存泄漏, 而 unique_ptr 这种智能指针可以自动帮我们 delete
// 虽然此处你看不出用 unique_ptr 和手动 delete 的区别, 但当我们定义了 AST 之后
// 这种写法会省下很多内存管理的负担
FuncDef
  : INT IDENT '(' ')' Block {
    auto ast = new FuncDefAST();
    ast->functype = "int";
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | INT IDENT '(' FuncFParams ')' Block {
    auto ast = new FuncDefAST();
    ast->functype = "int";
    ast->ident = *unique_ptr<string>($2);
    ast->params = make_optional(unique_ptr<FuncFParamsAST>(dynamic_cast<FuncFParamsAST*>($4)));
    ast->block = unique_ptr<BaseAST>($6);
    $$ = ast;
  }
  | VOID IDENT '(' ')' Block {
    auto ast = new FuncDefAST();
    ast->functype = "void";
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | VOID IDENT '(' FuncFParams ')' Block {
    auto ast = new FuncDefAST();
    ast->functype = "void";
    ast->ident = *unique_ptr<string>($2);
    ast->params = make_optional(unique_ptr<FuncFParamsAST>(dynamic_cast<FuncFParamsAST*>($4)));
    ast->block = unique_ptr<BaseAST>($6);
    $$ = ast;
  }
  ;


// 同上, 不再解释


FuncFParams
  : FuncFParamList {
    auto ast = new FuncFParamsAST();
    auto vec = ($1);
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->funcfparams.push_back(move(*it));
    $$ = ast;
  }
  ;

FuncFParamList
  : FuncFParam {
    auto vec = new vector<unique_ptr<FuncFParamAST>>;
    auto param = unique_ptr<FuncFParamAST>(dynamic_cast<FuncFParamAST*>($1));
    vec->push_back(move(param));
    $$ = vec;
  }
  | FuncFParamList ',' FuncFParam {
    auto vec = ($1);
    auto param = unique_ptr<FuncFParamAST>(dynamic_cast<FuncFParamAST*>($3));
    vec->push_back(move(param));
    $$ = vec;
  }
  ;

FuncFParam
  : INT IDENT {
    auto ast = new FuncFParamAST(0);
    ast->btype = "int";
    ast->ident = *unique_ptr<string>($2);
    $$ = ast;
  }
  | INT IDENT '[' ']' {
    auto ast = new FuncFParamAST(1);
    ast->btype = "int";
    ast->ident = *unique_ptr<string>($2);
    $$ = ast;
  }
  | INT IDENT '[' ']' ConstExpList{
    auto ast = new FuncFParamAST(1);
    ast->btype = "int";
    ast->ident = *unique_ptr<string>($2);
    auto vec = $5;
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->constexps.push_back(move(*it));
    $$ = ast;
  }
  ;

Block
  : '{' BlockItemList '}' {
    auto ast = new BlockAST();
    auto vec = ($2);
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->blockitems.push_back(move(*it));
    $$ = ast;
  }
  ;

BlockItemList
  : {
    auto vec = new vector<unique_ptr<BaseAST>>;
    $$ = vec;
  }
  | BlockItemList BlockItem {
    auto vec = ($1);
    vec->push_back(unique_ptr<BaseAST>($2));
    $$ = vec;
  }
  ;

BlockItem
  : Decl {
    auto decl = unique_ptr<BaseAST>($1);
    auto ast = new BlockItemAST(decl, true);
    $$ = ast;
  }
  | Stmt {  
    auto stmt = unique_ptr<BaseAST>($1);
    auto ast = new BlockItemAST(stmt, false);
    $$ = ast;
  }
  ;

Stmt
  : LVal '=' Expr ';' {
    auto lval = unique_ptr<BaseAST>($1);
    auto expr = unique_ptr<BaseAST>($3);
    auto ast = new StmtAST(lval, expr);
    $$ = ast;
  }
  | ';' {
    auto ast = new StmtAST(2);
    $$ = ast;
  }
  | Expr ';' {
    auto expr = unique_ptr<BaseAST>($1);
    auto ast = new StmtAST(expr, 2);
    $$ = ast;
  }
  | Block {
    auto block = unique_ptr<BaseAST>($1);
    auto ast = new StmtAST(block, 3);
    $$ = ast;
  }
  | RETURN ';' {
    auto ast = new StmtAST(1);
    $$ = ast;
  }
  | RETURN Expr ';' {
    auto expr = unique_ptr<BaseAST>($2);
    auto ast = new StmtAST(expr, 1);
    $$ = ast;
  }
  | IF '(' Expr ')' Stmt {
    auto expr = unique_ptr<BaseAST>($3);
    auto ifstmt = unique_ptr<BaseAST>($5);
    auto ast = new IfStmtAST(expr, ifstmt);
    $$ = ast;
  }
  | IF '(' Expr ')' Stmt ELSE Stmt {
    auto expr = unique_ptr<BaseAST>($3);
    auto ifstmt = unique_ptr<BaseAST>($5);
    auto elsestmt = unique_ptr<BaseAST>($7);
    auto ast = new IfStmtAST(expr, ifstmt, elsestmt);
    $$ = ast;

  }
  | WHILE '(' Expr ')' Stmt{
    auto expr = unique_ptr<BaseAST>($3);
    auto whilestmt = unique_ptr<BaseAST>($5);
    auto ast = new WhileStmtAST(expr, whilestmt);
    $$ = ast;
  }
  | BREAK ';' {
    auto ast = new BrConStmtAST(0);
    $$ = ast;
  }
  | CONTINUE ';' {
    auto ast = new BrConStmtAST(1);
    $$ = ast;
  }
  ;


Decl
  : ConstDecl {
    cerr<<"//! decl->constdecl\n";
    auto constdecl = unique_ptr<BaseAST>($1);
    auto ast = new DeclAST(constdecl, true);
    $$ = ast;
  }
  | VarDecl {
    cerr<<"//! decl->vardecl\n";
    auto vardecl = unique_ptr<BaseAST>($1);
    auto ast = new DeclAST(vardecl, false);
    $$ = ast;
  }
  ;

ConstDecl
  : CONST INT ConstDefList ';' {
    auto ast = new ConstDeclAST();
    ast->btype = "int";
    auto vec = ($3); 
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->constdefs.push_back(move(*it));
    $$ = ast;
  }
  ;

ConstDefList
  : ConstDef {
    auto vec = new vector<unique_ptr<BaseAST>>;
    auto constdef = unique_ptr<BaseAST>($1);
    vec->push_back(move(constdef));
    $$ = vec;
  }
  | ConstDefList ',' ConstDef {
    auto vec = $1;
    auto constdef = unique_ptr<BaseAST>($3);
    vec->push_back(move(constdef));
    $$ = vec;
  }
  ;

ConstDef
  : IDENT '=' ConstInitVal {
    auto ast = new ConstDefAST(0);
    ast->ident = *unique_ptr<string>($1);
    ast->constinitval=unique_ptr<ConstInitValAST>(dynamic_cast<ConstInitValAST*>($3));
    $$ = ast;
  }
  | IDENT ConstExpList '=' ConstInitVal {
    auto ast = new ConstDefAST(1);
    ast->ident = *unique_ptr<string>($1);
    ast->constinitval=unique_ptr<ConstInitValAST>(dynamic_cast<ConstInitValAST*>($4));
    auto vec = ($2);
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->constexps.push_back(move(*it));
    $$ = ast;
  }
  ;

ConstExpList
  : '[' ConstExp ']' {
    auto vec = new vector<unique_ptr<BaseAST>>;
    auto constexp = unique_ptr<BaseAST>($2);
    vec->push_back(move(constexp));
    $$ = vec;
  }
  | ConstExpList '[' ConstExp ']' {
    auto vec = $1;
    auto constexp = unique_ptr<BaseAST>($3);
    vec->push_back(move(constexp));
    $$ = vec;
  }
  ;


ConstInitVal
  : ConstExp {
    auto ast = new ConstInitValAST(0);
    ast->constexp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | '{' '}'{
    auto ast = new ConstInitValAST(1);
    $$ = ast;
  }
  | '{' ConstInitValList '}' {
    auto ast = new ConstInitValAST(1);
    auto vec = ($2);
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->constinitvals.push_back(move(*it));
    $$ = ast;
  }
  ;

ConstInitValList
  : ConstInitVal {
    auto constinitval = unique_ptr<ConstInitValAST>(dynamic_cast<ConstInitValAST*>($1));
    auto vec = new vector<unique_ptr<ConstInitValAST>>;
    vec->push_back(move(constinitval));
    $$ = vec;
  }
  | ConstInitValList ',' ConstInitVal {
    auto vec = $1;
    auto constinitval = unique_ptr<ConstInitValAST>(dynamic_cast<ConstInitValAST*>($3));
    vec->push_back(move(constinitval));
    $$ = vec;
  }
  ;

VarDecl
  : INT VarDefList ';' {
    cerr<<"//! vardecl->vardeflist\n";
    auto ast = new VarDeclAST();
    ast->btype = "int";
    auto vec = ($2); 
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->vardefs.push_back(move(*it));
    $$ = ast;
  }
  ;

VarDefList
  : VarDef {
    cerr<<"//! vardeflist->vardef\n";
    auto vec = new vector<unique_ptr<BaseAST>>;
    auto vardef = unique_ptr<BaseAST>($1);
    vec->push_back(move(vardef));
    $$ = vec;
  }
  | VarDefList ',' VarDef {
    cerr<<"//! vardeflist->vardeflist vardef\n";
    auto vec = $1;
    auto vardef = unique_ptr<BaseAST>($3);
    vec->push_back(move(vardef));
    $$ = vec;
  }
  ;

VarDef
  : IDENT {
    cerr<<"//! vardef-> ident\n";
    auto ident = ($1);
    auto ast = new VarDefAST(ident, 0);
    $$ = ast;
  }
  | IDENT '=' InitVal {
    cerr<<"//! vardef -> ident=init\n";
    auto ident = ($1);
    auto initval = unique_ptr<InitValAST>(dynamic_cast<InitValAST*>($3));
    auto ast = new VarDefAST(ident, initval, 0);
    $$ = ast;
  }
  | IDENT ConstExpList {
    cerr<<"//! vardef -> ident[]\n";
    auto ident = ($1);
    auto ast = new VarDefAST(ident, 1);
    auto vec = ($2);
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->constexps.push_back(move(*it));
    $$ = ast;
  }
  | IDENT ConstExpList '=' InitVal
  {
    cerr<<"//! vardef -> ident[]=init\n";
    auto ident = ($1);
    auto initval = unique_ptr<InitValAST>(dynamic_cast<InitValAST*>($4));
    auto ast = new VarDefAST(ident, initval, 1);
    auto vec = ($2);
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->constexps.push_back(move(*it));
    $$ = ast;
  }
  ;

InitVal
  : Expr {
    cerr<<"//! init->exp\n";
    auto ast = new InitValAST(0);
    ast->expr = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  |'{' '}'{
    auto ast = new InitValAST(1);
    $$ = ast;
  }
  | '{' InitValList '}' {
    cerr<<"//! init -> initlist\n";
    auto ast = new InitValAST(1);
    auto vec = ($2);
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->initvals.push_back(move(*it));
    $$ = ast;
  }
  ;

InitValList
  : InitVal {
    cerr<<"//! initlist-> initval\n";
    auto vec = new vector<unique_ptr<InitValAST>>;
    auto initval = unique_ptr<InitValAST>(dynamic_cast<InitValAST*>($1));
    vec->push_back(move(initval));
    $$ = vec;
  }
  | InitValList ',' InitVal {
    cerr<<"//! initlist-> initlist\n";
    auto vec = $1;
    auto initval = unique_ptr<InitValAST>(dynamic_cast<InitValAST*>($3));
    vec->push_back(move(initval));
    $$ = vec;
  }
  ;


ConstExp
  : Expr {
    auto ast = new ConstExpAST();
    ast->expr = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

Number
  : INT_CONST {
    $$ = (int)($1);
  }
  ;

Expr
  : LOrExp{
    auto ast = new ExprAST();
    ast->lorexp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

PrimaryExp
  : '(' Expr ')'  {
    auto expr = unique_ptr<BaseAST>($2);
    auto ast = new PrimaryExpAST(expr, 0);
    $$ = ast;
  }
  | Number{
    auto number = (int)($1);
    auto ast = new PrimaryExpAST(number);
    $$ = ast;
  }
  | LVal {
    auto lval = unique_ptr<BaseAST>($1);
    auto ast = new PrimaryExpAST(lval, 1);
    $$ = ast;
  }
  ;

LVal
  : IDENT {
    auto ast = new LValAST(0);
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT ArrayExpList {
    auto ast = new LValAST(1);
    ast->ident = *unique_ptr<string>($1);
    auto vec = ($2);
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->exprs.push_back(move(*it));
    $$ = ast;
  }
  ;

ArrayExpList
  : '[' Expr ']' {
    auto vec = new vector<unique_ptr<BaseAST>>;
    auto expr = unique_ptr<BaseAST>($2);
    vec->push_back(move(expr));
    $$ = vec;
  }
  | ArrayExpList '[' Expr ']' {
    auto vec = $1;
    auto expr = unique_ptr<BaseAST>($3);
    vec->push_back(move(expr));
    $$ = vec;
  }
  ;

UnaryExp
  : PrimaryExp {
    auto primary = unique_ptr<BaseAST>($1);
    auto ast = new UnaryExpAST(primary);
    $$ = ast;
  }
  | UnaryOp UnaryExp{
    auto unaryexp = unique_ptr<BaseAST>($2);
    auto ast = new UnaryExpAST($1, unaryexp);
    $$ = ast;
  }
  | IDENT '(' ')'  {
    auto ast = new FuncUnaryExpAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT '(' FuncRParams ')'  {
    auto ast = new FuncUnaryExpAST();
    ast->ident = *unique_ptr<string>($1);
    ast->params = make_optional(unique_ptr<FuncRParamsAST>(dynamic_cast<FuncRParamsAST*>($3)));
    $$ = ast;
  }
  ;

FuncRParams
  : ExprList {
    auto ast = new FuncRParamsAST();
    auto vec = ($1);
    for (auto it = vec->begin(); it != vec->end(); it++)
      ast->funcrparams.push_back(move(*it));
    $$ = ast;
  }
  ;

ExprList
  : Expr {
    auto vec = new vector<unique_ptr<BaseAST>>;
    auto expr = unique_ptr<BaseAST>($1);
    vec->push_back(move(expr));
    $$ = vec;
  }
  | ExprList ',' Expr {
    auto vec = $1;
    auto expr = unique_ptr<BaseAST>($3);
    vec->push_back(move(expr));
    $$ = vec;
  }
  ;

UnaryOp
  : '+' {
    string *op = new string("+");
    $$ = op;
  }
  | '-' {
    string *op = new string("-");
    $$ = op;
  }
  | '!' {
    string *op = new string("!");
    $$ = op;
  }
  ;

MulExp
  : UnaryExp {
    auto unaryexp = unique_ptr<BaseAST>($1);
    auto ast = new MulExpAST(unaryexp);
    $$ = ast;
  }
  | MulExp '*' UnaryExp {
    auto mulexp = unique_ptr<BaseAST>($1);
    string *op = new string("*");
    auto unaryexp = unique_ptr<BaseAST>($3);
    auto ast = new MulExpAST(mulexp, op, unaryexp);
    $$ = ast;
  }
  | MulExp '/' UnaryExp {
    auto mulexp = unique_ptr<BaseAST>($1);
    string *op = new string("/");
    auto unaryexp = unique_ptr<BaseAST>($3);
    auto ast = new MulExpAST(mulexp, op, unaryexp);
    $$ = ast;
  }
  | MulExp '%' UnaryExp {
    auto mulexp = unique_ptr<BaseAST>($1);
    string *op = new string("%");
    auto unaryexp = unique_ptr<BaseAST>($3);
    auto ast = new MulExpAST(mulexp, op, unaryexp);
    $$ = ast;
  }
  ;

AddExp
  : MulExp {
    auto mulexp = unique_ptr<BaseAST>($1);
    auto ast = new AddExpAST(mulexp);
    $$ = ast;
  }
  | AddExp '+' MulExp {
    auto addexp = unique_ptr<BaseAST>($1);
    string *op = new string("+");
    auto mulexp = unique_ptr<BaseAST>($3);
    auto ast = new AddExpAST(addexp, op, mulexp);
    $$ = ast;
  }
  | AddExp '-' MulExp {
    auto addexp = unique_ptr<BaseAST>($1);
    string *op = new string("-");
    auto mulexp = unique_ptr<BaseAST>($3);
    auto ast = new AddExpAST(addexp, op, mulexp);
    $$ = ast;
  }
  ;

RelExp
  : AddExp {
    auto addexp = unique_ptr<BaseAST>($1);
    auto ast = new RelExpAST(addexp);
    $$ = ast;
  }
  | RelExp '<' AddExp {
    auto relexp = unique_ptr<BaseAST>($1);
    string *op = new string("<");
    auto addexp = unique_ptr<BaseAST>($3);
    auto ast = new RelExpAST(relexp, op, addexp);
    $$ = ast;
  }
  | RelExp '>' AddExp {
    auto relexp = unique_ptr<BaseAST>($1);
    string *op = new string(">");
    auto addexp = unique_ptr<BaseAST>($3);
    auto ast = new RelExpAST(relexp, op, addexp);
    $$ = ast;
  }
  | RelExp LEQ AddExp {
    auto relexp = unique_ptr<BaseAST>($1);
    string *op = new string("<=");
    auto addexp = unique_ptr<BaseAST>($3);
    auto ast = new RelExpAST(relexp, op, addexp);
    $$ = ast;
  }
  | RelExp GEQ AddExp {
    auto relexp = unique_ptr<BaseAST>($1);
    string *op = new string(">=");
    auto addexp = unique_ptr<BaseAST>($3);
    auto ast = new RelExpAST(relexp, op, addexp);
    $$ = ast;
  }
  ;

EqExp
  : RelExp {
    auto relexp = unique_ptr<BaseAST>($1);
    auto ast = new EqExpAST(relexp);
    $$ = ast;
  }
  | EqExp EQ RelExp {
    auto eqexp = unique_ptr<BaseAST>($1);
    string *op = new string("==");
    auto relexp = unique_ptr<BaseAST>($3);
    auto ast = new EqExpAST(eqexp, op, relexp);
    $$ = ast;
  }
  | EqExp NEQ RelExp {
    auto eqexp = unique_ptr<BaseAST>($1);
    string *op = new string("!=");
    auto relexp = unique_ptr<BaseAST>($3);
    auto ast = new EqExpAST(eqexp, op, relexp);
    $$ = ast;
  }
  ;

LAndExp
  : EqExp {
    auto eqexp = unique_ptr<BaseAST>($1);
    auto ast = new LAndExpAST(eqexp);
    $$ = ast;
  }
  | LAndExp LAND EqExp {
    auto landexp = unique_ptr<BaseAST>($1);
    string *op = new string("&&");
    auto eqexp = unique_ptr<BaseAST>($3);
    auto ast = new LAndExpAST(landexp, op, eqexp);
    $$ = ast;
  }
  ;

LOrExp
  : LAndExp {
    auto landexp = unique_ptr<BaseAST>($1);
    auto ast = new LOrExpAST(landexp);
    $$ = ast;
  }
  | LOrExp LOR LAndExp {
    auto lorexp = unique_ptr<BaseAST>($1);
    string *op = new string("||");
    auto landexp = unique_ptr<BaseAST>($3);
    auto ast = new LOrExpAST(lorexp, op, landexp);
    $$ = ast;
  }
  ;
%%

// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s) {
  
    extern int yylineno;    // defined and maintained in lex
    extern char *yytext;    // defined and maintained in lex
    int len=strlen(yytext);
    int i;
    char buf[512]={0};
    for (i=0;i<len;++i)
    {
        sprintf(buf,"%s%d ",buf,yytext[i]);
    }
    fprintf(stderr, "ERROR: %s at symbol '%s' on line %d\n", s, buf, yylineno);

}