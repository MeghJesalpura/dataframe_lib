#ifndef EAGERDATAFRAME_H
#define EAGERDATAFRAME_H

#include "Types.h"
#include "expression/expr.h"
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
  std::shared_ptr<arrow::Table> table() const { return table_; }
  void print() const;

private:
  std::shared_ptr<arrow::Table> table_;
};
#endif