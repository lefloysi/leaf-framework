#include "embed.hpp"

#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) try {
	return leaf::build::embed_command(argc, argv);
}
catch (const std::exception& error) {
	std::cerr << error.what() << '\n';
	return 1;
}
