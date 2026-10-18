// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Tool for conversion CSV log file to ULog format

#include <ulog_cpp/simple_writer.hpp>
#include <rapidcsv.h>
#include <vector>
#include <string>
#include <filesystem>

using std::vector;
using std::string;

struct Data {
	uint64_t timestamp;
	float values[30];
};

int main(int argc, char** argv)
{
	if (argc < 2) {
		printf("Usage: %s file.csv [file.ulg]\n", argv[0]);
		return -1;
	}

	// check input file exists
	if (!std::filesystem::exists(argv[1])) {
		printf("Input file \"%s\" does not exist\n", argv[1]);
		return -1;
	}

	// open csv file
	rapidcsv::Document csv(argv[1]);
	auto columns = csv.GetColumnNames();


	// open ulog file
	string ulog_file;
	if (argc < 3) {
		ulog_file = std::filesystem::path(argv[1]).replace_extension(".ulg").string();
	} else {
		ulog_file = argv[2];
	}
	ulog_cpp::SimpleWriter writer(ulog_file.c_str(), 0);
	writer.writeInfo("sys_name", "flix");

	vector<ulog_cpp::Field> fields;
	fields.push_back(ulog_cpp::Field("uint64_t", "timestamp"));
	columns.erase(columns.begin()); // remove timestamp column
	for (auto& column : columns) {
		// Valid field name for ULog: [a-z0-9_]+
		std::replace(column.begin(), column.end(), '.', '_'); // replace dots with underscores
		std::transform(column.begin(), column.end(), column.begin(), [](unsigned char c) { return std::tolower(c); }); // lowercase column name
		fields.push_back(ulog_cpp::Field("float", column));
	}

	const char* msg_name = "state";
	writer.writeMessageFormat(msg_name, fields);
	writer.headerComplete();

	const uint16_t msg_id = writer.writeAddLoggedMessage(msg_name);

	for (size_t i = 0; i < csv.GetRowCount(); i++) {
		Data data;
		data.timestamp = csv.GetCell<float>(0, i) * 1000000.0;
		for (size_t j = 1; j <= columns.size(); j++) {
			data.values[j - 1] = csv.GetCell<float>(j, i);
		}
		writer.writeData(msg_id, data);
	}
}
