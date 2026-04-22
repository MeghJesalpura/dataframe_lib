#include "../include/dataframelib/EagerDataFrame.h"
#include <arrow/csv/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <arrow/csv/writer.h>
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

  // converting table to batches and write each one
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