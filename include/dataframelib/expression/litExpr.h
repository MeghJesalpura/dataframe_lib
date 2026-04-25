#ifndef LITEXPR_H
#define LITEXPR_H
#include "../Types.h"
#include "expr.h"

namespace dataframelib
{
  class LitExpr : public Expr
  {
    // Use variant to hold any supported scalar type
    using LitValue = std::variant<int32_t, int64_t, float, double, std::string, bool>;
    LitValue value_;
    DataType type_;

  public:
    explicit LitExpr(int32_t v) : value_(v), type_(DataType::INT32) {}
    explicit LitExpr(int64_t v) : value_(v), type_(DataType::INT64) {}
    explicit LitExpr(float v) : value_(v), type_(DataType::FLOAT32) {}
    explicit LitExpr(double v) : value_(v), type_(DataType::FLOAT64) {}
    explicit LitExpr(std::string v) : value_(v), type_(DataType::STRING) {}
    explicit LitExpr(bool v) : value_(v), type_(DataType::BOOLEAN) {}

    std::shared_ptr<arrow::ChunkedArray> evaluate(
        const std::shared_ptr<arrow::Table> &table) const override
    {

      // Build an array of the literal value repeated for every row
      int64_t numRows = table->num_rows();
      switch (type_)
      {
      case DataType::INT32:
      {
        arrow::Int32Builder builder;
        for (int64_t i = 0; i < numRows; i++)
          builder.Append(std::get<int32_t>(value_));
        std::shared_ptr<arrow::Array> arr;
        builder.Finish(&arr);
        return std::make_shared<arrow::ChunkedArray>(arr);
      }
      case DataType::INT64:
      {
        arrow::Int64Builder builder;
        for (int64_t i = 0; i < numRows; i++)
          builder.Append(std::get<int64_t>(value_));
        std::shared_ptr<arrow::Array> arr;
        builder.Finish(&arr);
        return std::make_shared<arrow::ChunkedArray>(arr);
      }
      case DataType::FLOAT32:
      {
        arrow::FloatBuilder builder;
        for (int64_t i = 0; i < numRows; i++)
          builder.Append(std::get<float>(value_));
        std::shared_ptr<arrow::Array> arr;
        builder.Finish(&arr);
        return std::make_shared<arrow::ChunkedArray>(arr);
      }
      case DataType::FLOAT64:
      {
        arrow::DoubleBuilder builder;
        for (int64_t i = 0; i < numRows; i++)
          builder.Append(std::get<double>(value_));
        std::shared_ptr<arrow::Array> arr;
        builder.Finish(&arr);
        return std::make_shared<arrow::ChunkedArray>(arr);
      }
      case DataType::STRING:
      {
        arrow::StringBuilder builder;
        for (int64_t i = 0; i < numRows; i++)
          builder.Append(std::get<std::string>(value_));
        std::shared_ptr<arrow::Array> arr;
        builder.Finish(&arr);
        return std::make_shared<arrow::ChunkedArray>(arr);
      }
      case DataType::BOOLEAN:
      {
        arrow::BooleanBuilder builder;
        for (int64_t i = 0; i < numRows; i++)
          builder.Append(std::get<bool>(value_));
        std::shared_ptr<arrow::Array> arr;
        builder.Finish(&arr);
        return std::make_shared<arrow::ChunkedArray>(arr);
      }
      default:
        throw std::runtime_error("Unhandled literal type");
      }
    }

    DataType resultType(const std::shared_ptr<arrow::Schema> &) const override
    {
      return type_;
    }

    std::string toString() const override { return "lit(...)"; }
  };

  // Factory - handles type deduction automatically
  template <typename T>
  inline ExprPtr lit(T value)
  {
    return std::make_shared<LitExpr>(value);
  }

} // namespace dataframelib
#endif