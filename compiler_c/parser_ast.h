
#include "ast.h"

void debug_log_parser(const char * fmt, ...) ;

bool is_comparison_operator(int op);

AST_node *parse_program_ast();
