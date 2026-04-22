// ArrayUtils.cpp
#include "../../include/dataframelib/utils/arrayUtils.h"

std::shared_ptr<arrow::Array> flatten(
    const std::shared_ptr<arrow::ChunkedArray> &chunked)
{

  // Fast path — already one chunk, no copy needed
  if (chunked->num_chunks() == 1)
    return chunked->chunk(0);

  // Slow path — concatenate all chunks into one
  // arrow::Concatenate is the only Arrow function used here
  // it's a memory operation, not a compute operation
  std::shared_ptr<arrow::Array> result;
  arrow::Concatenate(chunked->chunks(),
                     arrow::default_memory_pool())
      .Value(&result);
  return result;
}

std::shared_ptr<arrow::ChunkedArray> toChunked(
    const std::shared_ptr<arrow::Array> &arr)
{
  return std::make_shared<arrow::ChunkedArray>(arr);
}

// castArray — manually copies values into a new typed builder
std::shared_ptr<arrow::ChunkedArray> castArray(
    const std::shared_ptr<arrow::ChunkedArray> &arr,
    DataType from, DataType to)
{

  if (from == to)
    return arr; // no-op

  auto flat = flatten(arr);
  if (from == DataType::INT32 && to == DataType::INT64)
  {
    auto src = std::static_pointer_cast<arrow::Int32Array>(flat);
    arrow::Int64Builder builder;
    for (int64_t i = 0; i < src->length(); i++)
    {
      if (src->IsNull(i))
        builder.AppendNull();
      else
        builder.Append(static_cast<int64_t>(src->Value(i)));
    }
    std::shared_ptr<arrow::Array> result;
    builder.Finish(&result);
    return toChunked(result);
  }
  if (from == DataType::INT32 && to == DataType::FLOAT32)
  {
    auto src = std::static_pointer_cast<arrow::Int32Array>(flat);
    arrow::FloatBuilder builder;
    for (int64_t i = 0; i < src->length(); i++)
    {
      if (src->IsNull(i))
        builder.AppendNull();
      else
        builder.Append(static_cast<float>(src->Value(i)));
    }
    std::shared_ptr<arrow::Array> result;
    builder.Finish(&result);
    return toChunked(result);
  }
  if (from == DataType::INT32 && to == DataType::FLOAT64)
  {
    auto src = std::static_pointer_cast<arrow::Int32Array>(flat);
    arrow::DoubleBuilder builder;
    for (int64_t i = 0; i < src->length(); i++)
    {
      if (src->IsNull(i))
        builder.AppendNull();
      else
        builder.Append(static_cast<double>(src->Value(i)));
    }
    std::shared_ptr<arrow::Array> result;
    builder.Finish(&result);
    return toChunked(result);
  }
  if (from == DataType::INT64 && to == DataType::FLOAT64)
  {
    auto src = std::static_pointer_cast<arrow::Int64Array>(flat);
    arrow::DoubleBuilder builder;
    for (int64_t i = 0; i < src->length(); i++)
    {
      if (src->IsNull(i))
        builder.AppendNull();
      else
        builder.Append(static_cast<double>(src->Value(i)));
    }
    std::shared_ptr<arrow::Array> result;
    builder.Finish(&result);
    return toChunked(result);
  }
  if (from == DataType::INT64 && to == DataType::FLOAT32)
  {
    auto src = std::static_pointer_cast<arrow::Int64Array>(flat);
    arrow::FloatBuilder builder;
    for (int64_t i = 0; i < src->length(); i++)
    {
      if (src->IsNull(i))
        builder.AppendNull();
      else
        builder.Append(static_cast<float>(src->Value(i)));
    }
    std::shared_ptr<arrow::Array> result;
    builder.Finish(&result);
    return toChunked(result);
  }
  if (from == DataType::FLOAT32 && to == DataType::FLOAT64)
  {
    auto src = std::static_pointer_cast<arrow::FloatArray>(flat);
    arrow::DoubleBuilder builder;
    for (int64_t i = 0; i < src->length(); i++)
    {
      if (src->IsNull(i))
        builder.AppendNull();
      else
        builder.Append(static_cast<double>(src->Value(i)));
    }
    std::shared_ptr<arrow::Array> result;
    builder.Finish(&result);
    return toChunked(result);
  }
  throw std::runtime_error("Unsupported cast");
}