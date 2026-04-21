#ifndef binOpExpr_H
#define binOpExpr_H

#include "../Types.h"
#include "../expression/expr.h"
#include <arrow/compute/api.h>
class BinOpExpr : public Expr
{
private:
  ExprPtr left_;
  ExprPtr right_;
  BinaryOp op_;

  std::shared_ptr<arrow::ChunkedArray> applyOp(const std::shared_ptr<arrow::ChunkedArray> &leftArr,
                                               const std::shared_ptr<arrow::ChunkedArray> &rightArr) const
  {
    // This function would contain the logic to apply the binary operation
    std::string fnName;
    switch (op_)
    {
    case BinaryOp::ADD:
      fnName = "add";
      break;
    case BinaryOp::SUB:
      fnName = "subtract";
      break;
    case BinaryOp::MUL:
      fnName = "multiply";
      break;
    case BinaryOp::DIV:
      fnName = "divide";
      break;
    case BinaryOp::MOD:
      // INT_32 and INT_64 handled in the public function
      // does not have an inbuilt implementation as far as I saw - there was an issue where they were trying to implement this
      auto div = arrow::compute::CallFunction("divide", {leftArr, rightArr}).ValueOrDie().chunked_array();
      auto mul = arrow::compute::CallFunction("multiply", {div, rightArr}).ValueOrDie().chunked_array();
      auto result = arrow::compute::CallFunction("subtract", {leftArr, mul});
      if (!result.ok())
        throw std::runtime_error("Error applying modulo operation: " + result.status().ToString());
      return result.ValueOrDie().chunked_array();
      break;
    }
    auto result = arrow::compute::CallFunction(fnName, {leftArr, rightArr});
    if (!result.ok())
      throw std::runtime_error("Error applying binary operation: " + result.status().ToString());

    return result.ValueOrDie().chunked_array();
  }

  std::string opToString(BinaryOp op) const
  {
    switch (op)
    {
    case BinaryOp::ADD:
      return "+";
    case BinaryOp::SUB:
      return "-";
    case BinaryOp::MUL:
      return "*";
    case BinaryOp::DIV:
      return "/";
    case BinaryOp::MOD:
      return "%";
    }
    return "?";
  }

public:
  BinOpExpr(ExprPtr left, ExprPtr right, BinaryOp op)
      : left_(std::move(left)), right_(std::move(right)), op_(op) {}

  std::shared_ptr<arrow::ChunkedArray> evaluate(
      const std::shared_ptr<arrow::Table> &table) const override
  {
    auto leftArr = left_->evaluate(table);
    auto rightArr = right_->evaluate(table);
    // Here we would implement the logic to perform the binary operation

    // type promotion check
    auto leftType = left_->resultType(table->schema());
    auto rightType = right_->resultType(table->schema());
    assertCompatible(leftType, rightType);
    DataType resultType = promoteTypes(leftType, rightType);

    // cast both arrays to promoted type if needed
    auto targetArrowType = toArrowType(resultType);
    if (fromArrowType(leftArr->type()) != resultType)
    {
      leftArr = arrow::compute::Cast(leftArr, targetArrowType).ValueOrDie().chunked_array();
    }
    if (fromArrowType(rightArr->type()) != resultType)
    {
      rightArr = arrow::compute::Cast(rightArr, targetArrowType).ValueOrDie().chunked_array();
    }

    if (op_ == BinaryOp::MOD && (resultType != DataType::INT32 && resultType != DataType::INT64))
    {
      throw std::runtime_error("Modulo operator is only supported for integer types");
    }
    return applyOp(leftArr, rightArr);
  }

  DataType resultType(const std::shared_ptr<arrow::Schema> &schema) const override
  {
    return promoteTypes(left_->resultType(schema), right_->resultType(schema));
  };

  std::string toString() const override
  {
    return "(" + left_->toString() + " " + opToString(op_) + " " + right_->toString() + ")";
  }
};

template <typename T>
inline ExprPtr operator+(ExprPtr l, T r)
{
  return std::make_shared<BinOpExpr>(std::move(l), lit(r), BinaryOp::ADD);
}
template <typename T>
inline ExprPtr operator*(ExprPtr l, T r)
{
  return std::make_shared<BinOpExpr>(std::move(l), lit(r), BinaryOp::MUL);
}
template <typename T>
inline ExprPtr operator/(ExprPtr l, T r)
{
  return std::make_shared<BinOpExpr>(std::move(l), lit(r), BinaryOp::DIV);
}
template <typename T>
inline ExprPtr operator%(ExprPtr l, T r)
{
  return std::make_shared<BinOpExpr>(std::move(l), lit(r), BinaryOp::MOD);
}
template <typename T>
inline ExprPtr operator-(ExprPtr l, T r)
{
  return std::make_shared<BinOpExpr>(std::move(l), lit(r), BinaryOp::SUB);
}

// col("a") + lit(3) has not been implemented here
#endif