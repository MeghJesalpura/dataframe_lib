#include "../include/dataframelib/QueryExecutor.h"
#include "../include/dataframelib/EagerDataFrame.h"
#include "../include/dataframelib/groupByObj.h"

namespace dataframelib
{
  EagerDataFrame QueryExecutor::execute(const planNode &node)
  {
    // this function would use std::visit to recursively execute the plan based on the type of node
    EagerDataFrame result = std::visit(
        [](const auto &n) -> EagerDataFrame
        {
          using NodeType = std::decay_t<decltype(n)>;
          if constexpr (std::is_same_v<NodeType, ScanNode>)
          {
            if (n.is_csv)
              return EagerDataFrame::read_csv(n.file_path);
            else
              return EagerDataFrame::read_parquet(n.file_path);
          }
          else if constexpr (std::is_same_v<NodeType, FilterNode>)
          {
            auto childDF = execute(*n.child);
            return childDF.filter(n.predicate);
          }
          else if constexpr (std::is_same_v<NodeType, SelectNode>)
          {
            auto childDF = execute(*n.child);
            return childDF.select(n.columns);
          }
          else if constexpr (std::is_same_v<NodeType, WithColumnNode>)
          {
            auto childDF = execute(*n.child);
            return childDF.with_column(n.column_name, n.expr);
          }
          else if constexpr (std::is_same_v<NodeType, GroupByNode>)
          {
            throw std::runtime_error("GroupByNode must be followed by AggNode");
          }
          else if constexpr (std::is_same_v<NodeType, AggNode>)
          {
            const auto &groupByNode = std::get<GroupByNode>(*n.child);
            auto childDF = execute(*groupByNode.child);
            return childDF.group_by(groupByNode.group_columns).aggregate(n.agg_map);
          }
          else if constexpr (std::is_same_v<NodeType, JoinNode>)
          {
            auto leftDF = execute(*n.left);
            auto rightDF = execute(*n.right);
            return leftDF.join(rightDF, n.on_columns, n.how);
          }
          else if constexpr (std::is_same_v<NodeType, SortNode>)
          {
            auto childDF = execute(*n.child);
            return childDF.sort(n.sort_columns, n.ascending);
          }
          else if constexpr (std::is_same_v<NodeType, HeadNode>)
          {
            auto childDF = execute(*n.child);
            return childDF.head(n.n);
          }
          // other node types to be implemented similarly
          else
          {
            throw std::runtime_error("Incorrect function called in QueryExecutor for node type");
          }
        },
        node);
    return result;
  }
}