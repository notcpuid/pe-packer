#pragma once
#include <string>

namespace arguments {
	extern int argc;
	extern char** argv;

	void init(int v_argc, char** v_argv);
	bool has(const char* flag);
	const char* get(const char* flag);
	std::string get_after(const char* key, int offset = 0);
}
