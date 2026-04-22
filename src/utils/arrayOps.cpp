#include "../../include/dataframelib/utils/arrayOps.h"
#include "../../include/dataframelib/utils/opsHelper.h"
// Generic row iterator to remove abstract out repetitive code
template <DataType T, typename OutputBuilder, typename Func>
std::shared_ptr<arrow::Array> applyRowwise(
    const std::shared_ptr<arrow::Array> &left,
    const std::shared_ptr<arrow::Array> &right,
    Func &&fn)
{
  using ArrType = typename TypeTraits<T>::ArrayType;
  auto l = std::static_pointer_cast<ArrType>(left);
  auto r = std::static_pointer_cast<ArrType>(right);
  OutputBuilder builder;

  for (int64_t i = 0; i < l->length(); i++)
  {
    if (l->IsNull(i) || r->IsNull(i))
    {
      builder.AppendNull();
    }
    else
    {
      builder.Append(fn(l->Value(i), r->Value(i)));
    }
  }

  std::shared_ptr<arrow::Array> result;
  builder.Finish(&result);
  return result;
}

// Unary version
template <DataType T, typename OutputBuilder, typename Func>
std::shared_ptr<arrow::Array> applyRowwiseUnary(
    const std::shared_ptr<arrow::Array> &arr,
    Func &&fn)
{
  using ArrType = typename TypeTraits<T>::ArrayType;
  auto a = std::static_pointer_cast<ArrType>(arr);
  OutputBuilder builder;

  for (int64_t i = 0; i < a->length(); i++)
  {
    if (a->IsNull(i))
    {
      builder.AppendNull();
    }
    else
    {
      builder.Append(fn(a->Value(i)));
    }
  }
  std::shared_ptr<arrow::Array> result;
  builder.Finish(&result);
  return result;
}

template <typename T>
struct BinaryOpImpl
{
  std::shared_ptr<arrow::ChunkedArray> operator()(
      const std::shared_ptr<arrow::Array> &l,
      const std::shared_ptr<arrow::Array> &r,
      BinaryOp op)
  {
    using Builder = typename TypeTraits<T>::BuilderType;
    using Cpp = typename TypeTraits<T>::CppType;

    std::shared_ptr<arrow::Array> result;
    switch (op)
    {
    case BinaryOp::ADD:
      result = applyRowwise<T, Builder>(l, r,
                                        [](Cpp a, Cpp b)
                                        { return a + b; });
      break;
    case BinaryOp::SUB:
      result = applyRowwise<T, Builder>(l, r,
                                        [](Cpp a, Cpp b)
                                        { return a - b; });
      break;
    case BinaryOp::MUL:
      result = applyRowwise<T, Builder>(l, r,
                                        [](Cpp a, Cpp b)
                                        { return a * b; });
      break;
    case BinaryOp::DIV:
      result = applyRowwise<T, Builder>(l, r,
                                        [](Cpp a, Cpp b) -> Cpp
                                        {
                                          if (b == 0)
                                            throw std::runtime_error("Division by zero");
                                          return a / b;
                                        });
      break;
    case BinaryOp::MOD:
      if constexpr (std::is_integral_v<Cpp>)
      {
        result = applyRowwise<T, Builder>(l, r,
                                          [](Cpp a, Cpp b) -> Cpp
                                          {
                                            if (b == 0)
                                              throw std::runtime_error("Modulo by zero");
                                            return a % b;
                                          });
      }
      else
      {
        throw std::runtime_error("MOD only valid for integer types");
      }
      break;
    }
    return toChunked(result);
  }
};

std::shared_ptr<arrow::ChunkedArray> applyBinaryOp(
    const std::shared_ptr<arrow::ChunkedArray> &left,
    const std::shared_ptr<arrow::ChunkedArray> &right,
    DataType type, BinaryOp op)
{
  auto l = flatten(left);
  auto r = flatten(right);
  return dispatchNumeric<BinaryOpImpl>(type, l, r, op);
}

// RelOp dispatch
template <typename T>
struct RelOpImpl
{
  std::shared_ptr<arrow::ChunkedArray> operator()(
      const std::shared_ptr<arrow::Array> &l,
      const std::shared_ptr<arrow::Array> &r,
      RelOp op)
  {

    using Cpp = typename TypeTraits<T>::CppType;

    auto result = applyRowwise<T, arrow::BooleanBuilder>(l, r,
                                                         [op](Cpp a, Cpp b) -> bool
                                                         {
                                                           switch (op)
                                                           {
                                                           case RelOp::EQ:
                                                             return a == b;
                                                           case RelOp::NEQ:
                                                             return a != b;
                                                           case RelOp::LT:
                                                             return a < b;
                                                           case RelOp::LTE:
                                                             return a <= b;
                                                           case RelOp::GT:
                                                             return a > b;
                                                           case RelOp::GTE:
                                                             return a >= b;
                                                           }
                                                         });
    return toChunked(result);
  }
};

std::shared_ptr<arrow::ChunkedArray> applyRelOp(
    const std::shared_ptr<arrow::ChunkedArray> &left,
    const std::shared_ptr<arrow::ChunkedArray> &right,
    DataType type, RelOp op)
{

  auto l = flatten(left);
  auto r = flatten(right);
  return dispatchNumeric<RelOpImpl>(type, l, r, op);
}

// BoolOp
std::shared_ptr<arrow::ChunkedArray> applyBoolOp(
    const std::shared_ptr<arrow::ChunkedArray> &left,
    const std::shared_ptr<arrow::ChunkedArray> &right,
    BoolOp op)
{

  auto l = std::static_pointer_cast<arrow::BooleanArray>(flatten(left));
  auto r = std::static_pointer_cast<arrow::BooleanArray>(flatten(right));
  arrow::BooleanBuilder builder;

  for (int64_t i = 0; i < l->length(); i++)
  {
    if (l->IsNull(i) || r->IsNull(i))
    {
      builder.AppendNull();
      continue;
    }
    bool lv = l->Value(i), rv = r->Value(i);
    builder.Append(op == BoolOp::AND ? lv && rv : lv || rv);
  }

  std::shared_ptr<arrow::Array> result;
  builder.Finish(&result);
  return toChunked(result);
}

// UnaryOp
template <typename T>
struct UnaryOpImpl
{
  std::shared_ptr<arrow::ChunkedArray> operator()(
      const std::shared_ptr<arrow::Array> &arr, UnaryOp op)
  {

    using Builder = typename TypeTraits<T>::BuilderType;
    using Cpp = typename TypeTraits<T>::CppType;

    auto result = applyRowwiseUnary<T, Builder>(arr,
                                                [](Cpp a)
                                                { return std::abs(a); }); // only ABS reaches here
    return toChunked(result);
  }
};

std::shared_ptr<arrow::ChunkedArray> applyUnaryOp(
    const std::shared_ptr<arrow::ChunkedArray> &arr,
    DataType type, UnaryOp op)
{

  auto flat = flatten(arr);

  // IS_NULL and IS_NOT_NULL don't need type dispatch
  if (op == UnaryOp::IS_NULL || op == UnaryOp::IS_NOT_NULL)
  {
    arrow::BooleanBuilder builder;
    for (int64_t i = 0; i < flat->length(); i++)
      builder.Append(op == UnaryOp::IS_NULL
                         ? flat->IsNull(i)
                         : flat->IsValid(i));
    std::shared_ptr<arrow::Array> result;
    builder.Finish(&result);
    return toChunked(result);
  }

  // NOT
  if (op == UnaryOp::NOT)
  {
    auto a = std::static_pointer_cast<arrow::BooleanArray>(flat);
    arrow::BooleanBuilder builder;
    for (int64_t i = 0; i < a->length(); i++)
    {
      if (a->IsNull(i))
      {
        builder.AppendNull();
        continue;
      }
      builder.Append(!a->Value(i));
    }
    std::shared_ptr<arrow::Array> result;
    builder.Finish(&result);
    return toChunked(result);
  }

  // ABS — needs type dispatch
  return dispatchNumeric<UnaryOpImpl>(type, flat, op);
}

// StringOp
std::shared_ptr<arrow::ChunkedArray> applyStringUnOp(
    const std::shared_ptr<arrow::ChunkedArray> &arr,
    StringUnOp op)
{
  auto flat = std::static_pointer_cast<arrow::StringArray>(flatten(arr));
  arrow::StringBuilder strBuilder;
  arrow::Int32Builder intBuilder;
  arrow::BooleanBuilder boolBuilder;

  for (int64_t i = 0; i < flat->length(); i++)
  {
    if (flat->IsNull(i))
    {
      switch (op)
      {
      case StringUnOp::LENGTH:
        intBuilder.AppendNull();
        break;
      case StringUnOp::TO_LOWER:
      case StringUnOp::TO_UPPER:
        strBuilder.AppendNull();
        break;
      default:
        boolBuilder.AppendNull();
        break;
      }
      continue;
    }
    std::string s(flat->Value(i));
    switch (op)
    {
    case StringUnOp::LENGTH:
      intBuilder.Append(static_cast<int32_t>(s.size()));
      break;
    case StringUnOp::TO_LOWER:
      std::transform(s.begin(), s.end(), s.begin(), ::tolower);
      strBuilder.Append(s);
      break;
    case StringUnOp::TO_UPPER:
      std::transform(s.begin(), s.end(), s.begin(), ::toupper);
      strBuilder.Append(s);
      break;
    }
  }

  std::shared_ptr<arrow::Array> result;
  switch (op)
  {
  case StringUnOp::LENGTH:
    intBuilder.Finish(&result);
    break;
  case StringUnOp::TO_LOWER:
  case StringUnOp::TO_UPPER:
    strBuilder.Finish(&result);
    break;
  }
  return toChunked(result);
}

std::shared_ptr<arrow::ChunkedArray> applyStringBinOp(
    const std::shared_ptr<arrow::ChunkedArray> &arr,
    const std::string &arg,
    StringBinOp op)
{
  auto flat = std::static_pointer_cast<arrow::StringArray>(flatten(arr));
  arrow::BooleanBuilder builder; // all three ops always return boolean

  for (int64_t i = 0; i < flat->length(); i++)
  {
    if (flat->IsNull(i))
    {
      builder.AppendNull();
      continue;
    }
    std::string s(flat->Value(i));
    switch (op)
    {
    case StringBinOp::CONTAINS:
      builder.Append(s.find(arg) != std::string::npos);
      break;
    case StringBinOp::STARTS_WITH:
      builder.Append(s.rfind(arg, 0) == 0);
      break;
    case StringBinOp::ENDS_WITH:
      builder.Append(
          s.size() >= arg.size() &&
          s.compare(s.size() - arg.size(), arg.size(), arg) == 0);
      break;
    }
  }

  std::shared_ptr<arrow::Array> result;
  builder.Finish(&result);
  return toChunked(result);
}