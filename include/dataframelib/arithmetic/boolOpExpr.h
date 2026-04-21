#ifndef boolOpExpr_H
#define boolOpExpr_H

#include "../Types.h"
#include "../expression/expr.h"
#include <arrow/compute/api.h>

class BoolOpExpr : public Expr
{
private:
  ExprPtr left_;
  ExprPtr right_;
  BoolOp op_;

public:
  BoolOpExpr(ExprPtr left, ExprPtr right, BoolOp op) : left_(std::move(left)), right_(std::move(right)), op_(op) {}

  std::shared_ptr<arrow::ChunkedArray> evaluate(const std::shared_ptr<arrow::Table> &table) const override
  {
    auto leftArr = left_->evaluate(table);
    auto rightArr = right_->evaluate(table);

    std::string fnName;
    switch (op_)
    {
    case BoolOp::AND:
      fnName = "and";
      break;
    case BoolOp::OR:
      fnName = "or";
      break;
    }
    auto result = arrow::compute::CallFunction(fnName, {leftArr, rightArr});
    if (!result.ok())
      throw std::runtime_error("Error applying boolean operation: " + result.status().ToString());

    return result.ValueOrDie().chunked_array();
  }

  DataType resultType(const std::shared_ptr<arrow::Schema> &schema) const override
  {
    return DataType::BOOLEAN;
  }

  std::string toString() const override
  {
    std::string opStr;
    switch (op_)
    {
    case BoolOp::AND:
      opStr = "AND";
      break;
    case BoolOp::OR:
      opStr = "OR";
      break;
    }
    return "(" + left_->toString() + " " + opStr + " " + right_->toString() + ")";
  }
};

inline ExprPtr operator&(ExprPtr l, ExprPtr r)
{
  return std::make_shared<BoolOpExpr>(std::move(l), std::move(r), BoolOp::AND);
}

inline ExprPtr operator|(ExprPtr l, ExprPtr r)
{
  return std::make_shared<BoolOpExpr>(std::move(l), std::move(r), BoolOp::OR);
}
#endif