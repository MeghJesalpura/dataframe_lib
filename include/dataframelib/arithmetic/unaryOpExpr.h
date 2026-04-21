#ifndef UnaryOpExpr_H
#define UnaryOpExpr_H
#include "../Types.h"
#include "../expression/expr.h"
#include <arrow/compute/api.h>
class UnaryOpExpr : public Expr
{
private:
  ExprPtr operand_;
  UnaryOp op_;

public:
  UnaryOpExpr(ExprPtr operand, UnaryOp op) : operand_(std::move(operand)), op_(op) {}

  std::shared_ptr<arrow::ChunkedArray> evaluate(const std::shared_ptr<arrow::Table> &table) const override
  {
    auto operandArr = operand_->evaluate(table);
    // Logic to perform the unary operation

    // written this manner so that extension when more unary ops are added is easier
    std::string fnName;
    switch (op_)
    {
    case UnaryOp::ABS:
      fnName = "abs";
      break;
    case UnaryOp::NOT:
      fnName = "invert";
      break;
    case UnaryOp::IS_NULL:
      fnName = "is_null";
      break;
    case UnaryOp::IS_NOT_NULL:
      fnName = "is_valid";
      break;
    }
    auto result = arrow::compute::CallFunction(fnName, {operandArr});
    if (!result.ok())
      throw std::runtime_error("Error applying unary operation: " + result.status().ToString());

    return result.ValueOrDie().chunked_array();
  }

  DataType resultType(const std::shared_ptr<arrow::Schema> &schema) const override
  {
    // For ABS, the result type is the same as the operand type
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

inline ExprPtr abs(ExprPtr operand)
{
  return std::make_shared<UnaryOpExpr>(std::move(operand), UnaryOp::ABS);
}

inline ExprPtr operator~(ExprPtr operand)
{
  return std::make_shared<UnaryOpExpr>(std::move(operand), UnaryOp::NOT);
}

inline ExprPtr is_null(ExprPtr operand)
{
  return std::make_shared<UnaryOpExpr>(std::move(operand), UnaryOp::IS_NULL);
}

inline ExprPtr is_not_null(ExprPtr operand)
{
  return std::make_shared<UnaryOpExpr>(std::move(operand), UnaryOp::IS_NOT_NULL);
}

#endif