#include <iostream>
#include "../include/dataframelib/EagerDataFrame.h"
#include <arrow/api.h>
#include "../include/dataframelib/Types.h"
#include "../include/dataframelib/expression/expr.h"
#include "../include/dataframelib/expression/colExpr.h"
#include "../include/dataframelib/operations/relOpExpr.h"
#include "../include/dataframelib/operations/binOpExpr.h"
#include "../include/dataframelib/operations/unaryOpExpr.h"
#include "../include/dataframelib/groupByObj.h"
#include "../include/dataframelib/LazyDataFrame.h"
#include <map>

using namespace dataframelib;
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

    arrow::Int32Builder salaryBuilder;
    salaryBuilder.AppendValues({50000, 70001, 60000, 90000});
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
        .aggregate({{"salary", "mean"}})
        .print();
    // expected: age | salary_mean (column named salary_mean) with correct values

    // Test 7 - head
    std::cout << "=== head(2) ===\n";
    df.head(2).print();
    // expected: first 2 rows of the original table

    // Test 8 - sort
    std::cout << "=== sort by age desc, then salary asc ===\n";
    df.sort({"age"}, /*ascending=*/false).print();
    df.sort({"salary"}, /*ascending=*/true).print();

    // Test 9 - join
    std::cout << "=== join with another table ===\n";
    arrow::Int32Builder ageBuilder2;
    ageBuilder2.AppendValues({25, 28, 35});
    std::shared_ptr<arrow::Array> ageArr2;
    ageBuilder2.Finish(&ageArr2);
    arrow::StringBuilder deptBuilder;
    deptBuilder.AppendValues({"HR", "Engineering", "Sales"});
    std::shared_ptr<arrow::Array> deptArr;
    deptBuilder.Finish(&deptArr);
    std::map<std::string, std::shared_ptr<arrow::ChunkedArray>> columns2 = {
        {"age", std::make_shared<arrow::ChunkedArray>(ageArr2)},
        {"department", std::make_shared<arrow::ChunkedArray>(deptArr)}};
    auto df2 = EagerDataFrame::from_columns(columns2);
    df.join(df2, {"age"}, "left").print();

    std::cout << "=== lazy execution test ===\n";
    auto df3 = scan_parquet("data.parquet");
    std::cout << "=== lazy execution test - after scan_parquet ===\n";
    auto result = df3.filter(col("age") > 30)
                      .select({"name", "salary"})
                      .group_by({"dept"})
                      .aggregate({{"salary", "mean"}});

    std::cout << "=== lazy execution test - result of collect() ===\n";
    result.explain("plan.png"); // dumps the DAG to a file
    auto collected = result.collect();
    std::cout << "=== lazy execution test - after collect() ===\n";
    collected.print();
}