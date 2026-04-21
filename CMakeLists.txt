cmake_minimum_required(VERSION 3.25)

project(COP290_A4)

find_package(Arrow REQUIRED)
find_package(ArrowDataset REQUIRED)
find_package(Parquet REQUIRED)

add_executable(my_example my_example.cc
    include/dataframelib/Types.h
    include/dataframelib/expression/expr.h
    include/dataframelib/expression/colExpr.h
    include/dataframelib/expression/litExpr.h
    include/dataframelib/expression/aliasExpr.h
    include/dataframelib/arithmetic/binOpExpr.h
    include/dataframelib/arithmetic/unaryOpExpr.h
    include/dataframelib/expression/expr.cpp
    include/dataframelib/expression/exprPtr.h
    include/dataframelib/expression/exprPtr.cpp
    include/dataframelib/utils/arrayUtils.h
    include/dataframelib/utils/typeDispatch.h
    src/utils/arrayOps.cpp
    src/utils/typeDispatch.cpp)
target_link_libraries(my_example PRIVATE 
Arrow::arrow_shared
ArrowDataset::arrow_dataset_shared
Parquet::parquet_shared)

target_include_directories(my_example PRIVATE include/)
set(CMAKE_CXX_CLANG_TIDY clang-tidy;-checks=modernize-*,performance-*,readability-*)