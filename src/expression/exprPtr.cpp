#include "dataframelib/expression/exprPtr.h"
#include "dataframelib/operations/unaryOpExpr.h"
#include "dataframelib/expression/aliasExpr.h"
#include "dataframelib/operations/strUnOpExpr.h"
#include "dataframelib/operations/strBinOpExpr.h"
#include "dataframelib/operations/aggOpExpr.h"

namespace dataframelib
{
  auto ExprPtr::abs() const -> ExprPtr
  {
    return ExprPtr(std::make_shared<UnaryOpExpr>(expr_, UnaryOp::ABS));
  }
  auto ExprPtr::is_null() const -> ExprPtr
  {
    return ExprPtr(std::make_shared<UnaryOpExpr>(expr_, UnaryOp::IS_NULL));
  }
  auto ExprPtr::is_not_null() const -> ExprPtr
  {
    return ExprPtr(std::make_shared<UnaryOpExpr>(expr_, UnaryOp::IS_NOT_NULL));
  }
  auto ExprPtr::alias(const std::string &name) const -> ExprPtr
  {
    return ExprPtr(std::make_shared<AliasExpr>(expr_, name));
  }
  auto ExprPtr::length() const -> ExprPtr
  {
    return ExprPtr(std::make_shared<StringUnOpExpr>(expr_, StringUnOp::LENGTH));
  }
  auto ExprPtr::to_upper() const -> ExprPtr
  {
    return ExprPtr(std::make_shared<StringUnOpExpr>(expr_, StringUnOp::TO_UPPER));
  }
  auto ExprPtr::to_lower() const -> ExprPtr
  {
    return ExprPtr(std::make_shared<StringUnOpExpr>(expr_, StringUnOp::TO_LOWER));
  }
  auto ExprPtr::contains(const std::string &substr) const -> ExprPtr
  {
    return ExprPtr(std::make_shared<StringBinOpExpr>(expr_, substr, StringBinOp::CONTAINS));
  }
  auto ExprPtr::starts_with(const std::string &prefix) const -> ExprPtr
  {
    return ExprPtr(std::make_shared<StringBinOpExpr>(expr_, prefix, StringBinOp::STARTS_WITH));
  }
  auto ExprPtr::ends_with(const std::string &suffix) const -> ExprPtr
  {
    return ExprPtr(std::make_shared<StringBinOpExpr>(expr_, suffix, StringBinOp::ENDS_WITH));
  }
  auto ExprPtr::sum() const -> ExprPtr
  {
    return ExprPtr(std::make_shared<AggOpExpr>(AggOp::SUM, expr_));
  }
  auto ExprPtr::mean() const -> ExprPtr
  {
    return ExprPtr(std::make_shared<AggOpExpr>(AggOp::MEAN, expr_));
  }
  auto ExprPtr::count() const -> ExprPtr
  {
    return ExprPtr(std::make_shared<AggOpExpr>(AggOp::COUNT, expr_));
  }
  auto ExprPtr::min() const -> ExprPtr
  {
    return ExprPtr(std::make_shared<AggOpExpr>(AggOp::MIN, expr_));
  }
  auto ExprPtr::max() const -> ExprPtr
  {
    return ExprPtr(std::make_shared<AggOpExpr>(AggOp::MAX, expr_));
  }
}