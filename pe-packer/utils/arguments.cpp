#include "arguments.hpp"
#include <cstring>

int arguments::argc = 0;
char** arguments::argv = nullptr;

void arguments::init(int v_argc, char** v_argv) {
	argc = v_argc;
	argv = v_argv;
}

bool arguments::has(const char* flag) {
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], flag) == 0)
			return true;
	}
	return false;
}

const char* arguments::get(const char* flag) {
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], flag) == 0 && i + 1 < argc)
			return argv[i + 1];
	}
	return nullptr;
}

std::string arguments::get_after(const char* key, int offset) {
	for (int i = 0; i < argc; ++i) {
		if (strcmp(argv[i], key) == 0) {
			int index = i + 1 + offset;
			if (index < argc && argv[index][0] != '-') {
				return std::string(argv[index]);
			}
			else {
				return "";
			}
		}
	}
	return "";
}
