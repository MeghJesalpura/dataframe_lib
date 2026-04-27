#ifndef STR_UN_OP_EXPR_H
#define STR_UN_OP_EXPR_H

#include "../expression/expr.h"
#include "../utils/arrayOps.h"

namespace dataframelib
{
  class StringUnOpExpr : public Expr
  {
  public:
    StringUnOpExpr(const std::shared_ptr<Expr> &arr, StringUnOp op) : arr_(arr), op_(op) {};
    std::shared_ptr<arrow::ChunkedArray> evaluate(const std::shared_ptr<arrow::Table> &table) const override
    {
      auto arr = arr_->evaluate(table);

      return applyStringUnOp(arr, op_);
    }
    DataType resultType(const std::shared_ptr<arrow::Schema> &schema) const override
    {
      switch (op_)
      {
      case StringUnOp::LENGTH:
        return DataType::INT32;
      case StringUnOp::TO_LOWER:
      case StringUnOp::TO_UPPER:
        return DataType::STRING;
      }
      return DataType::STRING;
    }
    std::string toString() const override
    {
      std::string opStr;
      switch (op_)
      {
      case StringUnOp::LENGTH:
        opStr = "length";
        break;
      case StringUnOp::TO_LOWER:
        opStr = "to_lower";
        break;
      case StringUnOp::TO_UPPER:
        opStr = "to_upper";
        break;
      }
      return opStr + "(" + arr_->toString() + ")";
    }

  private:
    std::shared_ptr<Expr> arr_;
    StringUnOp op_;
  };
} // namespace dataframelib
#endif