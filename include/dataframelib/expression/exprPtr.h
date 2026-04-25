#ifndef EXPRPTR_H
#define EXPRPTR_H

#include "expr.h"
class UnaryOpExpr;
class AliasExpr;

// Wrapper around shared_ptr<Expr> to allow .abs() and similar methods
namespace dataframelib
{
  class ExprPtr
  {
  public:
    // Implicit constructor — allows shared_ptr<Expr> to convert to ExprPtr
    // This means make_shared<ColExpr>() can be returned as ExprPtr directly
    ExprPtr(std::shared_ptr<Expr> expr) : expr_(std::move(expr)) {}

    Expr *operator->() const { return expr_.get(); }
    ExprPtr abs() const;
    ExprPtr is_null() const;
    ExprPtr is_not_null() const;
    ExprPtr alias(const std::string &name) const;
    ExprPtr length() const;
    ExprPtr to_upper() const;
    ExprPtr to_lower() const;
    ExprPtr contains(const std::string &substr) const;
    ExprPtr starts_with(const std::string &prefix) const;
    ExprPtr ends_with(const std::string &suffix) const;
    ExprPtr sum() const;
    ExprPtr mean() const;
    ExprPtr count() const;
    ExprPtr min() const;
    ExprPtr max() const;
    // These make ExprPtr usable wherever Expr* is needed
    std::shared_ptr<arrow::ChunkedArray> evaluate(
        const std::shared_ptr<arrow::Table> &table) const
    {
      return expr_->evaluate(table);
    }
    DataType resultType(
        const std::shared_ptr<arrow::Schema> &schema) const
    {
      return expr_->resultType(schema);
    }
    std::string toString() const
    {
      return expr_->toString();
    }

    // defined to convert shared pointer to ExprPtr implicitly
    template <typename T, typename = std::enable_if_t<std::is_base_of_v<Expr, T>>>
    ExprPtr(std::shared_ptr<T> expr) : expr_(std::move(expr)) {}

    // Raw access — needed by DataFrame internals
    std::shared_ptr<Expr> get() const { return expr_; }

  private:
    std::shared_ptr<Expr> expr_;
  };
} // namespace dataframelib
#endif