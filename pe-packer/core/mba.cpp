#include "mba.hpp"
#include "../utils/utils.hpp"

using namespace asmjit;

c_mba::c_mba(c_core& g_core) : m_core(g_core){}

void c_mba::gen_math_operations() {
	switch (rand() % 4) {
	case 0:
		m_core.get_assembler()->shr(m_core.get_rand_reg(), random_value(1, 100));
		break;

	case 1:
		m_core.get_assembler()->and_(m_core.get_rand_reg(), random_value(1, 100));
		break;

	case 2:
		m_core.get_assembler()->xor_(m_core.get_rand_reg(), random_value(1, 100));
		break;

	case 3:
		m_core.get_assembler()->add(m_core.get_rand_reg(), random_value(1, 100));
		break;

	default:
		break;
	}


}

void c_mba::mba_code(c_mba::options opt) {

	int x = random_value(0, 3);
	switch (x) {

	case 0: {

		Label new_label = m_core.get_assembler()->newLabel();
		gen_math_operations();

		// create new jump equal to label
		m_core.get_assembler()->je(new_label);

		// load x and y into regs
		m_core.get_assembler()->mov(x86::eax, x86::edi);
		m_core.get_assembler()->mov(x86::ebx, x86::esi);

		// calculate in eax: (X | Y)
		// store result in stack
		m_core.get_assembler()->or_(x86::eax, x86::ebx); // eax = X | Y
		m_core.get_assembler()->push(x86::eax);

		// calculate in eax: (X & Y)
		m_core.get_assembler()->mov(x86::eax, x86::edi);
		m_core.get_assembler()->and_(x86::eax, x86::ebx);

		// get (X | Y) from stack and substraction (X & Y) 
		// store result in ecx
		m_core.get_assembler()->pop(x86::ecx);
		m_core.get_assembler()->sub(x86::ecx, x86::eax);

		m_core.get_assembler()->mov(x86::eax, x86::ecx);

		// store result in stack and manipulate it
		m_core.get_assembler()->push(x86::eax);
		m_core.get_assembler()->mov(x86::ebx, x86::eax);
		m_core.get_assembler()->xor_(x86::ebx, x86::edi);

		// its loc
		m_core.get_assembler()->bind(new_label);

		// store base pointer and push new from stack
		m_core.get_assembler()->push(x86::rbp);
		m_core.get_assembler()->mov(x86::rbp, x86::rsp);
		gen_math_operations();

		// restore base pointer
		m_core.get_assembler()->pop(x86::rbp);

		break;
	}

	case 1: {

		Label new_label = m_core.get_assembler()->newLabel();

		gen_math_operations();

		// create new jump equal to label
		m_core.get_assembler()->je(new_label);

		// load x and y into regs
		m_core.get_assembler()->mov(x86::eax, x86::edi);
		m_core.get_assembler()->mov(x86::ebx, x86::esi);

		// calculate in eax: (X & Y)
		// store result in stack
		m_core.get_assembler()->and_(x86::eax, x86::ebx);
		m_core.get_assembler()->push(x86::eax);

		// calculate in eax: (X | Y)
		m_core.get_assembler()->mov(x86::eax, x86::edi);
		m_core.get_assembler()->or_(x86::eax, x86::ebx);

		// get (X & Y) from stack and addition (X | Y)
		// store result in ecx
		m_core.get_assembler()->pop(x86::ecx);
		m_core.get_assembler()->add(x86::ecx, x86::eax);

		m_core.get_assembler()->mov(x86::eax, x86::ecx);

		// store result in stack and manipulate it
		m_core.get_assembler()->push(x86::eax);
		m_core.get_assembler()->mov(x86::ebx, x86::eax);
		m_core.get_assembler()->xor_(x86::ebx, x86::edi);

		// its loc
		m_core.get_assembler()->bind(new_label);

		// store base pointer and push new from stack
		m_core.get_assembler()->push(x86::rbp);
		m_core.get_assembler()->mov(x86::rbp, x86::rsp);
		gen_math_operations();

		// restore base pointer
		m_core.get_assembler()->pop(x86::rbp);

		break;
	}

	case 2: {
		Label new_label = m_core.get_assembler()->newLabel();

		// create new jump equal to label
		m_core.get_assembler()->je(new_label);

		// load x and y into regs
		m_core.get_assembler()->mov(x86::eax, x86::edi);
		m_core.get_assembler()->mov(x86::ebx, x86::esi);

		// calculate in eax: (X & Y)
		// store result in stack
		m_core.get_assembler()->xor_(x86::eax, x86::ebx);
		m_core.get_assembler()->neg(x86::eax);
		m_core.get_assembler()->push(x86::eax);

		// calculate in eax: (X | Y)
		m_core.get_assembler()->mov(x86::eax, x86::edi);
		m_core.get_assembler()->neg(x86::eax);
		m_core.get_assembler()->and_(x86::eax, x86::ebx);

		// get (X & Y) from stack and addition (X | Y)
		// store result in ecx
		m_core.get_assembler()->pop(x86::ecx);
		m_core.get_assembler()->add(x86::ecx, x86::eax);

		m_core.get_assembler()->mov(x86::eax, x86::ecx);

		// store result in stack and manipulate it
		m_core.get_assembler()->push(x86::eax);
		m_core.get_assembler()->mov(x86::ebx, x86::eax);
		m_core.get_assembler()->xor_(x86::ebx, x86::edi);

		// its loc
		m_core.get_assembler()->bind(new_label);

		// store base pointer and push new from stack
		m_core.get_assembler()->push(x86::rbp);
		m_core.get_assembler()->mov(x86::rbp, x86::rsp);
		gen_math_operations();

		// restore base pointer
		m_core.get_assembler()->pop(x86::rbp);

		break;
	}
	}
}