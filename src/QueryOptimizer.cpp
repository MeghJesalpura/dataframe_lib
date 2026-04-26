#include "../include/dataframelib/QueryOptimizer.h"
#include <algorithm>
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

          return n; // no rule matched (or not a FilterNode)
        },
        input);
  }
}
