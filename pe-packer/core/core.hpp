#pragma once
#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>
#include <winnt.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdint>
#include <string>
#include <ctime>
#include "../pe_lib/pe_bliss.h"
#include "../asmjit/asmjit.h"
#include "../handler/handler.hpp"
#include "../utils/arguments.hpp"

class c_core
{
public:
	c_core(std::string input_file, std::string output_file, std::uint32_t mutations_counter);

	asmjit::x86::Assembler* get_assembler() {
		return m_assembler.get();
	}

	pe_bliss::pe_base* get_peImage() {
		return m_peImage.get();
	}

	asmjit::CodeHolder* get_codeHolder() {
		return m_codeHolder.get();
	}

	struct xor_target_t {
		std::uintptr_t func_start;
		std::uint32_t func_end;
		std::uint8_t xor_key;
	};

	std::vector<xor_target_t> obf_xor_targets;

	void xor_function_range(xor_target_t xor_target);

	void insert_runtime_xor_stub(xor_target_t xor_target);

	void xor_sections(std::string sec_to_xor);

	void process();

	void simple_jump_obfuscation();
	void call_obfuscation();

	void generate_junk_code();

	void push_pop_junk();
	void big_conditions_junk();

	void obfuscation_process();

	asmjit::x86::Gp get_rand_reg();

	asmjit::x86::Gp get_rand_lower_reg();

	bool obf_call_oep = false;
	bool obf_anti_disasm = false;
	bool obf_mba = false;
	bool obf_xor_sections = false;
	bool obf_fake_instr = false;
	bool obf_func_pack = false;

	std::uint32_t m_mutations;
	std::string m_input;
	std::string m_output;

private:
	std::unique_ptr<asmjit::x86::Assembler> m_assembler;
	std::unique_ptr<pe_bliss::pe_base> m_peImage;
	std::unique_ptr<asmjit::CodeHolder> m_codeHolder;

}; extern c_core* mutator;

