#include <ast.hpp>
#include <unistd.h>
#include "code_gen.hpp"

using namespace std;

// 声明 lexer 的输入, 以及 parser 函数
extern FILE *yyin;
extern FILE *yyout;
extern int yyparse(unique_ptr<BaseAST> &ast);

string koopa_ret_str;
string riscv_ret_str;

int main(int argc, const char *argv[])
{
  // 解析命令行参数. 测试脚本/评测平台要求你的编译器能接收如下参数:
  // compiler 模式 输入文件 -o 输出文件
  assert(argc == 5);
  auto mode = argv[1];
  auto input = argv[2];
  auto output = argv[4];

  // 打开输入文件, 并且指定 lexer 在解析的时候读取这个文件
  yyin = fopen(input, "r");
  assert(yyin);

  yyout = fopen(output, "w");
  assert(yyout);

  dup2(fileno(yyout), STDOUT_FILENO);

  // 调用 parser 函数, parser 函数会进一步调用 lexer 解析输入文件的
  unique_ptr<BaseAST> ast;
  assert(!yyparse(ast));

  // dump AST
  ast->Dump();

  if (!strcmp(mode, "-koopa"))
  {
    cout << koopa_ret_str << endl;
    return 0;
  }

  // 解析字符串 str, 得到 Koopa IR 程序
  koopa_program_t program;
  koopa_error_code_t ret = koopa_parse_from_string(koopa_ret_str.data(), &program);
  assert(ret == KOOPA_EC_SUCCESS); // 确保解析时没有出错
  // 创建一个 raw program builder, 用来构建 raw program
  koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
  // 将 Koopa IR 程序转换为 raw program
  koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
  // 释放 Koopa IR 程序占用的内存
  koopa_delete_program(program);

  // 处理 raw program
  // ...
  Visit(raw);

  // 处理完成, 释放 raw program builder 占用的内存
  // 注意, raw program 中所有的指针指向的内存均为 raw program builder 的内存
  // 所以不要在 raw program 处理完毕之前释放 builder
  koopa_delete_raw_program_builder(builder);
  cout << riscv_ret_str << endl;

  return 0;
}
