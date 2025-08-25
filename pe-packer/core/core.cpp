#include "core.hpp"
#include "../utils/utils.hpp"
#include "../asmjit/src/asmjit/arm/a64operand.h"
#include "mba.hpp"
#include "adasm.hpp"

using namespace asmjit;

c_core::c_core(std::string input_file, std::string output_file, std::uint32_t mutations_counter)
{
	m_input = input_file;
	m_output = output_file;
	m_mutations = mutations_counter;

	std::ifstream pe_file(m_input, std::ios::in | std::ios::binary);
	if (!pe_file) {
		print_error("Binary is not PE file\n");
	}

	m_peImage = std::make_unique<pe_bliss::pe_base>(pe_bliss::pe_factory::create_pe(pe_file));
	if (m_peImage->get_pe_type() != pe_bliss::pe_type_32) {
		print_error("Binary is not x86 architecture\n");
	}

	bool clr_dir = m_peImage->directory_exists(14);
	if (clr_dir) {
		print_error("CLR directory found, .NET binary is not supported yet\n");
	}

	JitRuntime jitRt;
	m_codeHolder = std::make_unique<CodeHolder>();
	Error init_asmjit = m_codeHolder->init(jitRt.environment(), jitRt.cpuFeatures());

	if (init_asmjit != kErrorOk) {
		print_error("Failed initialization\n");
	}

	print_custom("asmjit", "Successfully asmjit initialization\n");

	xor_target_t xor_target;

	auto dll_char = m_peImage->get_dll_characteristics();

	if (arguments::has("-noaslr")) {
		if (dll_char & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) {
			dll_char &= ~IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;

			m_peImage->set_dll_characteristics(dll_char);
			print_info("ASLR flag has been removed\n");
		} else
			print_warning("ASLR flag not find yet\n");
	}

	if (arguments::has("-oep_call")) {
		if (dll_char & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) {
			print_info("OEP call obfuscation cannot be enabled because PE file has a ASLR flag\n");
		}
		else {
			print_info("OEP call obfuscation is enable\n");
			obf_call_oep = true;
		}
	}

	if (arguments::has("-adasm")) {
		print_info("Anti-disassembly obfuscation is enabled\n");
		obf_anti_disasm = true;
	}

	if (arguments::has("-mba")) {
		print_info("Mixed Boolean Arithmetic obfuscation is enabled\n");
		obf_mba = true;
	}

	if (arguments::has("-senc")) {
		print_info("Sections encryption is enabled\n");
		obf_xor_sections = true;
	}

	if (arguments::has("-finstr")) {
		print_info("Fake instructions is enabled\n");
		obf_fake_instr = true;
	}

	if (arguments::has("-fpack")) {
		std::string addr_start = arguments::get_after("-fpack", 0);
		std::string addr_end = arguments::get_after("-fpack", 1);
		std::uint8_t key = random_value(0x10, 0xFF);

		if (!addr_start.empty() && !addr_end.empty()) {

			print_info("Packing functions from %s to %s enabled\n", addr_start.c_str(), addr_end.c_str());

			obf_func_pack = true;

			xor_target.func_start = std::stoul(addr_start, nullptr, 16);
			xor_target.func_end = std::stoul(addr_end, nullptr, 16);

			obf_xor_targets.push_back({ xor_target.func_start, xor_target.func_end, key });
		}
		else {
			print_warning("Argument -fpack must be followed by two addresses [START_ADDR] [END_ADDR]\n");
		}
	}

	m_assembler = std::make_unique<x86::Assembler>(m_codeHolder.get());
}

void c_core::xor_function_range(xor_target_t xor_target)
{
	if (!obf_func_pack || xor_target.func_start >= xor_target.func_end)
		return;

	std::uint32_t func_size = static_cast<std::uint32_t>(xor_target.func_end - xor_target.func_start);
	std::uint8_t key = xor_target.xor_key;


	for (auto& sec : m_peImage->get_image_sections()) {
		std::uintptr_t sec_base = m_peImage->get_image_base_32() + sec.get_virtual_address();
		std::uintptr_t sec_end = sec_base + sec.get_raw_data().size();

		if (xor_target.func_start >= sec_base && xor_target.func_end <= sec_end) {
			std::size_t offset = xor_target.func_start - sec_base;
			std::string& data = sec.get_raw_data();
			for (std::size_t i = 0; i < func_size; ++i)
				data[offset + i] ^= key;

			break;
		}
	}
}

void c_core::insert_runtime_xor_stub(xor_target_t xor_target)
{
	if (!obf_func_pack)
		return;

	x86::Gp reg_base = x86::ecx;
	x86::Gp reg_counter = x86::ebx;
	x86::Gp reg_key = x86::al;

	m_assembler->mov(reg_base, xor_target.func_start);     // ecx = start address
	m_assembler->mov(reg_counter, xor_target.func_end - xor_target.func_start);   // ebx = size
	m_assembler->mov(reg_key, xor_target.xor_key);         // al  = key

	Label loop_start = m_assembler->newLabel();
	Label loop_end = m_assembler->newLabel();

	m_assembler->bind(loop_start);
	m_assembler->cmp(reg_counter, 0);
	m_assembler->jz(loop_end);
	m_assembler->xor_(x86::byte_ptr(reg_base), reg_key);
	m_assembler->inc(reg_base);
	m_assembler->dec(reg_counter);
	m_assembler->jmp(loop_start);
	m_assembler->bind(loop_end);

	print_info("Stub for func decryption has been inserted\n");
}

void c_core::xor_sections(std::string sec_to_xor)
{
	if (!obf_func_pack) {
		return;
	}

	std::string* reloc_data = nullptr;

	pe_bliss::section* reloc_secptr = nullptr;

	for (auto& sec : m_peImage->get_image_sections()) {
		if (sec.get_name() == sec_to_xor) {
			reloc_secptr = &sec;
			reloc_data = &sec.get_raw_data();
			break;
		}
	}

	std::uint8_t reloc_xor_key = static_cast<std::uint8_t>(random_value(1, 255));

	if (reloc_data && !reloc_data->empty()) {
		for (char& byte : *reloc_data) {
			byte ^= reloc_xor_key;
		}
		print_info("Section %s has been encrypted with key 0x%02x\n", sec_to_xor.c_str(), reloc_xor_key);
	}

	if (reloc_secptr) {
		uint32_t reloc_va = reloc_secptr->get_virtual_address() + m_peImage->get_image_base_32();
		uint32_t reloc_size = static_cast<uint32_t>(reloc_secptr->get_raw_data().size());

		x86::Gp reg_base = x86::ecx;
		x86::Gp reg_counter = x86::ebx;
		x86::Gp reg_key = x86::al;

		m_assembler->mov(reg_base, reloc_va);      // ecx = RVA(.reloc)
		m_assembler->mov(reg_counter, reloc_size); // ebx = size
		m_assembler->mov(reg_key, reloc_xor_key);  // al  = key

		Label loop_start = m_assembler->newLabel();
		Label loop_end = m_assembler->newLabel();

		m_assembler->bind(loop_start);
		m_assembler->cmp(reg_counter, 0);
		m_assembler->jz(loop_end);
		m_assembler->xor_(x86::byte_ptr(reg_base), reg_key); // ecx ^= key
		m_assembler->inc(reg_base);
		m_assembler->dec(reg_counter);
		m_assembler->jmp(loop_start);
		m_assembler->bind(loop_end);

		print_info("Stub for %s section decryption has been inserted\n", sec_to_xor.c_str());
	}
}

void c_core::process()
{
	uint32_t ep_addr = m_assembler->offset();
	uint32_t idx_oep = random_value(0x1000, 0xFFFFFFFF);
	uint32_t image_base = m_peImage->get_image_base_32();

	pe_bliss::section new_section;
	const char* section_name = ".ptext";

	m_assembler->push(x86::ebp);
	m_assembler->mov(x86::ebp, x86::esp); // set base pointer to stack pointer
	
	new_section.set_name(section_name);
	new_section.readable(true).writeable(false).executable(true);
	size_t codeSize = m_assembler->bufferCapacity();
	if (codeSize == 0) {
		print_error("Empty code section");
	}

	new_section.get_raw_data().resize(codeSize);

	pe_bliss::section& pe_section = m_peImage->add_section(new_section);
	m_codeHolder->_baseAddress = pe_section.get_virtual_address();
	std::uint32_t oep = obf_call_oep ? m_peImage->get_ep() + m_peImage->get_image_base_32() : m_peImage->get_ep();
	std::uint32_t oepvl_xor_key = random_value(128, 1024);
	Label new_label = m_assembler->newLabel();

	c_adasm adasm_obj(*this);

	obfuscation_process();

	if (obf_func_pack) {
		for (auto& sec : m_peImage->get_image_sections())
		{
			if (sec.get_name() == ".text")
			{
				sec.set_characteristics(0x40000000 | 0x80000000 | 0x20000000); // RWX
				print_info("Section %s flags changed to RWX\n", sec.get_name().c_str());
				break;
			}
		}

		for (const xor_target_t& target : obf_xor_targets) {
			xor_function_range(target);
			insert_runtime_xor_stub(target);
		}
	}

	if (obf_anti_disasm)
		adasm_obj.jmp_label_skip();

	if (obf_call_oep) {
		switch (rand() % 2) {
		case 0:

			oep -= idx_oep;
			m_assembler->mov(x86::edx, x86::esp);
			m_assembler->mov(x86::eax, oep);
			m_assembler->add(x86::eax, idx_oep);
			m_assembler->mov(x86::esp, x86::edx);
			//m_assembler->db(0xCC); 

			m_assembler->jmp(x86::eax);

			if (obf_anti_disasm)
				adasm_obj.jmp_label_skip();

			if (obf_fake_instr) {
				for (int i = 0; i < random_value(0x1, 0x400); ++i) {
					m_assembler->db(random_value(0x10, 0xFF));
				}
			}
			break;
		case 1:

			/*
			* calculate new oep address given the original offset value
			* xor original offset value with random xor key
			*/

			if (obf_anti_disasm)
				adasm_obj.jmp_label_skip();

			oep -= idx_oep;
			idx_oep ^= oepvl_xor_key;

			//m_assembler->mov(x86::eax, oep);
			// store stack pointer value
			m_assembler->mov(x86::edx, x86::esp);

			// store oep value into stack pointer
			m_assembler->mov(x86::esp, oep);

			// move xored value to eax
			m_assembler->mov(x86::eax, idx_oep);

			// dexor offset
			m_assembler->xor_(x86::eax, oepvl_xor_key);

			// build oep address restore our stack and call it
			m_assembler->add(x86::eax, x86::esp);
			m_assembler->mov(x86::esp, x86::edx);

			m_assembler->jmp(x86::eax);

			if (obf_fake_instr) {
				for (int i = 0; i < random_value(0x1, 0x400); ++i) {
					m_assembler->db(random_value(0x10, 0xFF));
				}
			}

			if (obf_anti_disasm)
				adasm_obj.jmp_label_skip();

			//m_assembler->db(0xCC); 
			//m_assembler->jmp(x86::eax);
			break;
		default:
			break;
		}
	}
	else {

		m_assembler->jmp(oep);

		if (obf_fake_instr) {
			for (int i = 0; i < random_value(0x1, 0x400); ++i) {
				m_assembler->db(random_value(0x10, 0xFF));
			}
		}
	}

	std::vector<std::string> section_to_xor = {
		".reloc"
	};

	for (unsigned int i = 0; i < section_to_xor.size(); i++)
	{
		xor_sections(section_to_xor[i]);
	}

	print_info("Address of entry point 0x%x\n", (unsigned int)pe_section.get_virtual_address() + image_base + ep_addr); 
	print_info("New section characteristics 0x%x\n", pe_section.get_characteristics());

	pe_section.set_raw_data(
		std::string(reinterpret_cast<char*>(m_assembler->bufferData()), m_assembler->offset())
	);

	pe_section.get_raw_data().resize(m_assembler->bufferCapacity());
	pe_section.set_virtual_size(m_assembler->offset());

	m_peImage->set_ep(pe_section.get_virtual_address() + ep_addr);
	pe_bliss::import_rebuilder_settings settings(true, false);

	std::ofstream patch_file(m_output, std::ios::out | std::ios::binary | std::ios::trunc);
	pe_bliss::rebuild_pe(*m_peImage, patch_file);
	patch_file.close();

	print_info("File successfully packed and saved in %s", m_output.c_str());

	return;
}

void c_core::simple_jump_obfuscation()
{
	int jmp_labes = (rand() % 10);
	for (int i = 0; i < jmp_labes; i++)
	{
		int first_value = random_value(0x10, 0x100);
		int second_value = random_value(0x10, 0x100);
		int third_value = 0;

		if (first_value < second_value)
		{
			third_value = second_value - first_value;
		} 
		else if (first_value > second_value)
		{
			third_value = first_value - second_value;
		}

		Label label = m_assembler->newLabel();
		auto rand_reg = get_rand_reg();
		m_assembler->xor_(rand_reg, random_value(0x10, 0x100));
		m_assembler->mov(x86::eax, first_value);
		m_assembler->mov(x86::ebx, second_value);
		m_assembler->add(x86::eax, third_value);
		
		m_assembler->cmp(x86::eax, x86::ebx);
		switch (rand() % 4)
		{
		case 0:
			m_assembler->jz(label);
			break;
		case 1:
			m_assembler->jnz(label);
			break;
		case 2:
			m_assembler->jecxz(label);
			break;
		case 3:
			m_assembler->jg(label);
			break;
		}
		int junk_bytes = rand() % 100;
		for (int j = 0; j < junk_bytes; j++)
			generate_junk_code();
		m_assembler->bind(label);
		generate_junk_code();
	}
}

void c_core::call_obfuscation()
{
	int call_deep = (rand() % 10) + 1;
	for (int i = 0; i < call_deep; i++)
	{
		Label call_label = m_assembler->newLabel();
		generate_junk_code();
		m_assembler->call(call_label);
		generate_junk_code();
		m_assembler->bind(call_label);
	}
}

void c_core::generate_junk_code()
{
	int junk_type = 2;
	switch (junk_type)
	{
	case 0:
		push_pop_junk();
		break;
	case 1:
		big_conditions_junk();
		break;
	default:
		break;
	}
}

void c_core::push_pop_junk()
{
	int push_pop_count = (rand() % 10) * m_mutations;
	for (int i = 0; i < push_pop_count; i++)
	{
		Operand src;

		if (rand() % 2) {
			src = get_rand_reg(); // x86::gp type
		}
		else {
			src = Imm(rand() % 100); // imm type
		}

		m_assembler->push(get_rand_reg());
		switch (rand() % 3)
		{
		case 0:
			if (src.isReg()) {
				m_assembler->add(get_rand_reg(), src.as<x86::Gp>());
			}
			else if (src.isImm()) {
				m_assembler->add(get_rand_reg(), src.as<Imm>());
			}
			break;
		case 1:
			if (src.isReg()) {
				m_assembler->imul(get_rand_reg(), src.as<x86::Gp>());
			}
			else if (src.isImm()) {
				m_assembler->imul(get_rand_reg(), src.as<Imm>());
			}
			break;
		case 2:
			if (src.isReg()) {
				m_assembler->sub(get_rand_reg(), src.as<x86::Gp>());
			}
			else if (src.isImm()) {
				m_assembler->sub(get_rand_reg(), src.as<Imm>());
			}
			break;
		default:
			m_assembler->nop();
		}
		m_assembler->pop(get_rand_reg());

		int junk_subnodes_count = rand() % 100;
		for (int n = 0; n < junk_subnodes_count; n++)
		{
			int rn = rand() % 3;
			switch (rn)
			{

			case 0:
				m_assembler->imul(get_rand_reg(), rand() % 100);
				m_assembler->imul(get_rand_reg(), rand() % 100);
				m_assembler->add(get_rand_reg(), rand() % 100);
				m_assembler->cpuid();
				m_assembler->nop();
				m_assembler->cpuid();
				m_assembler->push(get_rand_reg());
				m_assembler->pop(get_rand_reg());
				break;

			case 1:
				m_assembler->imul(get_rand_reg(), rand() % 100);
				m_assembler->add(get_rand_reg(), rand() % 100);
				m_assembler->push(get_rand_reg());
				m_assembler->add(get_rand_reg(), rand() % 100);
				m_assembler->cpuid();
				m_assembler->nop();
				m_assembler->nop();
				m_assembler->pop(get_rand_reg());

				break;

			case 2:
				m_assembler->sub(get_rand_reg(), rand() % 100);
				m_assembler->add(get_rand_reg(), rand() % 100);
				m_assembler->add(get_rand_reg(), rand() % 100);
				m_assembler->push(get_rand_reg());
				m_assembler->nop();
				m_assembler->nop();
				m_assembler->cpuid();
				m_assembler->pop(get_rand_reg());
				break;

			default:
				m_assembler->nop();
			}
		}
	}
}

void c_core::big_conditions_junk()
{
	int cond_count = (rand() % 100) + 1;
	for (int i = 0; i < cond_count; i++)
	{
		auto reg1 = get_rand_reg();
		auto reg2 = get_rand_reg();
		int actions_count = rand() % 100;
		for (int j = 0; j < actions_count; j++)
		{
			switch (rand() % 5)
			{
			case 0:
				m_assembler->xor_(reg1, reg2);
				break;
			case 1:
				m_assembler->add(reg1, rand() % 10);
				break;
			case 2:
				m_assembler->imul(reg2, rand() % 100);
				break;
			case 3:
				m_assembler->sub(reg1, rand() % 100);
			case 4:
				m_assembler->mov(reg1, reg2);
				break;
			default:
				break;
			}
		}
		m_assembler->cmp(reg1, reg2);
		Label cmp_label = m_assembler->newLabel();
		switch (rand() % 7)
		{
		case 0:
			m_assembler->jmp(cmp_label);
			break;
		case 1:
			m_assembler->jz(cmp_label);
			break;
		case 2:
			m_assembler->jnz(cmp_label);
			break;
		case 3:
			m_assembler->jb(cmp_label);
			break;
		case 4:
			m_assembler->jbe(cmp_label);
			break;
		case 5:
			m_assembler->jl(cmp_label);
			break;
		case 6:
			m_assembler->jle(cmp_label);
			break;
		default:
			m_assembler->jmp(cmp_label);
			break;
		}
		m_assembler->bind(cmp_label);
		push_pop_junk();

	}
}

x86::Gp c_core::get_rand_reg()
{
	int reg_num = rand() % 6;
	switch (reg_num)
	{
	case 0:
		return x86::eax;
	case 1:
		return x86::ebx;
	case 2:
		return x86::ecx;
	case 3:
		return x86::edx;
	case 4:
		return x86::esi;
	case 5:
		return x86::edi;
	default:
		return x86::eax;
	}
}

x86::Gp c_core::get_rand_lower_reg() {
	int reg_num = rand() % 6;
	switch (reg_num) {
	case 0:
		return x86::ax;
	case 1:
		return x86::bx;
	case 2:
		return x86::cx;
	case 3:
		return x86::dx;
	case 4:
		return x86::si;
	case 5:
		return x86::di;
	default:
		return x86::ax;
	}
}

void c_core::obfuscation_process()
{
	c_mba mba_obj(*this);
	c_mba::options mba_opt;

	c_adasm adasm_obj(*this);
	for (uint32_t i = 0; i < m_mutations; i++)
	{
		if (obf_anti_disasm) {
			adasm_obj.jmp_label_skip();
		}

		int junk_step = obf_mba ? rand() % 3 : rand() % 2;
		switch (junk_step)
		{
		case 0:
			simple_jump_obfuscation();
			break;
		case 1:
			call_obfuscation();
			break;

		case 2:
			mba_obj.mba_code(mba_opt);
			break;
		default:
			break;
		}
	}
}