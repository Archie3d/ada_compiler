#include "Ast.h"

BlockStmt::BlockStmt()
    : Stmt(StmtKind::Block)
{
}

BlockStmt::~BlockStmt() = default;
