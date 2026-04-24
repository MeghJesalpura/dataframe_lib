#include "../include/dataframelib/Types.h"
#include "../include/dataframelib/EagerDataFrame.h"
#include <iostream>
#include <arrow/csv/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <arrow/csv/writer.h>
#include "../include/dataframelib/utils/arrayUtils.h"
#include "../include/dataframelib/utils/arrayOps.h"
#include "../include/dataframelib/groupByObj.h"
// read_csv
EagerDataFrame EagerDataFrame::read_csv(const std::string &path)
{
  // opening the file
  auto fileResult = arrow::io::ReadableFile::Open(path);
  if (!fileResult.ok())
    throw std::runtime_error("Could not open file: " + path);
  auto file = fileResult.ValueOrDie();

  // creating CSV reader with default options
  auto readerResult = arrow::csv::TableReader::Make(
      arrow::io::default_io_context(),
      file,
      arrow::csv::ReadOptions::Defaults(),
      arrow::csv::ParseOptions::Defaults(),
      arrow::csv::ConvertOptions::Defaults());
  if (!readerResult.ok())
    throw std::runtime_error("Could not create CSV reader: " +
                             readerResult.status().ToString());

  // reading into Arrow table
  auto tableResult = readerResult.ValueOrDie()->Read();
  if (!tableResult.ok())
    throw std::runtime_error("Could not read CSV: " +
                             tableResult.status().ToString());

  return EagerDataFrame(tableResult.ValueOrDie());
}

// read_parquet
EagerDataFrame EagerDataFrame::read_parquet(const std::string &path)
{
  // opening the file
  auto fileResult = arrow::io::ReadableFile::Open(path);
  if (!fileResult.ok())
    throw std::runtime_error("Could not open file: " + path);

  // create Parquet reader
  auto readerResult = parquet::arrow::OpenFile(fileResult.ValueOrDie(), arrow::default_memory_pool());
  if (!readerResult.ok())
    throw std::runtime_error("Could not create Parquet reader: " + readerResult.status().ToString());

  auto reader = std::move(readerResult.ValueOrDie());
  // reading entire file into Arrow table
  std::shared_ptr<arrow::Table> table;
  auto status = reader->ReadTable(&table);
  if (!status.ok())
    throw std::runtime_error("Could not read Parquet: " +
                             status.ToString());

  return EagerDataFrame(table);
}

// from_columns
EagerDataFrame EagerDataFrame::from_columns(
    const std::map<std::string,
                   std::shared_ptr<arrow::ChunkedArray>> &cols)
{
  std::vector<std::shared_ptr<arrow::Field>> fields;
  std::vector<std::shared_ptr<arrow::ChunkedArray>> arrays;

  for (const auto &[name, arr] : cols)
  {
    fields.push_back(arrow::field(name, arr->type()));
    arrays.push_back(arr);
  }

  auto schema = arrow::schema(fields);
  auto table = arrow::Table::Make(schema, arrays);
  return EagerDataFrame(table);
}

// write_csv
void EagerDataFrame::write_csv(const std::string &path) const
{
  // opening output file
  auto fileResult = arrow::io::FileOutputStream::Open(path);
  if (!fileResult.ok())
    throw std::runtime_error("Could not open file for writing: " + path);
  auto file = fileResult.ValueOrDie();

  // using WriteCSV
  auto writeOptions = arrow::csv::WriteOptions::Defaults();

  // converting table to batches and writing each one
  auto batchResult = table_->CombineChunksToBatch();
  if (!batchResult.ok())
    throw std::runtime_error("Could not combine chunks: " +
                             batchResult.status().ToString());

  auto status = arrow::csv::WriteCSV(
      *batchResult.ValueOrDie(),
      writeOptions,
      file.get());
  if (!status.ok())
    throw std::runtime_error("Could not write CSV: " + status.ToString());
}

// write_parquet
void EagerDataFrame::write_parquet(const std::string &path) const
{
  // Step 1 — open output file
  auto fileResult = arrow::io::FileOutputStream::Open(path);
  if (!fileResult.ok())
    throw std::runtime_error("Could not open file for writing: " + path);

  // Step 2 — write with default properties
  auto status = parquet::arrow::WriteTable(
      *table_,
      arrow::default_memory_pool(),
      fileResult.ValueOrDie(),
      /*chunk_size=*/1024 // rows per row group
  );
  if (!status.ok())
    throw std::runtime_error("Could not write Parquet: " + status.ToString());
}

EagerDataFrame EagerDataFrame::select(const std::vector<std::string> &colNames) const
{
  std::vector<std::shared_ptr<arrow::ChunkedArray>> arrays;
  std::vector<std::shared_ptr<arrow::Field>> fields;

  for (const auto &name : colNames)
  {
    // look up column by name — throws if not found
    auto col = table_->GetColumnByName(name);
    if (!col)
      throw std::runtime_error("Column not found: " + name);

    auto field = table_->schema()->GetFieldByName(name);
    arrays.push_back(col);
    fields.push_back(field);
  }

  auto schema = arrow::schema(fields);
  auto table = arrow::Table::Make(schema, arrays);
  return EagerDataFrame(table);
}

void EagerDataFrame::print() const
{
  auto schema = table_->schema();
  int ncols = schema->num_fields();
  for (int i = 0; i < ncols; i++)
  {
    if (i)
      std::cout << "\t";
    std::cout << schema->field(i)->name();
  }
  std::cout << "\n";
  for (int64_t row = 0; row < table_->num_rows(); row++)
  {
    for (int col = 0; col < ncols; col++)
    {
      if (col)
        std::cout << "\t";
      auto arr = flatten(table_->column(col));
      if (arr->IsNull(row))
      {
        std::cout << "null";
      }
      else
      {
        auto res = arr->GetScalar(row);
        if (res.ok())
          std::cout << res.ValueOrDie()->ToString();
      }
    }
    std::cout << "\n";
  }
}

EagerDataFrame EagerDataFrame::filter(const ExprPtr &predicate) const
{
  // evaluate predicate to get boolean mask
  auto mask = predicate.evaluate(table_);

  // validate that result is boolean
  if (predicate.resultType(table_->schema()) != DataType::BOOLEAN)
    throw std::runtime_error("filter() predicate must return a boolean expression");

  // flatten mask since we need contiguous access
  auto flatMask = std::static_pointer_cast<arrow::BooleanArray>(
      flatten(mask));

  // manually apply mask to each column
  std::vector<std::shared_ptr<arrow::ChunkedArray>> filteredArrays;

  for (int i = 0; i < table_->num_columns(); i++)
  {
    auto col = table_->column(i);
    auto flatCol = flatten(col);
    auto filtered = applyBooleanMask(flatCol, flatMask,
                                     fromArrowType(table_->schema()->field(i)->type()));
    filteredArrays.push_back(filtered);
  }

  // building new table with same schema
  auto newTable = arrow::Table::Make(table_->schema(), filteredArrays);
  return EagerDataFrame(newTable);
}

EagerDataFrame EagerDataFrame::with_column(const std::string &name, const ExprPtr &expr) const
{
  // evaluate expression to get new column data
  auto newCol = expr.evaluate(table_);

  // get result type for new column
  auto newType = expr.resultType(table_->schema());

  // flatten new column for contiguous access
  auto flatNewCol = flatten(newCol);

  // create new field for the column
  auto newField = arrow::field(name, flatNewCol->type());

  std::vector<std::shared_ptr<arrow::ChunkedArray>> arrays;
  std::vector<std::shared_ptr<arrow::Field>> fields;

  for (int i = 0; i < table_->num_columns(); i++)
  {
    if (table_->schema()->field(i)->name() == name)
      continue; // to replace existing column if name already exists
    arrays.push_back(table_->column(i));
    fields.push_back(table_->schema()->field(i));
  }
  arrays.push_back(toChunked(flatNewCol));
  fields.push_back(newField);

  // build new table with added column
  auto newSchema = arrow::schema(fields);
  auto newTable = arrow::Table::Make(newSchema, arrays);
  return EagerDataFrame(newTable);
}

EagerDataFrame EagerDataFrame::head(size_t n) const
{
  std::vector<std::shared_ptr<arrow::ChunkedArray>> arrays;
  for (int i = 0; i < table_->num_columns(); i++)
  {
    auto col = table_->column(i);
    auto headCol = col->Slice(0, std::min(static_cast<int64_t>(n), col->length()));
    arrays.push_back(headCol);
  }
  auto newTable = arrow::Table::Make(table_->schema(), arrays);
  return EagerDataFrame(newTable);
}

EagerDataFrame EagerDataFrame::sort(
    const std::vector<std::string> &colNames,
    bool ascending) const
{
  int64_t n = table_->num_rows();

  // flatten all sort columns upfront
  // store as pair of (flattened array, DataType)
  std::vector<std::pair<std::shared_ptr<arrow::Array>, DataType>> sortCols;
  for (const auto &name : colNames)
  {
    auto col = table_->GetColumnByName(name);
    if (!col)
      throw std::runtime_error("Column not found: " + name);
    auto type = fromArrowType(
        table_->schema()->GetFieldByName(name)->type());
    sortCols.push_back({flatten(col), type});
  }

  // build index array
  std::vector<int64_t> indices(n);
  std::iota(indices.begin(), indices.end(), 0);

  // prefix sort comparator
  // Only looks at key i+1 if key i was a tie
  auto comparator = [&](int64_t rowA, int64_t rowB) -> bool
  {
    for (const auto &[arr, type] : sortCols)
    {
      auto valA = extractRowValue(arr, rowA, type);
      auto valB = extractRowValue(arr, rowB, type);

      int cmp = compareRowValues(valA, valB);

      if (cmp != 0)
      {
        // not a tie — decide here, don't look at further keys
        return ascending ? cmp < 0 : cmp > 0;
      }
      // tie — fall through to next key
    }
    // all keys tied — maintain original order (stable)
    return rowA < rowB;
  };

  // stable sort preserves original order for full ties
  std::stable_sort(indices.begin(), indices.end(), comparator);

  // reorder all columns according to sorted indices
  std::vector<std::shared_ptr<arrow::ChunkedArray>> sortedArrays;
  for (int col = 0; col < table_->num_columns(); col++)
  {
    auto colType = fromArrowType(table_->schema()->field(col)->type());
    auto flatCol = flatten(table_->column(col));
    sortedArrays.push_back(
        reorderByIndices(flatCol, indices, colType));
  }

  auto newTable = arrow::Table::Make(table_->schema(), sortedArrays);
  return EagerDataFrame(newTable);
}

EagerDataFrame EagerDataFrame::join(const EagerDataFrame &other,
                                    const std::vector<std::string> &onColumns, const std::string &how) const
{
  // joins to be implemented: inner, left, outer
  // first write a simple inner join, then extend to other types
  //  For inner join, we can do a hash-based approach:
  //  1. Build a hash map from the smaller table (other) using the join keys
  //  2. Iterate through the larger table (this), probe the hash map for matches, and build the result rows
  //  Implementing it

  std::map<std::string, std::shared_ptr<arrow::ChunkedArray>> resultColumns;
  if (table_->num_rows() > other.table()->num_rows())
  {
    // swap in this case
    return other.join(*this, onColumns, how);
  }

  std::vector<std::shared_ptr<arrow::Array>> flatCols;
  std::vector<DataType> colTypes;
  for (const auto &colName : onColumns)
  {
    auto col = table_->GetColumnByName(colName);
    if (!col)
      throw std::runtime_error("Join column not found: " + colName);
    flatCols.push_back(flatten(col));
    colTypes.push_back(fromArrowType(
        table_->schema()->GetFieldByName(colName)->type()));
  }
  // Build a single string key per row — much faster than vector<string>
  auto buildKey = [&](
                      const std::vector<std::shared_ptr<arrow::Array>> &cols,
                      const std::vector<DataType> &types,
                      int64_t row) -> std::string
  {
    std::string key;
    for (size_t i = 0; i < cols.size(); i++)
    {
      if (cols[i]->IsNull(row))
      {
        key += "__null__";
      }
      else
      {
        auto val = extractRowValue(cols[i], row, types[i]);
        std::visit([&key](const auto &v)
                   {
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>)
                    key += v;
                else
                    key += std::to_string(v); }, val);
      }
      key += '\0'; // null byte delimiter — safe since values won't contain it
    }
    return key;
  };
  // O(n) build, O(1) average lookup
  std::unordered_map<std::string, std::vector<int64_t>> hashMap;
  hashMap.reserve(table_->num_rows()); // pre-allocate

  for (int64_t i = 0; i < table_->num_rows(); i++)
  {
    hashMap[buildKey(flatCols, colTypes, i)].push_back(i);
  }
  // Pre-flatten other table's join columns too
  std::vector<std::shared_ptr<arrow::Array>> otherFlatCols;
  std::vector<DataType> otherColTypes;
  for (const auto &colName : onColumns)
  {
    auto col = other.table()->GetColumnByName(colName);
    otherFlatCols.push_back(flatten(col));
    otherColTypes.push_back(fromArrowType(
        other.table()->schema()->GetFieldByName(colName)->type()));
  }

  std::vector<int64_t> leftIndices, rightIndices;
  for (int64_t j = 0; j < other.table()->num_rows(); j++)
  {
    auto key = buildKey(otherFlatCols, otherColTypes, j);
    auto it = hashMap.find(key); // O(1) average
    if (it != hashMap.end())
    {
      for (int64_t i : it->second)
      {
        leftIndices.push_back(i);  // matched row from smaller table
        rightIndices.push_back(j); // matched row from larger table
      }
    }
  }

  // now got to build it
  if (how == "left" || how == "outer")
  {
    // For left join, we keep all rows from the left table, and add nulls for non-matching rows
    std::vector<int64_t> unmatchedLeftIndices;
    std::unordered_set<int64_t> matchedSet(leftIndices.begin(), leftIndices.end());
    for (int64_t i = 0; i < table_->num_rows(); i++)
    {
      if (matchedSet.find(i) == matchedSet.end())
      {
        unmatchedLeftIndices.push_back(i);
      }
    }
    for (int64_t idx : unmatchedLeftIndices)
    {
      leftIndices.push_back(idx);
      rightIndices.push_back(-1); // -1 indicates no match
    }
  }
  if (how == "outer")
  {
    // For outer join, we also keep unmatched rows from the right table
    std::vector<int64_t> unmatchedRightIndices;
    std::unordered_set<int64_t> matchedRightSet(rightIndices.begin(), rightIndices.end());
    for (int64_t j = 0; j < other.table()->num_rows(); j++)
    {
      if (matchedRightSet.find(j) == matchedRightSet.end())
      {
        unmatchedRightIndices.push_back(j);
      }
    }
    for (int64_t idx : unmatchedRightIndices)
    {
      leftIndices.push_back(-1); // -1 indicates no match
      rightIndices.push_back(idx);
    }
  }

  std::vector<std::shared_ptr<arrow::ChunkedArray>> leftArrays, rightArrays;
  for (int col = 0; col < table_->num_columns(); col++)
  {
    auto colType = fromArrowType(table_->schema()->field(col)->type());
    auto flatCol = flatten(table_->column(col));
    leftArrays.push_back(reorderByIndices(flatCol, leftIndices, colType));
  }
  for (int col = 0; col < other.table()->num_columns(); col++)
  {
    auto colType = fromArrowType(other.table()->schema()->field(col)->type());
    auto flatCol = flatten(other.table()->column(col));
    rightArrays.push_back(reorderByIndices(flatCol, rightIndices, colType));
  }
  std::vector<std::shared_ptr<arrow::ChunkedArray>> allArrays;
  allArrays.reserve(leftArrays.size() + rightArrays.size());
  allArrays.insert(allArrays.end(), leftArrays.begin(), leftArrays.end());
  allArrays.insert(allArrays.end(), rightArrays.begin(), rightArrays.end());

  std::vector<std::shared_ptr<arrow::Field>> allFields;
  for (int i = 0; i < table_->num_columns(); i++)
  {
    allFields.push_back(table_->schema()->field(i));
  }
  for (int i = 0; i < other.table()->num_columns(); i++)
  {
    auto fieldName = other.table()->schema()->field(i)->name();
    if (onColumns.end() != std::find(onColumns.begin(), onColumns.end(), fieldName))
      continue; // skip join keys from right table to avoid duplicates
    if (table_->schema()->GetFieldByName(fieldName))
      fieldName += "_right"; // if same name replacing with suffix
    allFields.push_back(arrow::field(fieldName, other.table()->schema()->field(i)->type()));
  }

  auto newSchema = arrow::schema(allFields);
  auto newTable = arrow::Table::Make(newSchema, allArrays);
  return EagerDataFrame(newTable);
}

GroupByObj EagerDataFrame::group_by(const std::vector<std::string> &colNames) const
{
  std::map<std::vector<std::string>, std::map<std::string, GroupAccumulator>> groupMap;

  for (const auto &k : colNames)
    if (!table_->GetColumnByName(k))
      return GroupByObj(colNames, {}, {});

  std::vector<std::string> aggCols;
  std::map<std::string, DataType> colTypes;
  for (int i = 0; i < table_->num_columns(); i++)
  {
    const std::string &name = table_->schema()->field(i)->name();
    if (std::find(colNames.begin(), colNames.end(), name) == colNames.end())
    {
      aggCols.push_back(name);
      auto type = fromArrowType(table_->schema()->field(i)->type());
      if (isNumeric(type))
        colTypes[name] = type;
    }
  }
  for (int64_t row = 0; row < table_->num_rows(); row++)
  {
    std::vector<std::string> key;
    for (const auto &k : colNames)
    {
      auto arr = flatten(table_->GetColumnByName(k));
      if (arr->IsNull(row))
        key.push_back("null");
      else
      {
        auto res = arr->GetScalar(row);
        key.push_back(res.ok() ? res.ValueOrDie()->ToString() : "null");
      }
    }
    groupMap.emplace(key, std::map<std::string, GroupAccumulator>{});
    for (const auto &colName : aggCols)
    {
      auto arr = flatten(table_->GetColumnByName(colName));
      if (arr->IsNull(row))
        continue;
      auto type = fromArrowType(table_->schema()->GetFieldByName(colName)->type());
      if (!isNumeric(type))
        continue;
      groupMap[key][colName].update(extractAsDouble(arr, row, type));
    }
  }
  return GroupByObj(colNames, groupMap, colTypes);
}

static std::shared_ptr<arrow::Array> buildAggArray(
    const std::vector<double> &vals, DataType type, const std::string &op)
{
  std::shared_ptr<arrow::Array> arr;
  if (op == "count")
  {
    arrow::Int64Builder b;
    for (double v : vals) b.Append(static_cast<int64_t>(v));
    b.Finish(&arr);
    return arr;
  }
  switch (type)
  {
  case DataType::INT32: {
    arrow::Int32Builder b;
    for (double v : vals) b.Append(static_cast<int32_t>(v));
    b.Finish(&arr); return arr;
  }
  case DataType::INT64: {
    arrow::Int64Builder b;
    for (double v : vals) b.Append(static_cast<int64_t>(v));
    b.Finish(&arr); return arr;
  }
  case DataType::FLOAT32: {
    arrow::FloatBuilder b;
    for (double v : vals) b.Append(static_cast<float>(v));
    b.Finish(&arr); return arr;
  }
  default: {
    arrow::DoubleBuilder b;
    for (double v : vals) b.Append(v);
    b.Finish(&arr); return arr;
  }
  }
}

static std::shared_ptr<arrow::DataType> aggOutputArrowType(DataType type, const std::string &op)
{
  if (op == "count") return arrow::int64();
  switch (type)
  {
  case DataType::INT32:   return arrow::int32();
  case DataType::INT64:   return arrow::int64();
  case DataType::FLOAT32: return arrow::float32();
  default:                return arrow::float64();
  }
}

EagerDataFrame GroupByObj::aggregate(const std::map<std::string, std::string> &aggMap) const
{
  std::vector<std::shared_ptr<arrow::Field>> fields;
  std::vector<std::shared_ptr<arrow::ChunkedArray>> arrays;

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

  for (const auto &[colName, op] : aggMap)
  {
    DataType colType = DataType::FLOAT64;
    if (auto it = colTypes_.find(colName); it != colTypes_.end())
      colType = it->second;

    std::vector<double> results;
    for (const auto &[key, colAccs] : groups_)
    {
      auto it = colAccs.find(colName);
      GroupAccumulator defaultAcc;
      const auto &acc = (it != colAccs.end()) ? it->second : defaultAcc;
      double result = 0.0;
      if (op == "sum")        result = acc.sum;
      else if (op == "count") result = static_cast<double>(acc.count);
      else if (op == "min")   result = acc.min;
      else if (op == "max")   result = acc.max;
      else if (op == "mean")  result = acc.count > 0 ? acc.sum / acc.count : 0.0;
      else throw std::runtime_error("Unsupported aggregation operation: " + op);
      results.push_back(result);
    }
    auto arr = buildAggArray(results, colType, op);
    fields.push_back(arrow::field(colName, aggOutputArrowType(colType, op)));
    arrays.push_back(toChunked(arr));
  }

  auto schema = arrow::schema(fields);
  auto table = arrow::Table::Make(schema, arrays);
  return EagerDataFrame(table);
}