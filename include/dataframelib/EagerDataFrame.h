#ifndef EAGERDATAFRAME_H
#define EAGERDATAFRAME_H

#include "Types.h"
#include "expression/expr.h"
#include "expression/exprPtr.h"
#include "groupByObj.h"
#include <map>
#include <memory>

namespace dataframelib
{
  class EagerDataFrame
  {
  public:
    explicit EagerDataFrame(const std::shared_ptr<arrow::Table> &table) : table_(table) {}
    static EagerDataFrame read_csv(const std::string &path);
    static EagerDataFrame read_csv(const std::string &path, const std::vector<std::string> &columns);
    static EagerDataFrame read_parquet(const std::string &path);
    static EagerDataFrame read_parquet(const std::string &path, const std::vector<std::string> &columns);
    static EagerDataFrame from_columns(
        const std::map<std::string, std::shared_ptr<arrow::ChunkedArray>> &cols);
    static EagerDataFrame from_columns(
        const std::vector<std::pair<std::string, std::shared_ptr<arrow::Array>>> &cols);

    void write_csv(const std::string &path) const;
    void write_parquet(const std::string &path) const;
    std::shared_ptr<arrow::Table> table() const { return table_; };
    int64_t num_rows() const { return table_->num_rows(); }
    int num_columns() const { return table_->num_columns(); }
    EagerDataFrame select(const std::vector<std::string> &colNames) const;
    EagerDataFrame filter(const ExprPtr &predicate) const;
    EagerDataFrame with_column(const std::string &name, const ExprPtr &expr) const;
    EagerDataFrame head(size_t n) const;
    GroupByObj group_by(const std::vector<std::string> &colNames) const;
    EagerDataFrame sort(const std::vector<std::string> &colNames, bool ascending = true) const;
    EagerDataFrame join(const EagerDataFrame &other,
                        const std::vector<std::string> &onColumns,
                        const std::string &how = "inner") const;
    void print() const;

  private:
    // The table storing the data for this DataFrame. All operations will produce new tables based on this one.
    std::shared_ptr<arrow::Table> table_;
  };
}

#endif