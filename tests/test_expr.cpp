#include <iostream>
#include "../include/dataframelib/EagerDataFrame.h"
#include <arrow/api.h>
#include "../include/dataframelib/Types.h"
#include "../include/dataframelib/expression/expr.h"
#include "../include/dataframelib/expression/colExpr.h"
#include "../include/dataframelib/operations/relOpExpr.h"
#include "../include/dataframelib/operations/binOpExpr.h"
#include "../include/dataframelib/operations/unaryOpExpr.h"
#include <map>
int main()
{
  // Build test table manually
  arrow::Int32Builder ageBuilder;
  ageBuilder.AppendValues({25, 25, 28, 42});
  std::shared_ptr<arrow::Array> ageArr;
  ageBuilder.Finish(&ageArr);

  arrow::StringBuilder nameBuilder;
  nameBuilder.AppendValues({"Alice", "Bob", "Carol", "Dave"});
  std::shared_ptr<arrow::Array> nameArr;
  nameBuilder.Finish(&nameArr);

  arrow::DoubleBuilder salaryBuilder;
  salaryBuilder.AppendValues({50000.0, 70000.0, 60000.0, 90000.0});
  std::shared_ptr<arrow::Array> salaryArr;
  salaryBuilder.Finish(&salaryArr);

  std::map<std::string, std::shared_ptr<arrow::ChunkedArray>> columns = {
      {"age", std::make_shared<arrow::ChunkedArray>(ageArr)},
      {"name", std::make_shared<arrow::ChunkedArray>(nameArr)},
      {"salary", std::make_shared<arrow::ChunkedArray>(salaryArr)}};
  auto df = EagerDataFrame::from_columns(columns);

  // Test 1 — basic select
  std::cout << "=== select name, age ===\n";
  df.select({"name", "age"}).print();
  // expected: name | age columns only

  // Test 2 — single column
  std::cout << "=== select name only ===\n";
  df.select({"name"}).print();

  // Test 3 — column not found
  std::cout << "=== select invalid column ===\n";
  try
  {
    df.select({"nonexistent"});
    std::cout << "ERROR: should have thrown\n";
  }
  catch (const std::exception &e)
  {
    std::cout << "Correctly threw: " << e.what() << "\n";
  }

  // Test 4 — chained with filter
  std::cout << "=== filter then select ===\n";
  df.filter(col("age") > 30)
      .select({"name", "salary"})
      .print();
  // expected: Bob 70000, Dave 90000

  // Test 5 — with_column
  df.with_column("age_plus_10", col("age") + 10)
      .select({"name", "age", "age_plus_10"})
      .print();
  // expected: name | age | age_plus_10 with correct values

  // Test 6 - with group_by and aggregate
  df.group_by({"age"})
      .agg({{"salary", AggOp::MEAN}})
      .print();
  // expected: age | salary_mean with correct values
}