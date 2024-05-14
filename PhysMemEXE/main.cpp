#ifndef KDLIBMODE

#include <Windows.h>
#include <string>
#include <vector>
#include <filesystem>

#include "physmem_setup/physmem.hpp"

int wmain(const int argc, wchar_t** argv) 
{
	bool status;
	physmem mem = physmem_setup::setup(&status);
	if (!status)
	{
		return -1;
	}

	auto e = mem.attach(L"PhysMemEXE.exe");
	std::cout << std::hex << "KPROC: " << e.kprocess << std::endl << "BASE: " << e.base << std::endl << "DTB: " << e.directory_table << std::endl << "PID: " << e.pid << std::endl;

	auto dtb = mem.bruteforce_dtb_from_base(e.base);
	std::cout << std::hex << "DTB (Bruteforced): " << dtb << std::endl;

	short s;
	mem.read_virtual_memory(e.base, (byte*) & s, sizeof(s));
	std::cout << s << std::endl;

	system("pause");
	return 0;
}

#endif