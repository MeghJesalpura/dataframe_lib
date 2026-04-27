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

#define DF_ARROW_THROW_NOT_OK(expr)                  \
  do                                                 \
  {                                                  \
    const auto _status_ = (expr);                    \
    if (!_status_.ok())                              \
      throw std::runtime_error(_status_.ToString()); \
  } while (0)
// read_csv
namespace dataframelib
{
  // To read the csv given a file path
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

  // to read a parquet file given a file path
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
    auto tableResult = reader->ReadTable();
    if (!tableResult.ok())
      throw std::runtime_error("Could not read Parquet: " +
                               tableResult.status().ToString());

    return EagerDataFrame(tableResult.ValueOrDie());
  }

  // from_columns (map form - unordered)
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

  // from_columns (ordered pair-vector form — preserves insertion order)
  EagerDataFrame EagerDataFrame::from_columns(
      const std::vector<std::pair<std::string, std::shared_ptr<arrow::Array>>> &cols)
  {
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::ChunkedArray>> arrays;

    for (const auto &[name, arr] : cols)
    {
      fields.push_back(arrow::field(name, arr->type()));
      arrays.push_back(std::make_shared<arrow::ChunkedArray>(arr));
    }

    auto schema = arrow::schema(fields);
    auto table = arrow::Table::Make(schema, arrays);
    return EagerDataFrame(table);
  }

  // writes a csv to the file
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

  // writes to a parquet file
  void EagerDataFrame::write_parquet(const std::string &path) const
  {
    // opening output file
    auto fileResult = arrow::io::FileOutputStream::Open(path);
    if (!fileResult.ok())
      throw std::runtime_error("Could not open file for writing: " + path);

    // writing with default properties
    auto status = parquet::arrow::WriteTable(
        *table_,
        arrow::default_memory_pool(),
        fileResult.ValueOrDie(),
        /*chunk_size=*/1024 // rows per row group
    );
    if (!status.ok())
      throw std::runtime_error("Could not write Parquet: " + status.ToString());
  }

  // for selecting columns from the dataframe
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

  // for printing the dataframe in a tabular format to the console for debugging
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

  // filter operation given a predicate expression
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

  // replaces a column with the result of evaluating an expression, or adds a new column if name doesn't exist
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

  // returns the first n rows of the dataframe as a new dataframe
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

  // sorts the dataframe by the specified columns in ascending or descending order(default is ascending and only one bool taken for all columns)
  EagerDataFrame EagerDataFrame::sort(
      const std::vector<std::string> &colNames,
      bool ascending) const
  {
    int64_t n = table_->num_rows();

    // Pre-extract sort key values into typed vectors so the comparator performs
    // only simple array index lookups — no variant construction or Arrow API
    // calls during the O(N log N) comparison phase.
    struct SortKey
    {
      bool isString = false;
      std::vector<double> numVals; // all numeric types cast to double
      std::vector<std::string> strVals;
      std::vector<bool> nulls;

      int compare(int64_t a, int64_t b) const
      {
        bool na = nulls[a], nb = nulls[b];
        if (na && nb)
          return 0;
        if (na)
          return -1;
        if (nb)
          return 1;
        if (!isString)
          return numVals[a] < numVals[b] ? -1 : numVals[a] > numVals[b] ? 1
                                                                        : 0;
        return strVals[a] < strVals[b] ? -1 : strVals[a] > strVals[b] ? 1
                                                                      : 0;
      }
    };

    std::vector<SortKey> sortKeys;
    for (const auto &name : colNames)
    {
      auto col = table_->GetColumnByName(name);
      if (!col)
        throw std::runtime_error("Column not found: " + name);
      auto arr = flatten(col);
      auto type = fromArrowType(table_->schema()->GetFieldByName(name)->type());

      SortKey sk;
      sk.isString = (type == DataType::STRING);
      sk.nulls.resize(n);

      if (sk.isString)
      {
        auto typed = std::static_pointer_cast<arrow::StringArray>(arr);
        sk.strVals.resize(n);
        for (int64_t i = 0; i < n; i++)
        {
          sk.nulls[i] = typed->IsNull(i);
          if (!sk.nulls[i])
            sk.strVals[i] = typed->GetString(i);
        }
      }
      else
      {
        sk.numVals.resize(n);
        for (int64_t i = 0; i < n; i++)
        {
          sk.nulls[i] = arr->IsNull(i);
          if (!sk.nulls[i])
          {
            switch (type)
            {
            case DataType::INT32:
              sk.numVals[i] = std::static_pointer_cast<arrow::Int32Array>(arr)->Value(i);
              break;
            case DataType::INT64:
              sk.numVals[i] = static_cast<double>(std::static_pointer_cast<arrow::Int64Array>(arr)->Value(i));
              break;
            case DataType::FLOAT32:
              sk.numVals[i] = std::static_pointer_cast<arrow::FloatArray>(arr)->Value(i);
              break;
            case DataType::FLOAT64:
              sk.numVals[i] = std::static_pointer_cast<arrow::DoubleArray>(arr)->Value(i);
              break;
            default:
              break;
            }
          }
        }
      }
      sortKeys.push_back(std::move(sk));
    }

    // build index array
    std::vector<int64_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);

    auto comparator = [&sortKeys, ascending](int64_t rowA, int64_t rowB) -> bool
    {
      for (const auto &key : sortKeys)
      {
        int cmp = key.compare(rowA, rowB);
        if (cmp != 0)
          return ascending ? cmp < 0 : cmp > 0;
      }
      return rowA < rowB; // stable: preserve original order on full tie
    };

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

  // Build a compact binary key — raw bytes avoid decimal-string conversion overhead
  static std::string buildJoinKey(
      const std::vector<std::shared_ptr<arrow::Array>> &cols,
      const std::vector<DataType> &types,
      int64_t row)
  {
    std::string key;
    for (size_t i = 0; i < cols.size(); i++)
    {
      if (cols[i]->IsNull(row))
      {
        key += '\xff'; // null sentinel byte
      }
      else
      {
        auto val = extractRowValue(cols[i], row, types[i]);
        std::visit([&key](const auto &v)
                   {
                     using T = std::decay_t<decltype(v)>;
                     if constexpr (std::is_same_v<T, std::string>)
                       key += v;
                     else
                       key.append(reinterpret_cast<const char *>(&v), sizeof(v)); },
                   val);
      }
      key += '\0'; // column delimiter
    }
    return key;
  }

  // Build hash map from a table's join key columns — key to row indices
  static std::unordered_map<std::string, std::vector<int64_t>> buildJoinHashMap(
      const std::shared_ptr<arrow::Table> &table,
      const std::vector<std::string> &onColumns)
  {
    std::vector<std::shared_ptr<arrow::Array>> flatCols;
    std::vector<DataType> colTypes;
    for (const auto &colName : onColumns)
    {
      auto col = table->GetColumnByName(colName);
      if (!col)
        throw std::runtime_error("Join column not found: " + colName);
      flatCols.push_back(flatten(col));
      colTypes.push_back(fromArrowType(table->schema()->GetFieldByName(colName)->type()));
    }

    std::unordered_map<std::string, std::vector<int64_t>> map;
    map.reserve(table->num_rows());
    for (int64_t i = 0; i < table->num_rows(); i++)
      map[buildJoinKey(flatCols, colTypes, i)].push_back(i);
    return map;
  }

  EagerDataFrame EagerDataFrame::join(const EagerDataFrame &other,
                                      const std::vector<std::string> &onColumns, const std::string &how) const
  {
    if (how != "inner" && how != "left" && how != "right" && how != "outer")
      throw std::runtime_error("Unsupported join type: " + how);

    // Always hash the smaller table to reduce memory and improve probe efficiency
    bool buildIsLeft = (table_->num_rows() <= other.table()->num_rows());
    const EagerDataFrame &buildDF = buildIsLeft ? *this : other;
    const EagerDataFrame &probeDF = buildIsLeft ? other : *this;

    auto hashMap = buildJoinHashMap(buildDF.table(), onColumns);

    // Flatten probe table's join columns upfront
    std::vector<std::shared_ptr<arrow::Array>> probeFlatCols;
    std::vector<DataType> probeColTypes;
    for (const auto &colName : onColumns)
    {
      auto col = probeDF.table()->GetColumnByName(colName);
      if (!col)
        throw std::runtime_error("Join column not found: " + colName);
      probeFlatCols.push_back(flatten(col));
      probeColTypes.push_back(fromArrowType(
          probeDF.table()->schema()->GetFieldByName(colName)->type()));
    }

    // Probe phase: collect matched (build, probe) index pairs
    std::vector<int64_t> buildIndices, probeIndices;
    buildIndices.reserve(std::min(buildDF.table()->num_rows(), probeDF.table()->num_rows()));
    probeIndices.reserve(buildIndices.capacity());

    std::vector<bool> buildMatched(buildDF.table()->num_rows(), false);
    std::vector<bool> probeMatched(probeDF.table()->num_rows(), false);

    for (int64_t j = 0; j < probeDF.table()->num_rows(); j++)
    {
      auto key = buildJoinKey(probeFlatCols, probeColTypes, j);
      auto it = hashMap.find(key);
      if (it != hashMap.end())
      {
        for (int64_t i : it->second)
        {
          buildIndices.push_back(i);
          probeIndices.push_back(j);
          buildMatched[i] = true;
          probeMatched[j] = true;
        }
      }
    }

    // Translate (build, probe) to (left, right) based on which side was hashed
    std::vector<int64_t> leftIndices, rightIndices;
    if (buildIsLeft)
    {
      leftIndices = std::move(buildIndices);
      rightIndices = std::move(probeIndices);
    }
    else
    {
      leftIndices = std::move(probeIndices);
      rightIndices = std::move(buildIndices);
    }

    // leftMatched[i] = true if row i of *this was part of a match
    const std::vector<bool> &leftMatched = buildIsLeft ? buildMatched : probeMatched;
    // rightMatched[j] = true if row j of other was part of a match
    const std::vector<bool> &rightMatched = buildIsLeft ? probeMatched : buildMatched;

    if (how == "left" || how == "outer")
    {
      for (int64_t i = 0; i < table_->num_rows(); i++)
      {
        if (!leftMatched[i])
        {
          leftIndices.push_back(i);
          rightIndices.push_back(-1);
        }
      }
    }
    if (how == "right" || how == "outer")
    {
      for (int64_t j = 0; j < other.table()->num_rows(); j++)
      {
        if (!rightMatched[j])
        {
          leftIndices.push_back(-1);
          rightIndices.push_back(j);
        }
      }
    }

    auto coalesceJoinKey = [&](const std::shared_ptr<arrow::Array> &leftArr,
                               const std::shared_ptr<arrow::Array> &rightArr,
                               DataType type) -> std::shared_ptr<arrow::ChunkedArray>
    {
      std::shared_ptr<arrow::Array> out;
      switch (type)
      {
      case DataType::INT32:
      {
        auto l = std::static_pointer_cast<arrow::Int32Array>(leftArr);
        auto r = std::static_pointer_cast<arrow::Int32Array>(rightArr);
        arrow::Int32Builder b;
        for (size_t k = 0; k < leftIndices.size(); k++)
        {
          int64_t li = leftIndices[k], ri = rightIndices[k];
          if (li >= 0)
          {
            if (l->IsNull(li))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(l->Value(li)));
          }
          else if (ri >= 0)
          {
            if (r->IsNull(ri))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(r->Value(ri)));
          }
          else
            DF_ARROW_THROW_NOT_OK(b.AppendNull());
        }
        DF_ARROW_THROW_NOT_OK(b.Finish(&out));
        break;
      }
      case DataType::INT64:
      {
        auto l = std::static_pointer_cast<arrow::Int64Array>(leftArr);
        auto r = std::static_pointer_cast<arrow::Int64Array>(rightArr);
        arrow::Int64Builder b;
        for (size_t k = 0; k < leftIndices.size(); k++)
        {
          int64_t li = leftIndices[k], ri = rightIndices[k];
          if (li >= 0)
          {
            if (l->IsNull(li))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(l->Value(li)));
          }
          else if (ri >= 0)
          {
            if (r->IsNull(ri))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(r->Value(ri)));
          }
          else
            DF_ARROW_THROW_NOT_OK(b.AppendNull());
        }
        DF_ARROW_THROW_NOT_OK(b.Finish(&out));
        break;
      }
      case DataType::FLOAT32:
      {
        auto l = std::static_pointer_cast<arrow::FloatArray>(leftArr);
        auto r = std::static_pointer_cast<arrow::FloatArray>(rightArr);
        arrow::FloatBuilder b;
        for (size_t k = 0; k < leftIndices.size(); k++)
        {
          int64_t li = leftIndices[k], ri = rightIndices[k];
          if (li >= 0)
          {
            if (l->IsNull(li))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(l->Value(li)));
          }
          else if (ri >= 0)
          {
            if (r->IsNull(ri))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(r->Value(ri)));
          }
          else
            DF_ARROW_THROW_NOT_OK(b.AppendNull());
        }
        DF_ARROW_THROW_NOT_OK(b.Finish(&out));
        break;
      }
      case DataType::FLOAT64:
      {
        auto l = std::static_pointer_cast<arrow::DoubleArray>(leftArr);
        auto r = std::static_pointer_cast<arrow::DoubleArray>(rightArr);
        arrow::DoubleBuilder b;
        for (size_t k = 0; k < leftIndices.size(); k++)
        {
          int64_t li = leftIndices[k], ri = rightIndices[k];
          if (li >= 0)
          {
            if (l->IsNull(li))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(l->Value(li)));
          }
          else if (ri >= 0)
          {
            if (r->IsNull(ri))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(r->Value(ri)));
          }
          else
            DF_ARROW_THROW_NOT_OK(b.AppendNull());
        }
        DF_ARROW_THROW_NOT_OK(b.Finish(&out));
        break;
      }
      case DataType::STRING:
      {
        auto l = std::static_pointer_cast<arrow::StringArray>(leftArr);
        auto r = std::static_pointer_cast<arrow::StringArray>(rightArr);
        arrow::StringBuilder b;
        for (size_t k = 0; k < leftIndices.size(); k++)
        {
          int64_t li = leftIndices[k], ri = rightIndices[k];
          if (li >= 0)
          {
            if (l->IsNull(li))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(std::string(l->Value(li))));
          }
          else if (ri >= 0)
          {
            if (r->IsNull(ri))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(std::string(r->Value(ri))));
          }
          else
            DF_ARROW_THROW_NOT_OK(b.AppendNull());
        }
        DF_ARROW_THROW_NOT_OK(b.Finish(&out));
        break;
      }
      case DataType::BOOLEAN:
      {
        auto l = std::static_pointer_cast<arrow::BooleanArray>(leftArr);
        auto r = std::static_pointer_cast<arrow::BooleanArray>(rightArr);
        arrow::BooleanBuilder b;
        for (size_t k = 0; k < leftIndices.size(); k++)
        {
          int64_t li = leftIndices[k], ri = rightIndices[k];
          if (li >= 0)
          {
            if (l->IsNull(li))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(l->Value(li)));
          }
          else if (ri >= 0)
          {
            if (r->IsNull(ri))
              DF_ARROW_THROW_NOT_OK(b.AppendNull());
            else
              DF_ARROW_THROW_NOT_OK(b.Append(r->Value(ri)));
          }
          else
            DF_ARROW_THROW_NOT_OK(b.AppendNull());
        }
        DF_ARROW_THROW_NOT_OK(b.Finish(&out));
        break;
      }
      }
      return toChunked(out);
    };

    std::vector<std::shared_ptr<arrow::ChunkedArray>> leftArrays, rightArrays;
    std::vector<std::shared_ptr<arrow::Field>> allFields;

    for (int col = 0; col < table_->num_columns(); col++)
    {
      auto fieldName = table_->schema()->field(col)->name();
      auto colType = fromArrowType(table_->schema()->field(col)->type());
      if (std::find(onColumns.begin(), onColumns.end(), fieldName) != onColumns.end())
      {
        auto leftKey = flatten(table_->column(col));
        auto rightKeyChunked = other.table()->GetColumnByName(fieldName);
        if (!rightKeyChunked)
          throw std::runtime_error("Join column not found: " + fieldName);
        auto rightKey = flatten(rightKeyChunked);
        leftArrays.push_back(coalesceJoinKey(leftKey, rightKey, colType));
      }
      else
      {
        leftArrays.push_back(reorderByIndices(flatten(table_->column(col)), leftIndices, colType));
      }
      allFields.push_back(table_->schema()->field(col));
    }
    for (int col = 0; col < other.table()->num_columns(); col++)
    {
      auto fieldName = other.table()->schema()->field(col)->name();
      if (std::find(onColumns.begin(), onColumns.end(), fieldName) != onColumns.end())
        continue; // skip join key — already present from left table
      auto colType = fromArrowType(other.table()->schema()->field(col)->type());
      rightArrays.push_back(reorderByIndices(flatten(other.table()->column(col)), rightIndices, colType));
      std::string outName = fieldName;
      if (table_->schema()->GetFieldByName(fieldName))
        outName += "_right";
      allFields.push_back(arrow::field(outName, other.table()->schema()->field(col)->type()));
    }

    std::vector<std::shared_ptr<arrow::ChunkedArray>> allArrays;
    allArrays.insert(allArrays.end(), leftArrays.begin(), leftArrays.end());
    allArrays.insert(allArrays.end(), rightArrays.begin(), rightArrays.end());

    auto newSchema = arrow::schema(allFields);
    auto newTable = arrow::Table::Make(newSchema, allArrays);
    return EagerDataFrame(newTable);
  }

  // for grouping the dataframe by one or more columns and applying aggregate functions to the groups
  GroupByObj EagerDataFrame::group_by(const std::vector<std::string> &colNames) const
  {
    for (const auto &k : colNames)
      if (!table_->GetColumnByName(k))
        return GroupByObj(colNames, {}, {});

    // Pre-flatten key columns once — avoids re-fetching inside the row loop
    std::vector<std::shared_ptr<arrow::Array>> keyArrs;
    std::vector<DataType> keyTypes;
    for (const auto &k : colNames)
    {
      keyArrs.push_back(flatten(table_->GetColumnByName(k)));
      keyTypes.push_back(fromArrowType(table_->schema()->GetFieldByName(k)->type()));
    }

    // Pre-flatten agg columns and record their types
    std::vector<std::string> aggCols;
    std::map<std::string, DataType> colTypes;
    std::vector<std::shared_ptr<arrow::Array>> aggArrs;
    std::vector<DataType> aggTypes;
    for (int i = 0; i < table_->num_columns(); i++)
    {
      const std::string &name = table_->schema()->field(i)->name();
      if (std::find(colNames.begin(), colNames.end(), name) != colNames.end())
        continue;
      auto type = fromArrowType(table_->schema()->field(i)->type());
      if (isNumeric(type))
      {
        aggCols.push_back(name);
        colTypes[name] = type;
        aggArrs.push_back(flatten(table_->column(i)));
        aggTypes.push_back(type);
      }
    }

    // Build a compact binary key for one row — same approach as join hash map.
    // Using raw bytes avoids GetScalar to ToString heap allocations per row.
    auto appendKey = [](std::string &key,
                        const std::shared_ptr<arrow::Array> &arr,
                        int64_t row, DataType type)
    {
      if (arr->IsNull(row))
      {
        key += '\xff';
        key += '\0';
        return;
      }
      switch (type)
      {
      case DataType::STRING:
      {
        auto v = std::static_pointer_cast<arrow::StringArray>(arr)->GetView(row);
        key.append(v.data(), v.size());
        break;
      }
      case DataType::INT32:
      {
        auto v = std::static_pointer_cast<arrow::Int32Array>(arr)->Value(row);
        key.append(reinterpret_cast<const char *>(&v), sizeof(v));
        break;
      }
      case DataType::INT64:
      {
        auto v = std::static_pointer_cast<arrow::Int64Array>(arr)->Value(row);
        key.append(reinterpret_cast<const char *>(&v), sizeof(v));
        break;
      }
      case DataType::FLOAT32:
      {
        auto v = std::static_pointer_cast<arrow::FloatArray>(arr)->Value(row);
        key.append(reinterpret_cast<const char *>(&v), sizeof(v));
        break;
      }
      case DataType::FLOAT64:
      {
        auto v = std::static_pointer_cast<arrow::DoubleArray>(arr)->Value(row);
        key.append(reinterpret_cast<const char *>(&v), sizeof(v));
        break;
      }
      default:
        key += '\xff';
        break;
      }
      key += '\0';
    };

    // String representation of a key value — used only once per new group
    auto keyToStr = [](const std::shared_ptr<arrow::Array> &arr,
                       int64_t row, DataType type) -> std::string
    {
      if (arr->IsNull(row))
        return "null";
      switch (type)
      {
      case DataType::STRING:
      {
        auto v = std::static_pointer_cast<arrow::StringArray>(arr)->GetView(row);
        return std::string(v.data(), v.size());
      }
      case DataType::INT32:
        return std::to_string(std::static_pointer_cast<arrow::Int32Array>(arr)->Value(row));
      case DataType::INT64:
        return std::to_string(std::static_pointer_cast<arrow::Int64Array>(arr)->Value(row));
      case DataType::FLOAT32:
        return std::to_string(std::static_pointer_cast<arrow::FloatArray>(arr)->Value(row));
      case DataType::FLOAT64:
        return std::to_string(std::static_pointer_cast<arrow::DoubleArray>(arr)->Value(row));
      default:
        return "null";
      }
    };

    // Accumulate using unordered_map for O(1) average lookup instead of O(log N)
    struct GroupEntry
    {
      std::vector<std::string> keyStrs;   // output representation — built once per group
      std::vector<GroupAccumulator> accs; // indexed parallel to aggCols
    };
    std::unordered_map<std::string, GroupEntry> fastMap;
    fastMap.reserve(1024);

    std::string binaryKey;
    binaryKey.reserve(colNames.size() * 16);

    for (int64_t row = 0; row < table_->num_rows(); row++)
    {
      binaryKey.clear();
      for (size_t ki = 0; ki < keyArrs.size(); ki++)
        appendKey(binaryKey, keyArrs[ki], row, keyTypes[ki]);

      auto &entry = fastMap[binaryKey];
      if (entry.accs.empty()) // first time seeing this group
      {
        for (size_t ki = 0; ki < keyArrs.size(); ki++)
          entry.keyStrs.push_back(keyToStr(keyArrs[ki], row, keyTypes[ki]));
        entry.accs.resize(aggCols.size());
      }

      for (size_t ai = 0; ai < aggArrs.size(); ai++)
        if (!aggArrs[ai]->IsNull(row))
          entry.accs[ai].update(extractAsDouble(aggArrs[ai], row, aggTypes[ai]));
    }

    // Convert to map<vector<string>, ...> for GroupByObj (K groups — fast)
    std::map<std::vector<std::string>, std::map<std::string, GroupAccumulator>> groupMap;
    for (auto &[binKey, entry] : fastMap)
    {
      auto &dst = groupMap[entry.keyStrs];
      for (size_t ai = 0; ai < aggCols.size(); ai++)
        dst[aggCols[ai]] = std::move(entry.accs[ai]);
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
      for (double v : vals)
        DF_ARROW_THROW_NOT_OK(b.Append(static_cast<int64_t>(v)));
      DF_ARROW_THROW_NOT_OK(b.Finish(&arr));
      return arr;
    }
    switch (type)
    {
    case DataType::INT32:
    {
      arrow::Int32Builder b;
      for (double v : vals)
        DF_ARROW_THROW_NOT_OK(b.Append(static_cast<int32_t>(v)));
      DF_ARROW_THROW_NOT_OK(b.Finish(&arr));
      return arr;
    }
    case DataType::INT64:
    {
      arrow::Int64Builder b;
      for (double v : vals)
        DF_ARROW_THROW_NOT_OK(b.Append(static_cast<int64_t>(v)));
      DF_ARROW_THROW_NOT_OK(b.Finish(&arr));
      return arr;
    }
    case DataType::FLOAT32:
    {
      arrow::FloatBuilder b;
      for (double v : vals)
        DF_ARROW_THROW_NOT_OK(b.Append(static_cast<float>(v)));
      DF_ARROW_THROW_NOT_OK(b.Finish(&arr));
      return arr;
    }
    default:
    {
      arrow::DoubleBuilder b;
      for (double v : vals)
        DF_ARROW_THROW_NOT_OK(b.Append(v));
      DF_ARROW_THROW_NOT_OK(b.Finish(&arr));
      return arr;
    }
    }
  }

  static std::shared_ptr<arrow::DataType> aggOutputArrowType(DataType type, const std::string &op)
  {
    if (op == "count")
      return arrow::int64();
    switch (type)
    {
    case DataType::INT32:
      return arrow::int32();
    case DataType::INT64:
      return arrow::int64();
    case DataType::FLOAT32:
      return arrow::float32();
    default:
      return arrow::float64();
    }
  }

  // to aggregate based on the groupings and accumulator results
  EagerDataFrame GroupByObj::aggregate(const std::vector<std::pair<std::string, std::string>> &aggList) const
  {
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::ChunkedArray>> arrays;

    for (size_t ki = 0; ki < groupKeys_.size(); ki++)
    {
      arrow::StringBuilder builder;
      for (const auto &[key, _] : groups_)
        DF_ARROW_THROW_NOT_OK(builder.Append(key[ki]));
      std::shared_ptr<arrow::Array> arr;
      DF_ARROW_THROW_NOT_OK(builder.Finish(&arr));
      fields.push_back(arrow::field(groupKeys_[ki], arrow::utf8()));
      arrays.push_back(toChunked(arr));
    }

    for (const auto &[colName, op] : aggList)
    {
      DataType colType = DataType::FLOAT64;
      if (auto it = colTypes_.find(colName); it != colTypes_.end())
        colType = it->second;

      std::vector<double> results;
      for (const auto &[key, colAccs] : groups_)
      {
        auto it2 = colAccs.find(colName);
        GroupAccumulator defaultAcc;
        const auto &acc = (it2 != colAccs.end()) ? it2->second : defaultAcc;
        double result = 0.0;
        if (op == "sum")
          result = acc.sum;
        else if (op == "count")
          result = static_cast<double>(acc.count);
        else if (op == "min")
          result = acc.min;
        else if (op == "max")
          result = acc.max;
        else if (op == "mean")
          result = acc.count > 0 ? acc.sum / acc.count : 0.0;
        else
          throw std::runtime_error("Unsupported aggregation operation: " + op);
        results.push_back(result);
      }
      auto arr = buildAggArray(results, colType, op);
      fields.push_back(arrow::field(colName + "_" + op, aggOutputArrowType(colType, op)));
      arrays.push_back(toChunked(arr));
    }

    auto schema = arrow::schema(fields);
    auto table = arrow::Table::Make(schema, arrays);
    return EagerDataFrame(table);
  }
}

#undef DF_ARROW_THROW_NOT_OK