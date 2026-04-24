#ifndef GROUP_BY_OBJ_H
#define GROUP_BY_OBJ_H

#include "Types.h"
class EagerDataFrame;
#include <limits>
#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct GroupAccumulator
{
  double sum = 0.0;
  double min = std::numeric_limits<double>::max();
  double max = std::numeric_limits<double>::lowest();
  int64_t count = 0;

  void update(double val)
  {
    sum += val;
    count++;
    if (val < min)
      min = val;
    if (val > max)
      max = val;
  }
};

// Extract a row's value as double from a flat Arrow array
static inline double extractAsDouble(const std::shared_ptr<arrow::Array> &arr, int64_t row, DataType type)
{
  switch (type)
  {
  case DataType::INT32:
    return static_cast<double>(std::static_pointer_cast<arrow::Int32Array>(arr)->Value(row));
  case DataType::INT64:
    return static_cast<double>(std::static_pointer_cast<arrow::Int64Array>(arr)->Value(row));
  case DataType::FLOAT32:
    return static_cast<double>(std::static_pointer_cast<arrow::FloatArray>(arr)->Value(row));
  case DataType::FLOAT64:
    return std::static_pointer_cast<arrow::DoubleArray>(arr)->Value(row);
  default:
    throw std::runtime_error("extractAsDouble: unsupported type");
  }
}

class GroupByObj
{
public:
  GroupByObj(const std::vector<std::string> &groupKeys,
             const std::map<std::vector<std::string>, std::map<std::string, GroupAccumulator>> &groups,
             const std::map<std::string, DataType> &colTypes)
      : groupKeys_(groupKeys), groups_(groups), colTypes_(colTypes) {}

  EagerDataFrame aggregate(const std::map<std::string, std::string> &aggMap) const;

private:
  std::vector<std::string> groupKeys_;
  std::map<std::vector<std::string>, std::map<std::string, GroupAccumulator>> groups_;
  std::map<std::string, DataType> colTypes_;
};

#endif
