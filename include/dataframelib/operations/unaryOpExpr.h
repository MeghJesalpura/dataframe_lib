#ifndef UnaryOpExpr_H
#define UnaryOpExpr_H
#include "../Types.h"
#include "../expression/expr.h"
#include "../utils/arrayOps.h"
#include "../expression/exprPtr.h"
namespace dataframelib
{
  class UnaryOpExpr : public Expr
  {
  private:
    ExprPtr operand_;
    UnaryOp op_;

  public:
    UnaryOpExpr(ExprPtr operand, UnaryOp op) : operand_(std::move(operand)), op_(op) {}

    ExprPtr getOperand() const { return operand_; }
    UnaryOp getOp() const { return op_; }

    std::shared_ptr<arrow::ChunkedArray> evaluate(const std::shared_ptr<arrow::Table> &table) const override
    {
      auto operandArr = operand_->evaluate(table);
      // Logic to perform the unary operation

      DataType type = operand_->resultType(table->schema());
      return applyUnaryOp(operandArr, type, op_);
    }

    DataType resultType(const std::shared_ptr<arrow::Schema> &schema) const override
    {
      if (op_ == UnaryOp::NOT || op_ == UnaryOp::IS_NULL || op_ == UnaryOp::IS_NOT_NULL)
        return DataType::BOOLEAN;
      // For ABS, the result type is the same as the operand type.
      return operand_->resultType(schema);
    }

    std::string toString() const override
    {
      std::string opStr;
      switch (op_)
      {
      case UnaryOp::ABS:
        opStr = "abs";
        break;
      case UnaryOp::NOT:
        opStr = "not";
        break;
      case UnaryOp::IS_NULL:
        opStr = "is_null";
        break;
      case UnaryOp::IS_NOT_NULL:
        opStr = "is_not_null";
        break;
      }
      return opStr + "(" + operand_->toString() + ")";
    }
  };

  inline ExprPtr operator~(ExprPtr operand)
  {
    return std::make_shared<UnaryOpExpr>(std::move(operand), UnaryOp::NOT);
  }
} // namespace dataframelib
#endif