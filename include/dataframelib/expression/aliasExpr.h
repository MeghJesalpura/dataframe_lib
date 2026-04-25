#ifndef ALIAS_EXPR_H
#define ALIAS_EXPR_H

#include "../Types.h"
#include "expr.h"

namespace dataframelib
{
  class AliasExpr : public Expr
  {
    ExprPtr inner_;
    std::string alias_;

  public:
    AliasExpr(ExprPtr inner, std::string alias)
        : inner_(std::move(inner)), alias_(std::move(alias)) {}

    std::shared_ptr<arrow::ChunkedArray> evaluate(
        const std::shared_ptr<arrow::Table> &table) const override
    {
      // Just delegates to inner - renaming happens at DataFrame level
      return inner_->evaluate(table);
    }

    DataType resultType(const std::shared_ptr<arrow::Schema> &schema) const override
    {
      return inner_->resultType(schema);
    }

    const std::string &getAlias() const { return alias_; }

    std::string toString() const override
    {
      return inner_->toString() + ".alias(" + alias_ + ")";
    }
  };

} // namespace dataframelib

#endif