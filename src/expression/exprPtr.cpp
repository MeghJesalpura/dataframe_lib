#include "dataframelib/expression/exprPtr.h"
#include "dataframelib/operations/unaryOpExpr.h"
#include "dataframelib/expression/aliasExpr.h"
#include "dataframelib/operations/strUnOpExpr.h"
#include "dataframelib/operations/strBinOpExpr.h"
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
ExprPtr ExprPtr::length() const
{
  return ExprPtr(std::make_shared<StringUnOpExpr>(expr_, StringUnOp::LENGTH));
}
ExprPtr ExprPtr::to_upper() const
{
  return ExprPtr(std::make_shared<StringUnOpExpr>(expr_, StringUnOp::TO_UPPER));
}
ExprPtr ExprPtr::to_lower() const
{
  return ExprPtr(std::make_shared<StringUnOpExpr>(expr_, StringUnOp::TO_LOWER));
}
ExprPtr ExprPtr::contains(const std::string &substr) const
{
  return ExprPtr(std::make_shared<StringBinOpExpr>(expr_, substr, StringBinOp::CONTAINS));
}
ExprPtr ExprPtr::starts_with(const std::string &prefix) const
{
  return ExprPtr(std::make_shared<StringBinOpExpr>(expr_, prefix, StringBinOp::STARTS_WITH));
}
ExprPtr ExprPtr::ends_with(const std::string &suffix) const
{
  return ExprPtr(std::make_shared<StringBinOpExpr>(expr_, suffix, StringBinOp::ENDS_WITH));
}