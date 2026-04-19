// Minimal Arrow CSV example: read CSV into arrow::Table and print schema.
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <arrow/api.h>
#include <arrow/csv/api.h>
#include <arrow/io/api.h>

int main()
{
	const std::string csv_path = "sample.csv";

	{
		std::ofstream out(csv_path);
		if (!out)
		{
			std::cerr << "Failed to create CSV file: " << csv_path << '\n';
			return 1;
		}
		out << "id,name,score\n";
		out << "1,Ada,98\n";
		out << "2,Bob,87\n";
	}

	auto maybe_input = arrow::io::ReadableFile::Open(csv_path);
	if (!maybe_input.ok())
	{
		std::cerr << "Failed to open CSV: " << maybe_input.status().ToString() << '\n';
		return 1;
	}
	std::shared_ptr<arrow::io::InputStream> input = *maybe_input;

	auto maybe_reader = arrow::csv::TableReader::Make(
		arrow::io::IOContext(arrow::default_memory_pool()), input,
		arrow::csv::ReadOptions::Defaults(),
		arrow::csv::ParseOptions::Defaults(), arrow::csv::ConvertOptions::Defaults());
	if (!maybe_reader.ok())
	{
		std::cerr << "Failed to create CSV reader: " << maybe_reader.status().ToString() << '\n';
		return 1;
	}
	std::shared_ptr<arrow::csv::TableReader> reader = *maybe_reader;
	auto maybe_table = reader->Read();
	if (!maybe_table.ok())
	{
		std::cerr << "Failed to read CSV into table: " << maybe_table.status().ToString() << '\n';
		return 1;
	}
	std::shared_ptr<arrow::Table> table = *maybe_table;

	std::cout << "Schema:\n"
			  << table->schema()->ToString() << '\n';
	return 0;
}
