#ifndef EAGERDATAFRAME_H
#define EAGERDATAFRAME_H

#include "Types.h"
#include "expression/expr.h"
#include "expression/exprPtr.h"
#include <map>
#include <memory>
class EagerDataFrame
{
public:
  explicit EagerDataFrame(const std::shared_ptr<arrow::Table> &table) : table_(table) {}
  static EagerDataFrame read_csv(const std::string &path);
  static EagerDataFrame read_parquet(const std::string &path);

  static EagerDataFrame from_columns(
      const std::map<std::string, std::shared_ptr<arrow::ChunkedArray>> &cols);

  void write_csv(const std::string &path) const;
  void write_parquet(const std::string &path) const;
  std::shared_ptr<arrow::Table> table() const { return table_; };
  EagerDataFrame select(const std::vector<std::string> &colNames) const;
  EagerDataFrame filter(const ExprPtr &predicate) const;
  void print() const;

private:
  std::shared_ptr<arrow::Table> table_;
};
#endif