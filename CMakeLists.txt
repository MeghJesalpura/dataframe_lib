cmake_minimum_required(VERSION 3.25)

project(COP290_A4)

find_package(Arrow REQUIRED)
find_package(ArrowDataset REQUIRED)
find_package(Parquet REQUIRED)

add_executable(test_expr tests/test_expr.cpp
    include/dataframelib/Types.h
    include/dataframelib/expression/expr.h
    include/dataframelib/expression/colExpr.h
    include/dataframelib/expression/litExpr.h
    include/dataframelib/expression/aliasExpr.h
    include/dataframelib/operations/binOpExpr.h
    include/dataframelib/operations/unaryOpExpr.h
    include/dataframelib/operations/relOpExpr.h
    include/dataframelib/operations/boolOpExpr.h
    include/dataframelib/operations/strBinOpExpr.h
    include/dataframelib/operations/strUnOpExpr.h
    include/dataframelib/operations/aggOpExpr.h
    src/expression/expr.cpp
    include/dataframelib/expression/exprPtr.h
    src/expression/exprPtr.cpp
    include/dataframelib/utils/arrayUtils.h
    include/dataframelib/utils/typeDispatch.h
    src/utils/arrayOps.cpp
    src/utils/arrayUtils.cpp
    src/utils/opsHelper.cpp
    include/dataframelib/utils/opsHelper.h
    src/EagerDataFrame.cpp
    include/dataframelib/lazy/nodes.h
    include/dataframelib/LazyDataFrame.h
    src/LazyDataFrame.cpp
    include/dataframelib/groupByObj.h
    include/dataframelib/QueryExecutor.h
    src/QueryExecutor.cpp
    data.parquet
    include/dataframelib/utils/graphRender.h
    include/dataframelib/utils/overloaded.h)
target_link_libraries(test_expr PRIVATE 
Arrow::arrow_shared
ArrowDataset::arrow_dataset_shared
Parquet::parquet_shared)

target_include_directories(test_expr PRIVATE include/)
set(CMAKE_CXX_CLANG_TIDY clang-tidy;-checks=modernize-*,performance-*,readability-*)