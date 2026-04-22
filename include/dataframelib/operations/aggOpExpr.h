#ifndef AGG_OP_EXPR_H
#define AGG_OP_EXPR_H

#include "../Types.h"
#include "../expression/expr.h"
#include "../utils/arrayOps.h"
#include "../utils/opsHelper.h"
#include "../expression/exprPtr.h"
#include "../utils/typeDispatch.h"
class AggOpExpr : public Expr
{
private:
  AggOp op_;
  ExprPtr operand_;

public:
  AggOpExpr(AggOp op, ExprPtr operand) : op_(op), operand_(operand) {}

  std::shared_ptr<arrow::ChunkedArray> evaluate(
      const std::shared_ptr<arrow::Table> &table) const override
  {

    auto arr = operand_.evaluate(table);
    DataType t = operand_.resultType(table->schema());

    switch (op_)
    {
    case AggOp::SUM:
      return dispatchNumeric<SumImpl>(t, arr);
    case AggOp::MEAN:
      return dispatchNumeric<MeanImpl>(t, arr);
    case AggOp::MIN:
      return dispatchNumeric<MinImpl>(t, arr);
    case AggOp::MAX:
      return dispatchNumeric<MaxImpl>(t, arr);
    case AggOp::COUNT:
    {
      arrow::Int64Builder builder;
      builder.Append(computeCount(arr));
      std::shared_ptr<arrow::Array> result;
      builder.Finish(&result);
      return toChunked(result);
    }
    }
  }

  DataType resultType(const std::shared_ptr<arrow::Schema> &schema) const override
  {
    // The result type of an aggregate operation is typically numeric or count (int64)
    switch (op_)
    {
    case AggOp::COUNT:
      return DataType::INT64;
    case AggOp::SUM:
    case AggOp::MEAN:
    case AggOp::MIN:
    case AggOp::MAX:
      return operand_->resultType(schema);
    }
  }

  std::string toString() const override
  {
    std::string opName;
    switch (op_)
    {
    case AggOp::SUM:
      opName = "SUM";
      break;
    case AggOp::MEAN:
      opName = "MEAN";
      break;
    case AggOp::COUNT:
      opName = "COUNT";
      break;
    case AggOp::MIN:
      opName = "MIN";
      break;
    case AggOp::MAX:
      opName = "MAX";
      break;
    }
    return opName + "()";
  }
};
#endif