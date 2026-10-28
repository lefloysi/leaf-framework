#include "embed.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace leaf::build {

std::string read(const std::filesystem::path& path) {
	std::ifstream input(path, std::ios::binary);
	if (!input) {
		throw std::runtime_error("Cannot read " + path.string());
	}
	return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
}

void write(const std::filesystem::path& path, const std::string& contents) {
	std::filesystem::create_directories(path.parent_path());
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output.write(contents.data(), static_cast<std::streamsize>(contents.size()))) {
		throw std::runtime_error("Cannot write " + path.string());
	}
}

std::string literal(std::string_view input) {
	std::string result = "\"";
	for (const unsigned char value : input) {
		switch (value) {
		case '\\': result += "\\\\"; break;
		case '"': result += "\\\""; break;
		case '\n': result += "\\n\"\n\""; break;
		case '\r': result += "\\r"; break;
		case '\t': result += "\\t"; break;
		default: result += static_cast<char>(value); break;
		}
	}
	return result + "\"";
}

int embed_command(int argc, char** argv) {
	if (argc < 2) {
		throw std::runtime_error("Expected embed-text, embed-bytes, or embed-program.");
	}
	const std::string_view command = argv[1];
	if (command == "embed-text" && argc == 6) {
		write(argv[3], "#pragma once\nnamespace " + std::string(argv[4]) + " {\ninline constexpr char " + argv[5] + "[] =\n" + literal(read(argv[2])) + ";\n}\n");
		return 0;
	}
	if ((command == "embed-bytes" && argc == 5) || (command == "embed-program" && argc == 7)) {
		const auto bytes = read(argv[2]);
		std::ostringstream output;
		if (command == "embed-program") {
			output << "#include <" << argv[5] << ".hpp>\n";
		}
		const std::string symbol = argv[4];
		const auto data = command == "embed-program" ? symbol + "_data" : symbol;
		output << "static const unsigned char " << data << "[] = {\n";
		for (std::size_t index = 0; index < bytes.size(); ++index) {
			output << static_cast<unsigned int>(static_cast<unsigned char>(bytes[index])) << ',';
			if (index % 32 == 31) {
				output << '\n';
			}
		}
		output << "\n};\n";
		if (command == "embed-program") {
			output << "extern \"C\" const " << argv[6] << ' ' << symbol << " = { " << data << ", sizeof(" << data << ") };\n";
		}
		write(argv[3], output.str());
		return 0;
	}
	throw std::runtime_error("Invalid embed arguments for " + std::string(command));
}

}
