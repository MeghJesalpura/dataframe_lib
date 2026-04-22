#ifndef OPS_HELPER_H
#define OPS_HELPER_H

#include "../Types.h"
#include "typeDispatch.h"
template <DataType T>
typename TypeTraits<T>::CppType computeSum(const std::shared_ptr<arrow::ChunkedArray> &arr);

template <DataType T>
typename TypeTraits<T>::CppType computeMean(const std::shared_ptr<arrow::ChunkedArray> &arr);

template <DataType T>
typename TypeTraits<T>::CppType computeMin(const std::shared_ptr<arrow::ChunkedArray> &arr);

template <DataType T>
typename TypeTraits<T>::CppType computeMax(const std::shared_ptr<arrow::ChunkedArray> &arr);

int64_t computeCount(const std::shared_ptr<arrow::ChunkedArray> &arr);

#endif