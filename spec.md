# DataFrameLib — Implementation Checklist

> COP290 Assignment 4 | Deadline: 27 April 2026, 11:59 PM

---

## Environment Setup

- [ ] Install Apache Arrow C++ via `vcpkg` or `conda` (see note below)
- [ ] Install Parquet support (`arrow-parquet` or `parquet-cpp`, usually bundled)
- [ ] Install Graphviz (`libgvc-dev` on Ubuntu, or `graphviz` via brew/conda)
- [ ] Set up CMake project with `find_package(Arrow REQUIRED)` and `find_package(Parquet REQUIRED)`
- [ ] Confirm a hello-world program that reads a CSV into `arrow::Table` and prints its schema compiles and runs
- [ ] Set up `clang-tidy` or `valgrind` for memory leak checking (needed for the 15% memory grade)

---

## Project Structure

- [ ] Define folder structure: `include/`, `src/`, `tests/`, `CMakeLists.txt`, `README.md`, `report.pdf`
- [ ] Create public headers: `EagerDataFrame.h`, `LazyDataFrame.h`, `QueryOptimizer.h`, `Expr.h`
- [ ] Ensure all public API signatures match the assignment exactly (names, argument types, return types)

---

## Phase 1 — Type System & Expression Tree

### Type System

- [ ] Define supported types: `int32`, `int64`, `float32`, `float64`, `string`, `boolean`
- [ ] Implement type promotion rule: `int + float = float`
- [ ] Throw errors on incompatible operations (e.g. numeric + string column)
- [ ] Represent missing values as Arrow `null` — never as `NaN`
- [ ] Any operation involving a null operand must produce a null result

### Expression Base Infrastructure

- [ ] Define abstract base class `Expr` with a virtual `evaluate(arrow::Table)` method
- [ ] `ColExpr` — references a column by name; throws if column not found
- [ ] `LitExpr` — wraps a scalar literal value (int, float, string, bool)
- [ ] `AliasExpr` — wraps another expr, renames result column

### Arithmetic Expressions

- [ ] `BinOpExpr` for `+`, `-`, `*`, `/` (element-wise, type-promoted)
- [ ] `BinOpExpr` for `%` (modulo, integer only)
- [ ] `UnaryExpr` for `abs()`

### Comparison & Boolean Expressions

- [ ] `==`, `!=`, `<`, `<=`, `>`, `>=` returning boolean arrays
- [ ] `&` (AND), `|` (OR), `~` (NOT) on boolean arrays
- [ ] `is_null()` — returns boolean array
- [ ] `is_not_null()` — returns boolean array

### String Expressions

- [ ] `length()` — length of each string element
- [ ] `contains(s)` — boolean, substring match
- [ ] `starts_with(s)` — boolean, prefix match
- [ ] `ends_with(s)` — boolean, suffix match
- [ ] `to_lower()` — lowercase each string
- [ ] `to_upper()` — uppercase each string

### Aggregation Expressions

- [ ] `sum()` — sum of non-null values
- [ ] `mean()` — mean of non-null values
- [ ] `count()` — count of non-null values
- [ ] `min()` — minimum value
- [ ] `max()` — maximum value

---

## Phase 2 — I/O Functions

- [ ] `read_csv(path)` → `EagerDataFrame`
- [ ] `read_parquet(path)` → `EagerDataFrame`
- [ ] `write_csv(path)` on `EagerDataFrame`
- [ ] `write_parquet(path)` on `EagerDataFrame`
- [ ] `from_columns(map)` — build `EagerDataFrame` from a column name → Arrow array map
- [ ] `scan_csv(path)` → `LazyDataFrame` (no data loaded yet, just records path)
- [ ] `scan_parquet(path)` → `LazyDataFrame` (no data loaded yet)
- [ ] `sink_csv(path)` on `LazyDataFrame` (calls `collect()` then writes)
- [ ] `sink_parquet(path)` on `LazyDataFrame`

---

## Phase 3 — EagerDataFrame Operations

Each operation immediately executes and returns a new `EagerDataFrame`.

- [ ] `select(columns)` — keep only named columns (or evaluate expressions)
- [ ] `filter(predicate)` — keep rows where boolean expr is true; nulls treated as false
- [ ] `with_column(name, expr)` — add new column or replace existing column by name
- [ ] `group_by(keys)` — groups rows by one or more key columns; returns intermediate grouped object
- [ ] `aggregate(agg_map)` — apply aggregation functions to grouped data (e.g. `{"salary": "sum"}`)
- [ ] `join(other, on, how)` — join two DataFrames; support `"inner"`, `"left"`, `"outer"`
- [ ] `sort(columns, ascending)` — sort rows by one or more columns, asc or desc
- [ ] `head(n)` — return first n rows

---

## Phase 4 — LazyDataFrame & DAG

### DAG Node Types

- [ ] `ScanNode` — leaf node, holds file path and format (CSV or Parquet)
- [ ] `FilterNode` — holds child node + predicate expression
- [ ] `SelectNode` — holds child node + list of column expressions
- [ ] `WithColumnNode` — holds child node + (name, expr) pair
- [ ] `GroupByNode` — holds child node + key column names
- [ ] `AggregateNode` — holds child node + aggregation map
- [ ] `JoinNode` — holds two child nodes + join keys + join type
- [ ] `SortNode` — holds child node + sort columns + direction
- [ ] `LimitNode` — holds child node + row count `n` (for `head`)

### LazyDataFrame Operations

- [ ] Each operation (filter, select, etc.) creates a new DAG node and returns a new `LazyDataFrame` — no data is processed
- [ ] `collect()` — triggers DAG traversal (post-order), executes each node using the Eager operations, returns `EagerDataFrame`
- [ ] `explain(path)` — renders the current DAG to a `.png` file using Graphviz

### DAG Rendering (`explain`)

- [ ] Traverse DAG and emit a `.dot` file with nodes and edges
- [ ] Label each node with its operation type and key parameters
- [ ] Call Graphviz (`dot -Tpng`) to produce the `.png`
- [ ] Show the **optimised** plan (run optimizer before rendering)

---

## Phase 5 — Query Optimizer

The optimizer rewrites the DAG before `collect()` executes it. Each rule is a DAG traversal that pattern-matches and rewrites subtrees.

- [ ] Wire the optimizer into `collect()` so it runs automatically before execution
- [ ] **Predicate Pushdown** — move `FilterNode` closer to `ScanNode`, through `SelectNode` and `WithColumnNode` where the filter only references available columns
- [ ] **Projection Pushdown** — at each `ScanNode`, compute the minimal set of columns required by all downstream operations and read only those columns
- [ ] **Constant Folding** — evaluate expressions involving only `LitExpr` nodes at plan-construction time (e.g. `lit(3) + lit(4)` → `lit(7)`)
- [ ] **Expression Simplification** — simplify trivially redundant expressions:
  - [ ] `x * lit(1)` → `x`
  - [ ] `x + lit(0)` → `x`
  - [ ] `x - lit(0)` → `x`
  - [ ] `x / lit(1)` → `x`
  - [ ] `~~x` (double NOT) → `x`
- [ ] **Limit Pushdown** — push `LimitNode` (from `head(n)`) as far down the DAG as possible, before joins and aggregations where safe

---

## Phase 6 — Memory, Safety & Code Quality

- [ ] Use `std::shared_ptr` for all DAG nodes and Arrow objects — no raw `new`/`delete`
- [ ] Use `std::unique_ptr` where sole ownership is clear
- [ ] Apply move semantics where copying large Arrow tables would be unnecessary
- [ ] Run `valgrind --leak-check=full` on test cases — zero leaks required
- [ ] All public API methods are documented with docstrings (what they do, parameters, return type, exceptions thrown)
- [ ] Throw meaningful exceptions with clear messages for type errors, missing columns, incompatible operations
- [ ] No global mutable state

---

## Phase 7 — Testing

- [ ] Test each I/O function with a sample CSV and Parquet file
- [ ] Test each EagerDataFrame operation in isolation
- [ ] Test expression evaluation: arithmetic, comparison, boolean, string, aggregation
- [ ] Test null handling: operations with null operands produce null results
- [ ] Test type errors throw exceptions (not silently produce wrong results)
- [ ] Test `collect()` on a multi-step lazy chain gives the same result as the equivalent eager chain
- [ ] Test `explain()` produces a `.png` file without crashing
- [ ] Test each optimizer rule: before and after optimisation, result must be identical
- [ ] Test join types: inner, left, outer
- [ ] Test group_by + aggregate with multiple key columns and multiple aggregations
- [ ] Test sort on multiple columns with mixed asc/desc

---

## Phase 8 — Report (`report.pdf`)

- [ ] Architecture overview — class hierarchy, how Eager and Lazy share the expression system
- [ ] DAG design — node types, how `collect()` traverses and executes
- [ ] For **each optimizer rule** implemented:
  - [ ] Description of the transformation
  - [ ] Correctness proof (why result is identical before and after)
  - [ ] Concrete before/after DAG example
  - [ ] Expected performance benefit
- [ ] README with build and installation instructions (`cmake`, `make`, how to link Arrow)

---

## Phase 9 — Submission Checklist

- [ ] Delete `build/` directory and all compiled binaries
- [ ] No generated output files or data files included
- [ ] All source in a directory named `project/`
- [ ] `report.pdf` inside `project/`
- [ ] `README.md` with build instructions inside `project/`
- [ ] Run `tar -cvf <entry_number>.tar project` from the parent directory
- [ ] Upload `.tar` to Moodle
- [ ] Re-download submission from Moodle and verify it unpacks and builds correctly
- [ ] Submit several hours before the deadline

---

## API Quick-Reference (must match exactly)

```cpp
// I/O
EagerDataFrame read_csv(std::string path);
EagerDataFrame read_parquet(std::string path);
LazyDataFrame  scan_csv(std::string path);
LazyDataFrame  scan_parquet(std::string path);
void           write_csv(std::string path);       // method on EagerDataFrame
void           write_parquet(std::string path);   // method on EagerDataFrame
void           sink_csv(std::string path);        // method on LazyDataFrame
void           sink_parquet(std::string path);    // method on LazyDataFrame
EagerDataFrame from_columns(std::map<std::string, arrow::Array> columns);

// DataFrame operations (both Eager and Lazy)
df.select({"col_a", "col_b"});
df.filter(col("x") > lit(0));
df.with_column("z", col("a") + col("b"));
df.group_by({"dept"}).aggregate({{"avg_sal", col("salary").mean()}});
df.join(df2, {"id"}, "inner");
df.sort({"age"}, true);
df.head(10);
df.collect();           // LazyDataFrame only

// Expressions
col("name")
lit(42)
col("x").alias("y")
col("x").abs()
col("x").is_null()
col("x").is_not_null()
col("name").length()
col("email").contains("@")
col("code").starts_with("A")
col("file").ends_with(".txt")
col("city").to_lower()
col("city").to_upper()
col("salary").sum()
col("score").mean()
col("id").count()
col("age").min()
col("age").max()
```
