#pragma once

#include "core.hpp"

class c_mba {
public:

	struct options {
		int mba_factor = 10;
	};

	c_mba(c_core& g_core);
	
	void gen_math_operations();
	void mba_code(c_mba::options opt);

private:
	c_core& m_core;
};