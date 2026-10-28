#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace leaf::build {

std::string read(const std::filesystem::path& path);
void write(const std::filesystem::path& path, const std::string& contents);
std::string literal(std::string_view input);
int embed_command(int argc, char** argv);

}
