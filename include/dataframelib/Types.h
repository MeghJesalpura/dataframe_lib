#ifndef TYPES_H
#define TYPES_H

#include <memory>
#include <arrow/api.h>
enum class DataType
{
  INT32,
  INT64,
  FLOAT32,
  FLOAT64,
  STRING,
  BOOLEAN
};

enum class BinaryOp
{
  ADD,
  SUB,
  MUL,
  DIV,
  MOD
};

enum class UnaryOp
{
  ABS,
  NOT,
  IS_NULL,
  IS_NOT_NULL
};

enum class RelOp
{
  EQ,
  NEQ,
  LT,
  LTE,
  GT,
  GTE
};

enum class BoolOp
{
  AND,
  OR
};

enum class StringUnOp
{
  LENGTH,
  TO_UPPER,
  TO_LOWER,
};

enum class StringBinOp
{
  CONTAINS,
  STARTS_WITH,
  ENDS_WITH
};

// Function to convert our DataType enum to Arrow's DataType
inline std::shared_ptr<arrow::DataType> toArrowType(DataType dt)
{
  switch (dt)
  {
  case DataType::INT32:
    return arrow::int32();
  case DataType::INT64:
    return arrow::int64();
  case DataType::FLOAT32:
    return arrow::float32();
  case DataType::FLOAT64:
    return arrow::float64();
  case DataType::STRING:
    return arrow::utf8();
  case DataType::BOOLEAN:
    return arrow::boolean();
  }
}

// Function to convert Arrow's DataType to our DataType enum
inline DataType fromArrowType(const std::shared_ptr<arrow::DataType> &arrowType)
{
  switch (arrowType->id())
  {
  case arrow::Type::INT32:
    return DataType::INT32;
  case arrow::Type::INT64:
    return DataType::INT64;
  case arrow::Type::FLOAT:
    return DataType::FLOAT32;
  case arrow::Type::DOUBLE:
    return DataType::FLOAT64;
  case arrow::Type::STRING:
    return DataType::STRING;
  case arrow::Type::BOOL:
    return DataType::BOOLEAN;
  default:
    throw std::runtime_error("Unsupported Arrow data type");
  }
}

// to promote to a common type for binary operations
inline DataType promoteTypes(DataType a, DataType b)
{
  if (a == b)
    return a;

  bool aIsFloat = (a == DataType::FLOAT32 || a == DataType::FLOAT64);
  bool bIsFloat = (b == DataType::FLOAT32 || b == DataType::FLOAT64);

  if (aIsFloat || bIsFloat)
  {
    return DataType::FLOAT64; // Promote to FLOAT64 if either is a float
  }

  if (a == DataType::INT64 || b == DataType::INT64)
  {
    return DataType::INT64; // Promote to INT64 if either is INT64
  }
}

inline bool isNumeric(DataType dt)
{
  return dt == DataType::INT32 || dt == DataType::INT64 ||
         dt == DataType::FLOAT32 || dt == DataType::FLOAT64;
}

// Function to check if two data types are compatible for operations
inline void assertCompatible(DataType a, DataType b)
{
  if (isNumeric(a) && isNumeric(b))
    return; // Numeric types are compatible with each other

  throw std::runtime_error("Incompatible data types: " + std::to_string(static_cast<int>(a)) +
                           " and " + std::to_string(static_cast<int>(b)));
}

#endif