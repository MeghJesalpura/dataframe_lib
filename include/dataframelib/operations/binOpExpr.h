#ifndef binOpExpr_H
#define binOpExpr_H

#include "../Types.h"
#include "../expression/expr.h"
#include "../utils/arrayOps.h"
#include "../expression/exprPtr.h"
#include "../expression/litExpr.h"
namespace dataframelib
{
  class BinOpExpr : public Expr
  {
  private:
    ExprPtr left_;
    ExprPtr right_;
    BinaryOp op_;

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

      leftArr = castArray(leftArr, leftType, resultType);
      rightArr = castArray(rightArr, rightType, resultType);

      return applyBinaryOp(leftArr, rightArr, resultType, op_);
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
}
#endif