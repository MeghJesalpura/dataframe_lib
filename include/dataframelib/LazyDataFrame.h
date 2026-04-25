#ifndef LAZYDATAFRAME_H
#define LAZYDATAFRAME_H
#include "Types.h"
#include "expression/expr.h"
#include "expression/exprPtr.h"
#include "EagerDataFrame.h"
#include "lazy/nodes.h"

namespace dataframelib
{
  class LazyDataFrame
  {
  private:
    planNode rootNode_;

  public:
    // static LazyDataFrame from_columns(
    //     const std::map<std::string, std::shared_ptr<arrow::ChunkedArray>> &cols);

    explicit LazyDataFrame(planNode rootNode) : rootNode_(std::move(rootNode)) {}
    void sink_csv(const std::string &path) const;
    void sink_parquet(const std::string &path) const;

    LazyDataFrame select(const std::vector<std::string> &colNames) const;
    LazyDataFrame filter(const ExprPtr &predicate) const;
    // with_column eft to be implemented
    // LazyDataFrame with_column(const std::string &name, const ExprPtr &expr) const;
    LazyDataFrame group_by(const std::vector<std::string> &groupCols) const;
    LazyDataFrame aggregate(const std::map<std::string, std::string> &aggMap) const;
    LazyDataFrame join(const LazyDataFrame &other, const std::vector<std::string> &onCols, const std::string &how = "inner") const;
    LazyDataFrame sort(const std::vector<std::string> &sortCols, bool ascending = true) const;
    LazyDataFrame head(size_t n) const;
    EagerDataFrame collect() const;              // executes the plan and returns an EagerDataFrame
    void explain(const std::string &path) const; // dumps the DAG
  };

  LazyDataFrame scan_csv(const std::string &path);
  LazyDataFrame scan_parquet(const std::string &path);
}
#endif