#include "exprPtr.h"
#include "../arithmetic/unaryOpExpr.h"
#include "aliasExpr.h"

ExprPtr ExprPtr::abs() const
{
  return ExprPtr(std::make_shared<UnaryOpExpr>(expr_, UnaryOp::ABS));
}
ExprPtr ExprPtr::is_null() const
{
  return ExprPtr(std::make_shared<UnaryOpExpr>(expr_, UnaryOp::IS_NULL));
}
ExprPtr ExprPtr::is_not_null() const
{
  return ExprPtr(std::make_shared<UnaryOpExpr>(expr_, UnaryOp::IS_NOT_NULL));
}
ExprPtr ExprPtr::alias(const std::string &name) const
{
  return ExprPtr(std::make_shared<AliasExpr>(expr_, name));
}