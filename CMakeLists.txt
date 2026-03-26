cmake_minimum_required(VERSION 3.25)

project(COP290_A4)

find_package(Arrow REQUIRED)
find_package(ArrowDataset REQUIRED)
find_package(Parquet REQUIRED)

add_executable(my_example my_example.cc)
target_link_libraries(my_example PRIVATE 
Arrow::arrow_shared
ArrowDataset::arrow_dataset_shared
Parquet::parquet_shared)