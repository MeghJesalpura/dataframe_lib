cmake_minimum_required(VERSION 3.25)

project(COP290_A4)

find_package(Arrow REQUIRED)
find_package(ArrowDataset REQUIRED)
find_package(Parquet REQUIRED)

set(CMAKE_CXX_CLANG_TIDY clang-tidy;-checks=modernize-*,performance-*,readability-*)

add_library(dataframelib
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
    include/dataframelib/utils/graphRender.h)

add_executable(test_expr tests/test_expr.cpp)

target_link_libraries(test_expr PRIVATE dataframelib)

target_include_directories(dataframelib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries( dataframelib PUBLIC
Arrow::arrow_shared
ArrowDataset::arrow_dataset_shared
Parquet::parquet_shared)

