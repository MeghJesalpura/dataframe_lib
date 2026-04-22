#ifndef STR_BIN_OP_EXPR_H
#define STR_BIN_OP_EXPR_H

#include "../expression/expr.h"
#include "../utils/arrayOps.h"
class StringBinOpExpr : public Expr
{
public:
  StringBinOpExpr(const std::shared_ptr<Expr> &left, const std::string &arg, StringBinOp op) : left_(left), arg_(arg), op_(op) {};
  std::shared_ptr<arrow::ChunkedArray> evaluate(const std::shared_ptr<arrow::Table> &table) const override
  {
    auto leftArr = left_->evaluate(table);
    // Logic to perform the string operation
    return applyStringBinOp(leftArr, arg_, op_);
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
    case StringBinOp::CONTAINS:
      opStr = "contains";
      break;
    case StringBinOp::STARTS_WITH:
      opStr = "starts_with";
      break;
    case StringBinOp::ENDS_WITH:
      opStr = "ends_with";
      break;
    }
    return left_->toString() + " " + opStr + " '" + arg_ + "'";
  }

private:
  std::shared_ptr<Expr> left_;
  std::string arg_;
  StringBinOp op_;
};
#endif