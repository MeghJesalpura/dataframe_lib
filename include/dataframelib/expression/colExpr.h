#ifndef COLEXPR_H
#define COLEXPR_H

#include "expr.h"
// essential functionality to get a column from the table and return it as a ChunkedArray
namespace dataframelib
{
  class ColExpr : public Expr
  {
    std::string colName_;

  public:
    explicit ColExpr(std::string name) : colName_(std::move(name)) {}

    std::shared_ptr<arrow::ChunkedArray> evaluate(
        const std::shared_ptr<arrow::Table> &table) const override
    {
      auto col = table->GetColumnByName(colName_);
      if (!col)
        throw std::runtime_error("Column not found: " + colName_);
      return col;
    }

    DataType resultType(const std::shared_ptr<arrow::Schema> &schema) const override
    {
      auto field = schema->GetFieldByName(colName_);
      if (!field)
        throw std::runtime_error("Column not found: " + colName_);
      return fromArrowType(field->type());
    }

    std::string toString() const override { return "col(" + colName_ + ")"; }
  };

  // Factory function - this is what users call
  inline ExprPtr col(const std::string &name)
  {
    return std::make_shared<ColExpr>(name);
  };
} // namespace dataframelib
#endif