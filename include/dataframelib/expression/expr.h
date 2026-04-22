#ifndef EXPR_H
#define EXPR_H
#include "../Types.h"
#include <memory>
#include <arrow/api.h>

// Abstract base class for expressions
class Expr
{
public:
  virtual ~Expr() = default;
  virtual std::shared_ptr<arrow::ChunkedArray> evaluate(const std::shared_ptr<arrow::Table> &table) const = 0;
  virtual DataType resultType(const std::shared_ptr<arrow::Schema> &schema) const = 0;
  virtual std::string toString() const = 0;
};

#endif