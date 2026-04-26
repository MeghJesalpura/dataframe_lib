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
  // (e.g. for ScanNode — we would need to read the file).
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

  // ── Constant folding & expression simplification helpers ──────────────────

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
      b.Append(0);
      std::shared_ptr<arrow::Array> arr;
      b.Finish(&arr);
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
  //   • folds pure-constant sub-trees into a single lit()
  //   • applies boolean identity / zero rules
  //   • applies arithmetic identity / zero rules
  //   • eliminates double negation  NOT(NOT(X)) → X
  static ExprPtr simplifyExpr(const ExprPtr &expr)
  {
    // Whole sub-tree is constant → fold it immediately
    if (isConstExpr(expr))
      return foldConstant(expr);

    // ── BinOpExpr ──────────────────────────────────────────────────────────
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

    // ── RelOpExpr ─────────────────────────────────────────────────────────
    // Simplify children; no identity rules apply to relational ops.
    if (auto *r = dynamic_cast<const RelOpExpr *>(expr.get().get()))
      return std::make_shared<RelOpExpr>(
          simplifyExpr(r->getLeft()), simplifyExpr(r->getRight()), r->getOp());

    // ── BoolOpExpr ─────────────────────────────────────────────────────────
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

    // ── UnaryOpExpr ────────────────────────────────────────────────────────
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

  // ── End helpers ───────────────────────────────────────────────────────────

  planNode QueryOptimizer::optimize(planNode input)
  {
    // Step 1 — Recurse into children bottom-up so lower nodes are optimised
    //          before we apply rules at the current level.
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

    // Step 2 — Apply predicate pushdown rules at the current level.
    //
    // Rule 1: Filter → Select     (always safe)
    // Rule 2: Filter → Sort       (always safe)
    // Rule 3: Filter → WithColumn (safe when predicate doesn't reference new column)
    // Rule 4: Filter → Join       (depends on join type and which side owns the columns)
    //
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
            // ── Rule 1: Filter over Select ──────────────────────────────────
            if (auto *sel = std::get_if<SelectNode>(n.child.get()))
            {
              auto pushedFilter = FilterNode{.predicate = n.predicate, .child = sel->child};
              auto newSelect = SelectNode{.columns = sel->columns,
                                          .child = std::make_shared<planNode>(pushedFilter)};
              return optimize(newSelect);
            }

            // ── Rule 2: Filter over Sort ────────────────────────────────────
            if (auto *srt = std::get_if<SortNode>(n.child.get()))
            {
              auto pushedFilter = FilterNode{.predicate = n.predicate, .child = srt->child};
              auto newSort = SortNode{.sort_columns = srt->sort_columns,
                                      .ascending = srt->ascending,
                                      .child = std::make_shared<planNode>(pushedFilter)};
              return optimize(newSort);
            }

            // ── Rule 3: Filter over WithColumn ──────────────────────────────
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

            // ── Rule 4: Filter over Join ────────────────────────────────────
            // Push only to the side(s) that own all referenced columns.
            //
            // Join-type safety:
            //   inner  — safe to push to either or both sides
            //   left   — only safe to push to RIGHT (left rows always preserved)
            //   right  — only safe to push to LEFT
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

                // Conservatively: unknown schema (empty set) → cannot push there
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

          // now will implement constant folding

          // now will implement expression simplification

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
}
