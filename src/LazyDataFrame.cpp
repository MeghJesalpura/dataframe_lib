#include "../include/dataframelib/LazyDataFrame.h"
#include "../include/dataframelib/QueryExecutor.h"
#include "../include/dataframelib/QueryOptimizer.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include "../include/dataframelib/utils/graphRender.h"
namespace dataframelib
{
  // function to create LazyDataFrame from CSV file
  LazyDataFrame scan_csv(const std::string &path)
  {
    return LazyDataFrame(ScanNode{.file_path = path, .is_csv = true});
  }

  // function to create LazyDataFrame from Parquet file
  LazyDataFrame scan_parquet(const std::string &path)
  {
    return LazyDataFrame(ScanNode{.file_path = path, .is_csv = false});
  }

  // function to write LazyDataFrame to CSV file — this would execute the plan first
  void LazyDataFrame::sink_csv(const std::string &path) const
  {
    // this would first execute the plan and get an EagerDataFrame, then call write_csv on it
    collect().write_csv(path);
  }

  // function to write LazyDataFrame to Parquet file — this would execute the plan first
  void LazyDataFrame::sink_parquet(const std::string &path) const
  {
    // this would first execute the plan and get an EagerDataFrame, then call write_parquet on it
    collect().write_parquet(path);
  }

  // select columns — this would add a SelectNode on top of the current plan
  LazyDataFrame LazyDataFrame::select(const std::vector<std::string> &colNames) const
  {
    return LazyDataFrame(SelectNode{.columns = colNames, .child = std::make_shared<planNode>(rootNode_)});
  }

  // filter rows — this would add a FilterNode on top of the current plan
  LazyDataFrame LazyDataFrame::filter(const ExprPtr &predicate) const
  {
    return LazyDataFrame(FilterNode{.predicate = predicate, .child = std::make_shared<planNode>(rootNode_)});
  }

  // add a new column — this would add a WithColumnNode on top of the current plan
  LazyDataFrame LazyDataFrame::group_by(const std::vector<std::string> &groupCols) const
  {
    return LazyDataFrame(GroupByNode{.group_columns = groupCols, .child = std::make_shared<planNode>(rootNode_)});
  }

  // add a new column — this would add a WithColumnNode on top of the current plan
  LazyDataFrame LazyDataFrame::with_column(const std::string &name, const ExprPtr &expr) const
  {
    return LazyDataFrame(WithColumnNode{
        .column_name = name,
        .expr = expr,
        .child = std::make_shared<planNode>(rootNode_)});
  }

  // add an aggregate — this would add an AggNode on top of the current plan
  LazyDataFrame LazyDataFrame::aggregate(const std::vector<std::pair<std::string, std::string>> &aggList) const
  {
    return LazyDataFrame(AggNode{.agg_map = aggList, .child = std::make_shared<planNode>(rootNode_)});
  }

  // join with another LazyDataFrame - this would create a JoinNode with the two plans as children
  LazyDataFrame LazyDataFrame::join(const LazyDataFrame &other, const std::vector<std::string> &onCols, const std::string &how) const
  {
    return LazyDataFrame(JoinNode{
        .left = std::make_shared<planNode>(rootNode_),
        .right = std::make_shared<planNode>(other.rootNode_),
        .on_columns = onCols,
        .how = how});
  }

  // sort by columns - this would add a SortNode on top of the current plan
  LazyDataFrame LazyDataFrame::sort(const std::vector<std::string> &sortCols, bool ascending) const
  {
    return LazyDataFrame(SortNode{
        .sort_columns = sortCols,
        .ascending = ascending,
        .child = std::make_shared<planNode>(rootNode_)});
  }

  // gets top n rows
  LazyDataFrame LazyDataFrame::head(size_t n) const
  {
    return LazyDataFrame(HeadNode{
        .n = n,
        .child = std::make_shared<planNode>(rootNode_)});
  }

  // optimizes the plan and executes it to get an EagerDataFrame
  EagerDataFrame LazyDataFrame::collect() const
  {
    planNode optimized = QueryOptimizer::optimize(rootNode_);
    planNode projected = QueryOptimizer::pushdownProjections(optimized);
    return QueryExecutor::execute(projected);
  }

  // generates a visualization of the execution plan DAG and dumps it to the specified path
  void LazyDataFrame::explain(const std::string &path) const
  {
    auto optimized = QueryOptimizer::optimize(rootNode_);
    auto projected = QueryOptimizer::pushdownProjections(optimized);
    // Step 1 — generate .dot file
    std::string dotPath = path + ".dot";
    std::ofstream out(dotPath);

    out << "digraph {\n";
    out << "  rankdir=BT;\n"; // bottom to top — scan at bottom, result at top
    out << "  node [shape=box, style=filled, fillcolor=lightblue];\n";

    int nodeId = 0;
    writeDot(out, projected, nodeId);

    out << "}\n";
    out.close();

    // Step 2 - call graphviz to render
    std::string cmd = "dot -Tpng " + dotPath + " -o " + path;
    int ret = system(cmd.c_str());
    if (ret != 0)
      throw std::runtime_error("Graphviz failed — is 'dot' installed?");
  }
}