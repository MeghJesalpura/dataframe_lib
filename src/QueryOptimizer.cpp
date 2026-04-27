#include "../include/dataframelib/QueryOptimizer.h"
#include "../include/dataframelib/expression/litExpr.h"
#include "../include/dataframelib/operations/binOpExpr.h"
#include "../include/dataframelib/operations/boolOpExpr.h"
#include "../include/dataframelib/operations/relOpExpr.h"
#include "../include/dataframelib/operations/unaryOpExpr.h"
#include "../include/dataframelib/utils/arrayUtils.h"
#include <arrow/api.h>
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace dataframelib
{
  // Extract every column name referenced as col(<name>) in an expression string.
  static std::unordered_set<std::string> extractReferencedColumns(const std::string &predStr)
  {
    std::unordered_set<std::string> cols;
    const std::string prefix = "col(";
    size_t pos = 0;
    while ((pos = predStr.find(prefix, pos)) != std::string::npos)
    {
      pos += prefix.size();
      size_t end = predStr.find(')', pos);
      if (end != std::string::npos)
      {
        cols.insert(predStr.substr(pos, end - pos));
        pos = end + 1;
      }
    }
    return cols;
  }

  // Infer the set of output column names for a plan subtree.
  // Returns an empty set when the schema cannot be determined statically
  static std::unordered_set<std::string> getOutputColumns(const planNode &node)
  {
    return std::visit(
        [](const auto &n) -> std::unordered_set<std::string>
        {
          using T = std::decay_t<decltype(n)>;

          if constexpr (std::is_same_v<T, ScanNode>)
            return {}; // schema unknown without reading the file

          else if constexpr (std::is_same_v<T, SelectNode>)
            return {n.columns.begin(), n.columns.end()};

          // These nodes pass through the same column set as their child
          else if constexpr (std::is_same_v<T, FilterNode> ||
                             std::is_same_v<T, SortNode> ||
                             std::is_same_v<T, HeadNode>)
            return getOutputColumns(*n.child);

          // WithColumn adds one new column; propagate only if child schema is known
          else if constexpr (std::is_same_v<T, WithColumnNode>)
          {
            auto cols = getOutputColumns(*n.child);
            if (!cols.empty())
              cols.insert(n.column_name);
            return cols;
          }

          // GroupBy, Agg, Join: schema depends on runtime data or is complex — unknown
          return {};
        },
        node);
  }

  // True when the expression has no col() references and can be evaluated
  // without a real table.
  static bool isConstExpr(const ExprPtr &expr)
  {
    return expr.toString().find("col(") == std::string::npos;
  }

  // Lazily construct a 1-row dummy table used to evaluate constant expressions.
  static const std::shared_ptr<arrow::Table> &dummyTable()
  {
    static const auto tbl = []()
    {
      auto schema = arrow::schema({arrow::field("_", arrow::int32())});
      arrow::Int32Builder b;
      auto appendStatus = b.Append(0);
      if (!appendStatus.ok())
        throw std::runtime_error(appendStatus.ToString());
      std::shared_ptr<arrow::Array> arr;
      auto finishStatus = b.Finish(&arr);
      if (!finishStatus.ok())
        throw std::runtime_error(finishStatus.ToString());
      return arrow::Table::Make(schema, {std::make_shared<arrow::ChunkedArray>(arr)});
    }();
    return tbl;
  }

  // Evaluate a constant expression to a lit() ExprPtr.
  // Returns the original if folding is not possible.
  static ExprPtr foldConstant(const ExprPtr &expr)
  {
    if (!isConstExpr(expr))
      return expr;
    try
    {
      auto &tbl = dummyTable();
      DataType dt = expr.resultType(tbl->schema());
      auto flat = flatten(expr.evaluate(tbl));
      switch (dt)
      {
      case DataType::BOOLEAN:
        return lit(std::static_pointer_cast<arrow::BooleanArray>(flat)->Value(0));
      case DataType::INT32:
        return lit(std::static_pointer_cast<arrow::Int32Array>(flat)->Value(0));
      case DataType::INT64:
        return lit(std::static_pointer_cast<arrow::Int64Array>(flat)->Value(0));
      case DataType::FLOAT32:
        return lit(std::static_pointer_cast<arrow::FloatArray>(flat)->Value(0));
      case DataType::FLOAT64:
        return lit(std::static_pointer_cast<arrow::DoubleArray>(flat)->Value(0));
      default:
        return expr;
      }
    }
    catch (...)
    {
      return expr;
    }
  }

  // Check if expr is a constant boolean equal to expected.
  static bool isBoolConst(const ExprPtr &expr, bool expected)
  {
    if (!isConstExpr(expr))
      return false;
    try
    {
      auto &tbl = dummyTable();
      if (expr.resultType(tbl->schema()) != DataType::BOOLEAN)
        return false;
      auto flat = flatten(expr.evaluate(tbl));
      return std::static_pointer_cast<arrow::BooleanArray>(flat)->Value(0) == expected;
    }
    catch (...)
    {
      return false;
    }
  }

  // Check if expr is a constant numeric value equal to expected (within 1e-10).
  static bool isNumericConst(const ExprPtr &expr, double expected)
  {
    if (!isConstExpr(expr))
      return false;
    try
    {
      auto &tbl = dummyTable();
      DataType dt = expr.resultType(tbl->schema());
      auto flat = flatten(expr.evaluate(tbl));
      double v = 0;
      switch (dt)
      {
      case DataType::INT32:
        v = std::static_pointer_cast<arrow::Int32Array>(flat)->Value(0);
        break;
      case DataType::INT64:
        v = static_cast<double>(std::static_pointer_cast<arrow::Int64Array>(flat)->Value(0));
        break;
      case DataType::FLOAT32:
        v = std::static_pointer_cast<arrow::FloatArray>(flat)->Value(0);
        break;
      case DataType::FLOAT64:
        v = std::static_pointer_cast<arrow::DoubleArray>(flat)->Value(0);
        break;
      default:
        return false;
      }
      return std::abs(v - expected) < 1e-10;
    }
    catch (...)
    {
      return false;
    }
  }

  // Recursively simplify an expression tree:
  // folds pure-constant sub-trees into a single lit()
  // applies boolean identity / zero rules
  // applies arithmetic identity / zero rules
  // eliminates double negation  NOT(NOT(X)) goes to X
  static ExprPtr simplifyExpr(const ExprPtr &expr)
  {
    // Whole sub-tree is constant to fold it immediately
    if (isConstExpr(expr))
      return foldConstant(expr);

    // BinOpExpr
    if (auto *b = dynamic_cast<const BinOpExpr *>(expr.get().get()))
    {
      ExprPtr l = simplifyExpr(b->getLeft());
      ExprPtr r = simplifyExpr(b->getRight());
      BinaryOp op = b->getOp();

      switch (op)
      {
      case BinaryOp::ADD:
        if (isNumericConst(r, 0.0))
          return l; // x + 0 = x
        if (isNumericConst(l, 0.0))
          return r; // 0 + x = x
        break;
      case BinaryOp::SUB:
        if (isNumericConst(r, 0.0))
          return l; // x - 0 = x
        break;
      case BinaryOp::MUL:
        if (isNumericConst(r, 1.0))
          return l; // x * 1 = x
        if (isNumericConst(l, 1.0))
          return r; // 1 * x = x
        if (isNumericConst(r, 0.0))
          return foldConstant(lit(0)); // x * 0 = 0
        if (isNumericConst(l, 0.0))
          return foldConstant(lit(0)); // 0 * x = 0
        break;
      case BinaryOp::DIV:
        if (isNumericConst(r, 1.0))
          return l; // x / 1 = x
        break;
      default:
        break;
      }
      return std::make_shared<BinOpExpr>(l, r, op);
    }

    // RelOpExpr
    // Simplify children; no identity rules apply to relational ops.
    if (auto *r = dynamic_cast<const RelOpExpr *>(expr.get().get()))
      return std::make_shared<RelOpExpr>(
          simplifyExpr(r->getLeft()), simplifyExpr(r->getRight()), r->getOp());

    // BoolOpExpr
    if (auto *b = dynamic_cast<const BoolOpExpr *>(expr.get().get()))
    {
      ExprPtr l = simplifyExpr(b->getLeft());
      ExprPtr r = simplifyExpr(b->getRight());
      BoolOp op = b->getOp();

      if (op == BoolOp::AND)
      {
        if (isBoolConst(l, true))
          return r; // true & X = X
        if (isBoolConst(r, true))
          return l; // X & true = X
        if (isBoolConst(l, false))
          return lit(false); // false & X = false
        if (isBoolConst(r, false))
          return lit(false); // X & false = false
      }
      else // OR
      {
        if (isBoolConst(l, false))
          return r; // false | X = X
        if (isBoolConst(r, false))
          return l; // X | false = X
        if (isBoolConst(l, true))
          return lit(true); // true | X = true
        if (isBoolConst(r, true))
          return lit(true); // X | true = true
      }
      return std::make_shared<BoolOpExpr>(l, r, op);
    }

    if (auto *u = dynamic_cast<const UnaryOpExpr *>(expr.get().get()))
    {
      ExprPtr operand = simplifyExpr(u->getOperand());
      UnaryOp op = u->getOp();

      // NOT(NOT(X)) = X
      if (op == UnaryOp::NOT)
      {
        if (auto *inner = dynamic_cast<const UnaryOpExpr *>(operand.get().get()))
          if (inner->getOp() == UnaryOp::NOT)
            return inner->getOperand();
      }
      return std::make_shared<UnaryOpExpr>(operand, op);
    }

    // ColExpr, LitExpr, string ops — already in simplest form
    return expr;
  }

  // Projection Pushdown
  // Propagates the set of columns actually needed (requiredCols) downward
  // through the plan tree so that each node only produces what the nodes above
  // it will consume.  An empty requiredCols means "all columns needed" (used
  // at the root where there is no outer constraint).

  static planNode propagateProjection(
      const planNode &node,
      const std::unordered_set<std::string> &requiredCols)
  {
    return std::visit(
        [&requiredCols](const auto &n) -> planNode
        {
          using T = std::decay_t<decltype(n)>;

          // ScanNode
          // Leave scan unchanged — read_csv/read_parquet always reads all
          // columns from disk regardless, so inserting a SelectNode here
          // only adds an extra in-memory pass with no I/O savings.
          if constexpr (std::is_same_v<T, ScanNode>)
            return n;

          // SelectNode
          // Narrow the column list to the intersection with requiredCols, then
          // propagate the narrowed list downward.
          else if constexpr (std::is_same_v<T, SelectNode>)
          {
            std::vector<std::string> cols;
            if (requiredCols.empty())
            {
              cols = n.columns; // no outer constraint — keep as-is
            }
            else
            {
              for (const auto &c : n.columns)
                if (requiredCols.count(c))
                  cols.push_back(c);
              if (cols.empty())
                cols = n.columns; // safety: never reduce to nothing
            }
            std::unordered_set<std::string> neededBelow(cols.begin(), cols.end());
            return SelectNode{
                .columns = cols,
                .child = std::make_shared<planNode>(
                    propagateProjection(*n.child, neededBelow))};
          }

          // FilterNode
          // Only add predicate columns to an already-constrained set.
          // If requiredCols is empty ("all needed"), propagate empty unchanged —
          // introducing {pred_cols} here would restrict the scan to 1 column.
          else if constexpr (std::is_same_v<T, FilterNode>)
          {
            std::unordered_set<std::string> neededBelow = requiredCols;
            if (!requiredCols.empty())
            {
              auto predCols = extractReferencedColumns(n.predicate.toString());
              neededBelow.insert(predCols.begin(), predCols.end());
            }
            return FilterNode{
                .predicate = n.predicate,
                .child = std::make_shared<planNode>(
                    propagateProjection(*n.child, neededBelow))};
          }

          // SortNode
          //  Same invariant: only add sort-key columns to an existing constraint.
          else if constexpr (std::is_same_v<T, SortNode>)
          {
            std::unordered_set<std::string> neededBelow = requiredCols;
            if (!requiredCols.empty())
              for (const auto &c : n.sort_columns)
                neededBelow.insert(c);
            return SortNode{
                .sort_columns = n.sort_columns,
                .ascending = n.ascending,
                .child = std::make_shared<planNode>(
                    propagateProjection(*n.child, neededBelow))};
          }

          // HeadNode
          else if constexpr (std::is_same_v<T, HeadNode>)
          {
            return HeadNode{
                .n = n.n,
                .child = std::make_shared<planNode>(
                    propagateProjection(*n.child, requiredCols))};
          }

          // WithColumnNode
          // If the new column is not consumed by anything above, drop the node.
          // Otherwise, extend the required set with the expression's inputs.
          else if constexpr (std::is_same_v<T, WithColumnNode>)
          {
            if (!requiredCols.empty() && !requiredCols.count(n.column_name))
              return propagateProjection(*n.child, requiredCols); // column unused

            auto exprCols = extractReferencedColumns(n.expr.toString());
            std::unordered_set<std::string> neededBelow = requiredCols;
            if (!requiredCols.empty())
            {
              neededBelow.erase(n.column_name); // produced here, not needed from child
              neededBelow.insert(exprCols.begin(), exprCols.end());
            }
            return WithColumnNode{
                .column_name = n.column_name,
                .expr = n.expr,
                .child = std::make_shared<planNode>(
                    propagateProjection(*n.child, neededBelow))};
          }

          // AggNode
          // Trim agg_map to aggregations whose output column is in requiredCols,
          // then propagate source columns + group keys into the GroupByNode.
          else if constexpr (std::is_same_v<T, AggNode>)
          {
            auto *gbn = std::get_if<GroupByNode>(n.child.get());
            if (!gbn)
              return n; // malformed plan — leave untouched

            // Trim: keep only agg entries whose output (col_op) is needed above
            std::vector<std::pair<std::string, std::string>> trimmedAgg;
            if (requiredCols.empty())
            {
              trimmedAgg = n.agg_map;
            }
            else
            {
              for (const auto &[col, op] : n.agg_map)
                if (requiredCols.count(col + "_" + op))
                  trimmedAgg.push_back({col, op});
              if (trimmedAgg.empty())
                trimmedAgg = n.agg_map; // safety
            }

            // Below the GroupBy we need: group keys + source cols for kept aggs
            std::unordered_set<std::string> neededForData(
                gbn->group_columns.begin(), gbn->group_columns.end());
            for (const auto &[col, op] : trimmedAgg)
              neededForData.insert(col);

            auto newGroupBy = GroupByNode{
                .group_columns = gbn->group_columns,
                .child = std::make_shared<planNode>(
                    propagateProjection(*gbn->child, neededForData))};

            return AggNode{
                .agg_map = trimmedAgg,
                .child = std::make_shared<planNode>(newGroupBy)};
          }

          // GroupByNode
          // Handled inside AggNode above.  In isolation (shouldn't happen in a
          // valid plan) just propagate the group-key columns downward.
          else if constexpr (std::is_same_v<T, GroupByNode>)
          {
            std::unordered_set<std::string> neededBelow = requiredCols;
            for (const auto &c : n.group_columns)
              neededBelow.insert(c);
            return GroupByNode{
                .group_columns = n.group_columns,
                .child = std::make_shared<planNode>(
                    propagateProjection(*n.child, neededBelow))};
          }

          // JoinNode
          // Split requiredCols between the two sides using static schema info.
          // Join-key columns are always required on both sides.
          // If either side's schema is unknown, fall back to "all needed".
          else if constexpr (std::is_same_v<T, JoinNode>)
          {
            auto leftSchema = getOutputColumns(*n.left);
            auto rightSchema = getOutputColumns(*n.right);

            std::unordered_set<std::string> neededLeft(
                n.on_columns.begin(), n.on_columns.end());
            std::unordered_set<std::string> neededRight(
                n.on_columns.begin(), n.on_columns.end());

            if (!requiredCols.empty() && !leftSchema.empty() && !rightSchema.empty())
            {
              for (const auto &col : requiredCols)
              {
                if (leftSchema.count(col))
                  neededLeft.insert(col);
                else if (rightSchema.count(col))
                  neededRight.insert(col);
                // col may be a renamed right-side column (col_right) — fall through,
                // the join key already ensures the right side is queried correctly
              }
            }
            else
            {
              neededLeft.clear(); // unknown schema — don't restrict
              neededRight.clear();
            }

            return JoinNode{
                .left = std::make_shared<planNode>(
                    propagateProjection(*n.left, neededLeft)),
                .right = std::make_shared<planNode>(
                    propagateProjection(*n.right, neededRight)),
                .on_columns = n.on_columns,
                .how = n.how};
          }

          return n; // fallback (should not be reached)
        },
        node);
  }

  planNode QueryOptimizer::optimize(planNode input)
  {
    // Recurse into children bottom-up so lower nodes are optimised
    //  before we apply rules at the current level.
    input = std::visit(
        [](auto n) -> planNode
        {
          using T = std::decay_t<decltype(n)>;
          if constexpr (std::is_same_v<T, ScanNode>)
            return n;
          else if constexpr (std::is_same_v<T, JoinNode>)
          {
            n.left = std::make_shared<planNode>(optimize(*n.left));
            n.right = std::make_shared<planNode>(optimize(*n.right));
            return n;
          }
          else
          {
            n.child = std::make_shared<planNode>(optimize(*n.child));
            // Apply constant folding / expression simplification to node-level expressions
            if constexpr (std::is_same_v<T, FilterNode>)
              n.predicate = simplifyExpr(n.predicate);
            if constexpr (std::is_same_v<T, WithColumnNode>)
              n.expr = simplifyExpr(n.expr);
            return n;
          }
        },
        input);

    // Apply predicate pushdown rules at the current level.
    // After each rewrite optimize() is called again so the filter can
    // continue propagating further down.

    return std::visit(
        [](const auto &n) -> planNode
        {
          using T = std::decay_t<decltype(n)>;

          // All pushdown rules require a FilterNode — wrap everything in
          // if constexpr so member accesses (n.child, n.predicate) are only
          // compiled when T really is FilterNode.
          if constexpr (std::is_same_v<T, FilterNode>)
          {
            // Filter over Select
            if (auto *sel = std::get_if<SelectNode>(n.child.get()))
            {
              auto pushedFilter = FilterNode{.predicate = n.predicate, .child = sel->child};
              auto newSelect = SelectNode{.columns = sel->columns,
                                          .child = std::make_shared<planNode>(pushedFilter)};
              return optimize(newSelect);
            }

            // Filter over Sort
            if (auto *srt = std::get_if<SortNode>(n.child.get()))
            {
              auto pushedFilter = FilterNode{.predicate = n.predicate, .child = srt->child};
              auto newSort = SortNode{.sort_columns = srt->sort_columns,
                                      .ascending = srt->ascending,
                                      .child = std::make_shared<planNode>(pushedFilter)};
              return optimize(newSort);
            }

            // Filter over WithColumn
            // Safe only when the predicate does not reference the new column.
            if (auto *wc = std::get_if<WithColumnNode>(n.child.get()))
            {
              const std::string colToken = "col(" + wc->column_name + ")";
              if (n.predicate.toString().find(colToken) == std::string::npos)
              {
                auto pushedFilter = FilterNode{.predicate = n.predicate, .child = wc->child};
                auto newWithCol = WithColumnNode{.column_name = wc->column_name,
                                                 .expr = wc->expr,
                                                 .child = std::make_shared<planNode>(pushedFilter)};
                return optimize(newWithCol);
              }
            }

            // Filter over Join
            // Push only to the side(s) that own all referenced columns.
            //
            // Join-type safety:
            //   inner  — safe to push to either or both sides
            //   left   — only safe to push to RIGHT (left rows always preserved)
            //   outer  — not safe to push to either side
            if (auto *jn = std::get_if<JoinNode>(n.child.get()))
            {
              bool canPushLeft = (jn->how == "inner" || jn->how == "right");
              bool canPushRight = (jn->how == "inner" || jn->how == "left");

              if (canPushLeft || canPushRight)
              {
                auto predCols = extractReferencedColumns(n.predicate.toString());
                auto leftCols = getOutputColumns(*jn->left);
                auto rightCols = getOutputColumns(*jn->right);

                // Conservatively: unknown schema (empty set) to cannot push there
                bool allInLeft = !leftCols.empty();
                bool allInRight = !rightCols.empty();
                for (const auto &col : predCols)
                {
                  if (!leftCols.count(col))
                    allInLeft = false;
                  if (!rightCols.count(col))
                    allInRight = false;
                }

                // Push to both only when every referenced column is a join key
                // (identical value on both sides — safe and beneficial).
                bool allAreJoinKeys = !predCols.empty() &&
                                      std::all_of(predCols.begin(), predCols.end(),
                                                  [&](const std::string &c)
                                                  {
                                                    return std::find(jn->on_columns.begin(),
                                                                     jn->on_columns.end(), c) !=
                                                           jn->on_columns.end();
                                                  });

                auto newLeft = jn->left;
                auto newRight = jn->right;
                bool pushed = false;

                if (allAreJoinKeys && canPushLeft && canPushRight)
                {
                  newLeft = std::make_shared<planNode>(
                      FilterNode{.predicate = n.predicate, .child = jn->left});
                  newRight = std::make_shared<planNode>(
                      FilterNode{.predicate = n.predicate, .child = jn->right});
                  pushed = true;
                }
                else if (allInLeft && !allInRight && canPushLeft)
                {
                  newLeft = std::make_shared<planNode>(
                      FilterNode{.predicate = n.predicate, .child = jn->left});
                  pushed = true;
                }
                else if (allInRight && !allInLeft && canPushRight)
                {
                  newRight = std::make_shared<planNode>(
                      FilterNode{.predicate = n.predicate, .child = jn->right});
                  pushed = true;
                }

                if (pushed)
                {
                  auto newJoin = JoinNode{.left = newLeft,
                                          .right = newRight,
                                          .on_columns = jn->on_columns,
                                          .how = jn->how};
                  return optimize(newJoin);
                }
              }
            }
          } // end if constexpr FilterNode

          // now will implement projection pushdown
          // read only the columns which are useful when going down
          // basically won't push down select but will try to push down select's column requirements to scan and join and group by and aggregate

          // now will implement limit pushdown - try to push head as down as possible similar to filter
          if constexpr (std::is_same_v<T, HeadNode>)
          {
            // cannot push down through sort and group and aggregate
            // and filter and join
            // can push down through with_column select

            if (auto *sel = std::get_if<SelectNode>(n.child.get()))
            {
              auto pushedHead = HeadNode{.n = n.n, .child = sel->child};
              auto newSelect = SelectNode{.columns = sel->columns,
                                          .child = std::make_shared<planNode>(pushedHead)};
              return optimize(newSelect);
            }
            else if (auto *wc = std::get_if<WithColumnNode>(n.child.get()))
            {
              auto pushedHead = HeadNode{.n = n.n, .child = wc->child};
              auto newWithCol = WithColumnNode{.column_name = wc->column_name,
                                               .expr = wc->expr,
                                               .child = std::make_shared<planNode>(pushedHead)};
              return optimize(newWithCol);
            }
          }

          return n; // no rule matched
        },
        input);
  }

  planNode QueryOptimizer::pushdownProjections(const planNode &input)
  {
    return propagateProjection(input, {}); // empty = all columns needed at root
  }
}
