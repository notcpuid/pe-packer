#pragma once

#include "core.hpp"

class c_adasm {
public:

	c_adasm(c_core& g_core);
	void jmp_label_skip();
private:
	c_core& m_core;
};