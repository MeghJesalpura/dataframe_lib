#ifndef OPS_HELPER_H
#define OPS_HELPER_H

#include "../Types.h"
#include "typeDispatch.h"
#include <optional>
#include <stdexcept>

namespace dataframelib
{
  template <DataType T, typename AccFn>
  typename TypeTraits<T>::CppType iterateNonNull(
      const std::shared_ptr<arrow::ChunkedArray> &arr,
      typename TypeTraits<T>::CppType init,
      AccFn &&fn)
  {
    using ArrType = typename TypeTraits<T>::ArrayType;
    using CppType = typename TypeTraits<T>::CppType;
    CppType acc = init;
    for (const auto &chunk : arr->chunks())
    {
      auto typed = std::static_pointer_cast<ArrType>(chunk);
      for (int64_t i = 0; i < typed->length(); i++)
        if (!typed->IsNull(i))
          acc = fn(acc, typed->Value(i));
    }
    return acc;
  }

  template <DataType T>
  typename TypeTraits<T>::CppType computeSum(const std::shared_ptr<arrow::ChunkedArray> &arr)
  {
    using CppType = typename TypeTraits<T>::CppType;
    return iterateNonNull<T>(arr, CppType{0}, [](CppType a, CppType b)
                             { return a + b; });
  }

  template <DataType T>
  typename TypeTraits<T>::CppType computeMean(const std::shared_ptr<arrow::ChunkedArray> &arr)
  {
    using ArrType = typename TypeTraits<T>::ArrayType;
    using CppType = typename TypeTraits<T>::CppType;
    double sum = 0.0;
    int64_t count = 0;
    for (const auto &chunk : arr->chunks())
    {
      auto typed = std::static_pointer_cast<ArrType>(chunk);
      for (int64_t i = 0; i < typed->length(); i++)
        if (!typed->IsNull(i))
        {
          sum += static_cast<double>(typed->Value(i));
          count++;
        }
    }
    if (count == 0)
      throw std::runtime_error("mean() called on all-null column");
    return static_cast<CppType>(sum / count);
  }

  template <DataType T>
  typename TypeTraits<T>::CppType computeMin(const std::shared_ptr<arrow::ChunkedArray> &arr)
  {
    using CppType = typename TypeTraits<T>::CppType;
    using ArrType = typename TypeTraits<T>::ArrayType;
    std::optional<CppType> result;
    for (const auto &chunk : arr->chunks())
    {
      auto typed = std::static_pointer_cast<ArrType>(chunk);
      for (int64_t i = 0; i < typed->length(); i++)
        if (!typed->IsNull(i))
        {
          CppType v = typed->Value(i);
          if (!result || v < *result)
            result = v;
        }
    }
    if (!result)
      throw std::runtime_error("min() called on all-null column");
    return *result;
  }

  template <DataType T>
  typename TypeTraits<T>::CppType computeMax(const std::shared_ptr<arrow::ChunkedArray> &arr)
  {
    using CppType = typename TypeTraits<T>::CppType;
    using ArrType = typename TypeTraits<T>::ArrayType;
    std::optional<CppType> result;
    for (const auto &chunk : arr->chunks())
    {
      auto typed = std::static_pointer_cast<ArrType>(chunk);
      for (int64_t i = 0; i < typed->length(); i++)
        if (!typed->IsNull(i))
        {
          CppType v = typed->Value(i);
          if (!result || v > *result)
            result = v;
        }
    }
    if (!result)
      throw std::runtime_error("max() called on all-null column");
    return *result;
  }

  inline int64_t computeCount(const std::shared_ptr<arrow::ChunkedArray> &arr)
  {
    int64_t count = 0;
    for (const auto &chunk : arr->chunks())
      count += chunk->length() - chunk->null_count();
    return count;
  }
} // namespace dataframelib
#endif
