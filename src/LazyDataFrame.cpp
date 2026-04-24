#include "../include/dataframelib/LazyDataFrame.h"
#include "../include/dataframelib/QueryExecutor.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include "../include/dataframelib/utils/graphRender.h"
LazyDataFrame scan_csv(const std::string &path)
{
  return LazyDataFrame(ScanNode{.file_path = path, .is_csv = true});
}

LazyDataFrame scan_parquet(const std::string &path)
{
  return LazyDataFrame(ScanNode{.file_path = path, .is_csv = false});
}

void LazyDataFrame::sink_csv(const std::string &path) const
{
  // this would first execute the plan and get an EagerDataFrame, then call write_csv on it
  collect().write_csv(path);
}

void LazyDataFrame::sink_parquet(const std::string &path) const
{
  // this would first execute the plan and get an EagerDataFrame, then call write_parquet on it
  collect().write_parquet(path);
}

LazyDataFrame LazyDataFrame::select(const std::vector<std::string> &colNames) const
{
  return LazyDataFrame(SelectNode{.columns = colNames, .child = std::make_shared<planNode>(rootNode_)});
}

LazyDataFrame LazyDataFrame::filter(const ExprPtr &predicate) const
{
  return LazyDataFrame(FilterNode{.predicate = predicate, .child = std::make_shared<planNode>(rootNode_)});
}

LazyDataFrame LazyDataFrame::group_by(const std::vector<std::string> &groupCols) const
{
  return LazyDataFrame(GroupByNode{.group_columns = groupCols, .child = std::make_shared<planNode>(rootNode_)});
}

LazyDataFrame LazyDataFrame::aggregate(const std::map<std::string, std::string> &aggMap) const
{
  return LazyDataFrame(AggNode{.agg_map = aggMap, .child = std::make_shared<planNode>(rootNode_)});
}

LazyDataFrame LazyDataFrame::join(const LazyDataFrame &other, const std::vector<std::string> &onCols, const std::string &how) const
{
  return LazyDataFrame(JoinNode{
      .left = std::make_shared<planNode>(rootNode_),
      .right = std::make_shared<planNode>(other.rootNode_),
      .on_columns = onCols,
      .how = how});
}

LazyDataFrame LazyDataFrame::sort(const std::vector<std::string> &sortCols, bool ascending) const
{
  return LazyDataFrame(SortNode{
      .sort_columns = sortCols,
      .ascending = ascending,
      .child = std::make_shared<planNode>(rootNode_)});
}

LazyDataFrame LazyDataFrame::head(size_t n) const
{
  return LazyDataFrame(HeadNode{
      .n = n,
      .child = std::make_shared<planNode>(rootNode_)});
}

EagerDataFrame LazyDataFrame::collect() const
{
  // Ideally we need to optimize the plan - Query optimizer is yet to be implemented
  // this is where we would execute the plan represented by rootNode_ and return an EagerDataFrame
  // post order traversal of the DAG
  return QueryExecutor::execute(rootNode_);
}

void LazyDataFrame::explain(const std::string &path) const
{
  // Step 1 — generate .dot file
  std::string dotPath = path + ".dot";
  std::ofstream out(dotPath);

  out << "digraph {\n";
  out << "  rankdir=BT;\n"; // bottom to top — scan at bottom, result at top
  out << "  node [shape=box, style=filled, fillcolor=lightblue];\n";

  int nodeId = 0;
  writeDot(out, rootNode_, nodeId);

  out << "}\n";
  out.close();

  // Step 2 — call graphviz to render
  std::string cmd = "dot -Tpng " + dotPath + " -o " + path;
  int ret = system(cmd.c_str());
  if (ret != 0)
    throw std::runtime_error("Graphviz failed — is 'dot' installed?");
}