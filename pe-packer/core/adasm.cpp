#include "adasm.hpp"
#include "../utils/utils.hpp"

using namespace asmjit;

c_adasm::c_adasm(c_core& g_core) : m_core(g_core) {}

void c_adasm::jmp_label_skip() {
	Label skip_cc = m_core.get_assembler()->newLabel();
	m_core.get_assembler()->jz(skip_cc);
	m_core.get_assembler()->jnz(skip_cc);
	m_core.get_assembler()->db(0xE9);
	if (m_core.obf_fake_instr) {
		for (int i = 0; i < random_value(0x1, 0x100); ++i) {
			m_core.get_assembler()->db(random_value(0x10, 0xFF));
		}
	}
	m_core.get_assembler()->bind(skip_cc);
}