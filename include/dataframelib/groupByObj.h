#ifndef GROUP_BY_OBJ_H
#define GROUP_BY_OBJ_H

#include "Types.h"
#include "EagerDataFrame.h"
#include "utils/arrayUtils.h"
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
             const std::map<std::vector<std::string>, std::map<std::string, GroupAccumulator>> &groups)
      : groupKeys_(groupKeys), groups_(groups) {}

  EagerDataFrame agg(const std::map<std::string, AggOp> &aggMap) const
  {
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::ChunkedArray>> arrays;

    // Key columns — emit as string (keys were stringified during group_by)
    for (size_t ki = 0; ki < groupKeys_.size(); ki++)
    {
      arrow::StringBuilder builder;
      for (const auto &[key, _] : groups_)
        builder.Append(key[ki]);
      std::shared_ptr<arrow::Array> arr;
      builder.Finish(&arr);
      fields.push_back(arrow::field(groupKeys_[ki], arrow::utf8()));
      arrays.push_back(toChunked(arr));
    }

    // Aggregated columns — always emitted as double
    for (const auto &[colName, op] : aggMap)
    {
      arrow::DoubleBuilder builder;
      for (const auto &[key, colAccs] : groups_)
      {
        auto it = colAccs.find(colName);
        GroupAccumulator defaultAcc;
        const auto &acc = (it != colAccs.end()) ? it->second : defaultAcc;
        double result = 0.0;
        switch (op)
        {
        case AggOp::SUM:
          result = acc.sum;
          break;
        case AggOp::MEAN:
          result = acc.count > 0 ? acc.sum / acc.count : 0.0;
          break;
        case AggOp::MIN:
          result = acc.min;
          break;
        case AggOp::MAX:
          result = acc.max;
          break;
        case AggOp::COUNT:
          result = static_cast<double>(acc.count);
          break;
        }
        builder.Append(result);
      }
      std::shared_ptr<arrow::Array> arr;
      builder.Finish(&arr);
      fields.push_back(arrow::field(colName, arrow::float64()));
      arrays.push_back(toChunked(arr));
    }

    auto schema = arrow::schema(fields);
    auto table = arrow::Table::Make(schema, arrays);
    return EagerDataFrame(table);
  }

private:
  std::vector<std::string> groupKeys_;
  std::map<std::vector<std::string>, std::map<std::string, GroupAccumulator>> groups_;
};

// Defined here (not in EagerDataFrame.cpp) to avoid circular include
// groupByObj.h needs EagerDataFrame complete; EagerDataFrame.h needs GroupByObj complete.
inline GroupByObj EagerDataFrame::group_by(const std::vector<std::string> &colNames) const
{
  std::map<std::vector<std::string>, std::map<std::string, GroupAccumulator>> groupMap;

  // collect non-key columns for aggregation
  std::vector<std::string> aggCols;
  for (int i = 0; i < table_->num_columns(); i++)
  {
    const std::string &name = table_->schema()->field(i)->name();
    if (std::find(colNames.begin(), colNames.end(), name) == colNames.end())
      aggCols.push_back(name);
  }

  for (int64_t row = 0; row < table_->num_rows(); row++)
  {
    // stringify each group-key column value for this row
    std::vector<std::string> key;
    for (const auto &k : colNames)
    {
      auto arr = flatten(table_->GetColumnByName(k));
      if (arr->IsNull(row))
      {
        key.push_back("null");
      }
      else
      {
        auto res = arr->GetScalar(row);
        key.push_back(res.ok() ? res.ValueOrDie()->ToString() : "null");
      }
    }

    // ensure this group key exists even if all agg columns are null
    groupMap.emplace(key, std::map<std::string, GroupAccumulator>{});

    // update accumulator for each non-null numeric agg column
    for (const auto &colName : aggCols)
    {
      auto arr = flatten(table_->GetColumnByName(colName));
      if (arr->IsNull(row))
        continue;
      auto type = fromArrowType(table_->schema()->GetFieldByName(colName)->type());
      if (!isNumeric(type))
        continue;
      double val = extractAsDouble(arr, row, type);
      groupMap[key][colName].update(val);
    }
  }

  return GroupByObj(colNames, groupMap);
}

#endif
