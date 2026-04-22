#ifndef relOpExpr_H
#define relOpExpr_H

#include "../Types.h"
#include "../expression/expr.h"
#include "../utils/arrayOps.h"

class RelOpExpr : public Expr
{
private:
  ExprPtr left_;
  ExprPtr right_;
  RelOp op_;

public:
  RelOpExpr(ExprPtr left, ExprPtr right, RelOp op)
      : left_(std::move(left)), right_(std::move(right)), op_(op) {}

  std::shared_ptr<arrow::ChunkedArray> evaluate(
      const std::shared_ptr<arrow::Table> &table) const override
  {
    auto leftArr = left_->evaluate(table);
    auto rightArr = right_->evaluate(table);

    // type promotion check
    auto leftType = left_->resultType(table->schema());
    auto rightType = right_->resultType(table->schema());
    assertCompatible(leftType, rightType);
    DataType resultType = promoteTypes(leftType, rightType);

    return applyRelOp(leftArr, rightArr, resultType, op_);
  }

  DataType resultType(const std::shared_ptr<arrow::Schema> &schema) const override
  {
    return DataType::BOOLEAN;
  };

  std::string toString() const override
  {
    std::string opStr;
    switch (op_)
    {
    case RelOp::EQ:
      opStr = "==";
      break;
    case RelOp::NEQ:
      opStr = "!=";
      break;
    case RelOp::LT:
      opStr = "<";
      break;
    case RelOp::LTE:
      opStr = "<=";
      break;
    case RelOp::GT:
      opStr = ">";
      break;
    case RelOp::GTE:
      opStr = ">=";
      break;
    }
    return "(" + left_->toString() + " " + opStr + " " + right_->toString() + ")";
  }
};

template <typename T>
inline ExprPtr operator==(ExprPtr l, T r)
{
  return std::make_shared<RelOpExpr>(std::move(l), lit(r), RelOp::EQ);
}
template <typename T>
inline ExprPtr operator!=(ExprPtr l, T r)
{
  return std::make_shared<RelOpExpr>(std::move(l), lit(r), RelOp::NEQ);
}
template <typename T>
inline ExprPtr operator<(ExprPtr l, T r)
{
  return std::make_shared<RelOpExpr>(std::move(l), lit(r), RelOp::LT);
}
template <typename T>
inline ExprPtr operator<=(ExprPtr l, T r)
{
  return std::make_shared<RelOpExpr>(std::move(l), lit(r), RelOp::LTE);
}
template <typename T>
inline ExprPtr operator>(ExprPtr l, T r)
{
  return std::make_shared<RelOpExpr>(std::move(l), lit(r), RelOp::GT);
}
template <typename T>
inline ExprPtr operator>=(ExprPtr l, T r)
{
  return std::make_shared<RelOpExpr>(std::move(l), lit(r), RelOp::GTE);
}

/*template <>
inline ExprPtr operator== <ExprPtr>(ExprPtr l, ExprPtr r)
{
  return std::make_shared<RelOpExpr>(std::move(l), std::move(r), RelOp::EQ);
}
template <>
inline ExprPtr operator!= <ExprPtr>(ExprPtr l, ExprPtr r)
{
  return std::make_shared<RelOpExpr>(std::move(l), std::move(r), RelOp::NEQ);
}
template <>
inline ExprPtr operator< <ExprPtr>(ExprPtr l, ExprPtr r)
{
  return std::make_shared<RelOpExpr>(std::move(l), std::move(r), RelOp::LT);
}
template <>
inline ExprPtr operator<= <ExprPtr>(ExprPtr l, ExprPtr r)
{
  return std::make_shared<RelOpExpr>(std::move(l), std::move(r), RelOp::LTE);
}
template <>
inline ExprPtr operator><ExprPtr>(ExprPtr l, ExprPtr r)
{
  return std::make_shared<RelOpExpr>(std::move(l), std::move(r), RelOp::GT);
}
template <>
inline ExprPtr operator>= <ExprPtr>(ExprPtr l, ExprPtr r)
{
  return std::make_shared<RelOpExpr>(std::move(l), std::move(r), RelOp::GTE);
}*/

#endif