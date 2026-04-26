#ifndef NODES_H
#define NODES_H
#include "../Types.h"
#include <iostream>
#include <map>
#include <utility>
#include <memory>
#include "arrow/api.h"
#include "../expression/exprPtr.h"
// using the sumtype concept from functional programming
namespace dataframelib
{
  struct ScanNode;
  struct FilterNode;
  struct SelectNode;
  struct WithColumnNode;
  struct GroupByNode;
  struct AggNode;
  struct JoinNode;
  struct SortNode;
  struct HeadNode;
  using planNode = std::variant<ScanNode, FilterNode, SelectNode, WithColumnNode, GroupByNode, AggNode, JoinNode, SortNode, HeadNode>;

  struct ScanNode
  {
    std::string file_path;
    bool is_csv = true;
  };

  struct FilterNode
  {
    ExprPtr predicate;
    std::shared_ptr<planNode> child;
  };

  struct SelectNode
  {
    std::vector<std::string> columns;
    std::shared_ptr<planNode> child;
  };

  struct WithColumnNode
  {
    std::string column_name;
    ExprPtr expr;
    std::shared_ptr<planNode> child;
  };

  struct GroupByNode
  {
    std::vector<std::string> group_columns;
    std::shared_ptr<planNode> child;
  };

  struct AggNode
  {
    std::vector<std::pair<std::string, std::string>> agg_map;
    std::shared_ptr<planNode> child;
  };

  struct JoinNode
  {
    std::shared_ptr<planNode> left;
    std::shared_ptr<planNode> right;
    std::vector<std::string> on_columns;
    std::string how = "inner";
  };

  struct SortNode
  {
    std::vector<std::string> sort_columns;
    bool ascending = true;
    std::shared_ptr<planNode> child;
  };

  struct HeadNode
  {
    size_t n;
    std::shared_ptr<planNode> child;
  };
} // namespace dataframelib
#endif