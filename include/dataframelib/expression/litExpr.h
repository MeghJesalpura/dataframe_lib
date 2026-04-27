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
      int64_t numRows = table->num_rows();
      // MakeArrayFromScalar fills the buffer with memset/SIMD — far faster than
      // a per-element loop for large tables (O(N/64) vs O(N)).
      std::shared_ptr<arrow::Scalar> scalar;
      switch (type_)
      {
      case DataType::INT32:   scalar = arrow::MakeScalar(std::get<int32_t>(value_)); break;
      case DataType::INT64:   scalar = arrow::MakeScalar(std::get<int64_t>(value_)); break;
      case DataType::FLOAT32: scalar = arrow::MakeScalar(std::get<float>(value_)); break;
      case DataType::FLOAT64: scalar = arrow::MakeScalar(std::get<double>(value_)); break;
      case DataType::STRING:  scalar = arrow::MakeScalar(std::get<std::string>(value_)); break;
      case DataType::BOOLEAN: scalar = arrow::MakeScalar(std::get<bool>(value_)); break;
      default: throw std::runtime_error("Unhandled literal type");
      }
      auto arr = arrow::MakeArrayFromScalar(*scalar, numRows).ValueOrDie();
      return std::make_shared<arrow::ChunkedArray>(arr);
    }

    DataType resultType(const std::shared_ptr<arrow::Schema> &) const override
    {
      return type_;
    }

    std::string toString() const override
    {
      // return something more specific hopefully
      return "lit(" + std::visit([](auto &&arg) -> std::string
                                 {
        using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_same_v<T, std::string>) {
        return "\"" + arg + "\""; // Wrap strings in quotes
    } 
    else if constexpr (std::is_same_v<T, bool>) {
        return arg ? "true" : "false";
    }
    else {
        return std::to_string(arg); // Handles int, float, double, etc.
    } }, value_) +
             ")";
    }
  };

  // Factory - handles type deduction automatically
  template <typename T>
  inline ExprPtr lit(T value)
  {
    return std::make_shared<LitExpr>(value);
  }

  inline ExprPtr lit(const char *value)
  {
    return std::make_shared<LitExpr>(std::string(value));
  }

} // namespace dataframelib
#endif