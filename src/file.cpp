#include <iostream>
#include <fstream>
#include <vector>
#include <string_view>

bool check_elf_linkage(std::string_view filepath) {
	std::ifstream file(filepath.data(), std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "cannot open file!\n";
		return false;
	}

	std::vector<char> header(64);
	if (!file.read(header.data(), 64)) {
		std::cerr << "failed to read file reader\n";
		return false;
	}

	if (header[0] != 0x7F || header[1] != 'E' || header[2] != 'L' || header[3] != 'F') {
		std::cerr << "not a valid ELF binary\n";
		return false;
	}

	char class_type = header[4];
	if (class_type != 2) {
		std::cerr << "only 64-bit ELF binaries are supported\n";
		return false;
	}
	unsigned long long phoff = *reinterpret_cast<unsigned long long*>(&header[32]);
	unsigned short phnum = *reinterpret_cast<unsigned short*>(&header[56]);
	file.seekg(phoff);
	for (int i = 0; i < phnum; ++i) {
		std::vector<char> ph(56);
		if (!file.read(ph.data(), 56)) break;

		unsigned int p_type = *reinterpret_cast<unsigned int*>(&ph[0]);
		if (p_type == 3) {
			std::cout << filepath << ": dynamically linked\n";
			return true;
		}
	}

	std::cout << filepath << ": statically linked\n";
	return true;
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "usage: " << argv[0] << " <binary_path>\n";
		return 1;
	}
	if (!check_elf_linkage(argv[1])) {
		return 1;
	}

	return 0;
} 





		
















