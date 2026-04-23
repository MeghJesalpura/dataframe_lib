#ifndef TYPE_DISPATCH_H
#define TYPE_DISPATCH_H
#include <arrow/api.h>
#include "../Types.h"

// Maps DataType enum to Arrow array/builder types at compile time
template <DataType T>
struct TypeTraits
{
};

template <>
struct TypeTraits<DataType::INT32>
{
  using ArrayType = arrow::Int32Array;
  using BuilderType = arrow::Int32Builder;
  using CppType = int32_t;
};
template <>
struct TypeTraits<DataType::INT64>
{
  using ArrayType = arrow::Int64Array;
  using BuilderType = arrow::Int64Builder;
  using CppType = int64_t;
};
template <>
struct TypeTraits<DataType::FLOAT32>
{
  using ArrayType = arrow::FloatArray;
  using BuilderType = arrow::FloatBuilder;
  using CppType = float;
};
template <>
struct TypeTraits<DataType::FLOAT64>
{
  using ArrayType = arrow::DoubleArray;
  using BuilderType = arrow::DoubleBuilder;
  using CppType = double;
};
template <>
struct TypeTraits<DataType::STRING>
{
  using ArrayType = arrow::StringArray;
  using BuilderType = arrow::StringBuilder;
  using CppType = std::string_view;
};
template <>
struct TypeTraits<DataType::BOOLEAN>
{
  using ArrayType = arrow::BooleanArray;
  using BuilderType = arrow::BooleanBuilder;
  using CppType = bool;
};

// Runtime dispatcher — calls Func<T>{}(args) based on runtime DataType
// allows to write one lambda and dispatch to the right typed version
template <template <DataType> class Func, typename... Args>
auto dispatchNumeric(DataType t, Args &&...args)
{
  switch (t)
  {
  case DataType::INT32:
    return Func<DataType::INT32>{}(std::forward<Args>(args)...);
  case DataType::INT64:
    return Func<DataType::INT64>{}(std::forward<Args>(args)...);
  case DataType::FLOAT32:
    return Func<DataType::FLOAT32>{}(std::forward<Args>(args)...);
  case DataType::FLOAT64:
    return Func<DataType::FLOAT64>{}(std::forward<Args>(args)...);
  default:
    throw std::runtime_error("Operation not supported for this type");
  }
}

#endif